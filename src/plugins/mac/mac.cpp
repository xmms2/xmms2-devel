/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team and Ma Xuan
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <mac/All.h>
#include <mac/MACLib.h>
#include <mac/APETag.h>
#include <mac/APEInfo.h>
#include <mac/CharacterHelper.h>

#include "source_adapter.h"

#include "xmms/xmms_log.h"
#include "xmms/xmms_xformplugin.h"

#include <stdlib.h>
#include <stdio.h>


extern "C" {
/*
 * Type Definitions
 */
typedef struct {
	guint start_time;

	IAPEDecompress *p_decompress;

	guint block_align;
	guint sample_rate;
	guint bits_per_sample;
	guint channels;
} xmms_mac_data_t;

typedef enum { STRING, INTEGER } ptype;
typedef struct {
	const gchar *vname;
	const gchar *xname;
	ptype type;
} props;

static const props properties[] = {
	{ "title",                XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,     STRING  },
	{ "artist",               XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,    STRING  },
	{ "album",                XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,     STRING  },
	{ "tracknumber",          XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,   INTEGER },
	{ "date",                 XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,      STRING  },
	{ "year",                 XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,      STRING  },
	{ "genre",                XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,     STRING  },
	{ "comment",              XMMS_MEDIALIB_ENTRY_PROPERTY_COMMENT,   STRING  },
	{ "discnumber",           XMMS_MEDIALIB_ENTRY_PROPERTY_PARTOFSET, INTEGER }
};

/*
 * Function prototypes
 */

static gboolean xmms_mac_plugin_setup (xmms_xform_plugin_t *xform_plugin);

static void xmms_mac_destroy (xmms_xform_t *decoder);
static gboolean xmms_mac_init (xmms_xform_t *decoder);
static gint xmms_mac_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static gint64 xmms_mac_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err);

static void xmms_mac_get_media_info (xmms_xform_t *decoder);

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN_DEFINE ("mac",
                          "Monkey's Audio", XMMS_VERSION,
                          "Monkey's Audio Decoder",
                          xmms_mac_plugin_setup);

static gboolean
xmms_mac_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);

	methods.init = xmms_mac_init;
	methods.destroy = xmms_mac_destroy;
	methods.read = xmms_mac_read;
	methods.seek = xmms_mac_seek;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "audio/x-ape",
	                              XMMS_STREAM_TYPE_PRIORITY,
	                              60,
	                              XMMS_STREAM_TYPE_END);

	xmms_magic_add ("Monkey's Audio Magic", "audio/x-ape",
	                "0 string MAC ", NULL);

	return TRUE;
}

static gboolean
xmms_mac_init (xmms_xform_t *xform)
{
	xmms_mac_data_t *data;
	gint start_block = -1, end_block = -1;
	gint err = 0;
	CAPEInfo *ape_info = NULL;

	XMMS_DBG ("xmms_mac_init");

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_mac_data_t, 1);

	xmms_xform_private_data_set (xform, data);

	CSourceAdapter *source_adapter = new CSourceAdapter (xform);
	ape_info = new CAPEInfo (&err, source_adapter);

	/*
	 * Since we have to use a source adapter, so
	 * using this function to create the decompressor is the only way.
	 */
	data->p_decompress = CreateIAPEDecompressEx2 (ape_info, start_block, end_block, &err);

	data->block_align = data->p_decompress->GetInfo (APE_INFO_BLOCK_ALIGN);
	data->sample_rate = data->p_decompress->GetInfo (APE_INFO_SAMPLE_RATE);
	data->bits_per_sample = data->p_decompress->GetInfo (APE_INFO_BITS_PER_SAMPLE);
	data->channels = data->p_decompress->GetInfo (APE_INFO_CHANNELS);

	xmms_mac_get_media_info (xform);

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             data->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             data->sample_rate,
	                             XMMS_STREAM_TYPE_END);

	return TRUE;
}

static void
xmms_mac_destroy (xmms_xform_t *xform)
{
	xmms_mac_data_t *data;

	XMMS_DBG ("xmms_mac_destroy");
	g_return_if_fail (xform);

	data = (xmms_mac_data_t *)xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	if (data->p_decompress) {
		delete data->p_decompress;
	}

	g_free (data);
}

static void
xmms_mac_get_media_info (xmms_xform_t *xform)
{
	xmms_mac_data_t *data;
	xmms_error_t error;

	XMMS_DBG ("xmms_mac_get_media_info");

	g_return_if_fail (xform);

	data = (xmms_mac_data_t *)xmms_xform_private_data_get (xform);

	xmms_error_reset (&error);

	/* Meta information */

	CAPETag *p_ape_tag = (CAPETag *)(data->p_decompress->GetInfo (APE_INFO_TAG));

	if (p_ape_tag) {
		BOOL bHasID3Tag = p_ape_tag->GetHasID3Tag ();
		BOOL bHasAPETag = p_ape_tag->GetHasAPETag ();

		if (bHasID3Tag || bHasAPETag) {
			CAPETagField * pTagField;
			int index = 0;
			while ((pTagField = p_ape_tag->GetTagField (index)) != NULL) {
				index ++;

				const wchar_t *field_name;
				char field_value[255];

				gchar *name;

				field_name = pTagField->GetFieldName ();

#if MAC_DLL_INTERFACE_VERSION_NUMBER >= 1000
				name = (gchar *)CAPECharacterHelper::GetUTF8FromUTF16 (field_name);
#else
				name = (gchar *)GetUTF8FromUTF16 (field_name);
#endif
				memset (field_value, 0, 255);
				int size = 255;
				p_ape_tag->GetFieldString (field_name, (char *)field_value, &size, TRUE);

				guint i = 0;
				for (i = 0; i < G_N_ELEMENTS (properties); i++) {
					if (g_ascii_strcasecmp (name, properties[i].vname) == 0) {
						if (properties[i].type == INTEGER) {
							gint tmp = strtol (field_value, NULL, 10);
							xmms_xform_metadata_set_int (xform,
														 properties[i].xname,
														 tmp);
						} else {
							xmms_xform_metadata_set_str (xform,
														 properties[i].xname,
														 field_value);
						}
						break;
					}
				}
				if (i >= G_N_ELEMENTS (properties)) {
					xmms_xform_metadata_set_str (xform, name, field_value);
				}
				g_free (name);
			}
		}
	}

	const gchar *name, *value, *metakey;
	gint filesize;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	if (xmms_xform_metadata_get_int (xform, metakey, &filesize)) {
		gint duration = data->p_decompress->GetInfo (APE_DECOMPRESS_LENGTH_MS);
		metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION;
		xmms_xform_metadata_set_int (xform, metakey, duration);
	}

	/* Technical Information */

	/* Compression Level */
	name = "Compression Level";
	switch (data->p_decompress->GetInfo (APE_INFO_COMPRESSION_LEVEL)) {
	case COMPRESSION_LEVEL_FAST:
		value = "Fast";
		break;
	case COMPRESSION_LEVEL_NORMAL:
		value = "Normal";
		break;
	case COMPRESSION_LEVEL_HIGH:
		value = "High";
		break;
	case COMPRESSION_LEVEL_EXTRA_HIGH:
		value = "Extra High";
		break;
	case COMPRESSION_LEVEL_INSANE:
		value = "Insane";
		break;
	}
	xmms_xform_metadata_set_str (xform, name, value);

	/* Format Flags */
	name = "Flags";
	xmms_xform_metadata_set_int (xform, name, data->p_decompress->GetInfo (APE_INFO_FORMAT_FLAGS));

	/* Average Bitrate */
	/* mac library returns bits per millisecond(!), thus '*1000' */
	xmms_xform_metadata_set_int (xform,
	                             XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
	                             data->p_decompress->GetInfo (APE_INFO_AVERAGE_BITRATE) * 1000);
}

static gint
xmms_mac_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err)
{
	xmms_mac_data_t *data;

	int blocks_to_read = 0, actrual_read = 0;
	int nRetVal = 0;

	data = (xmms_mac_data_t *)xmms_xform_private_data_get (xform);

	blocks_to_read = len / data->block_align;

	nRetVal = data->p_decompress->GetData ((gchar *)buf, blocks_to_read, &actrual_read);

	return actrual_read * data->block_align;
}

static gint64
xmms_mac_seek (xmms_xform_t *xform, gint64 samples, xmms_xform_seek_mode_t whence, xmms_error_t *err)
{
	xmms_mac_data_t *data;
	gint64 blocks;

	g_return_val_if_fail (xform, FALSE);

	data = (xmms_mac_data_t *)xmms_xform_private_data_get (xform);
	switch (whence) {
	case XMMS_XFORM_SEEK_CUR:
		blocks = data->p_decompress->GetInfo (APE_DECOMPRESS_CURRENT_BLOCK);
		blocks += samples;
		break;
	case XMMS_XFORM_SEEK_SET:
		blocks = samples;
		break;
	case XMMS_XFORM_SEEK_END:
		blocks = data->p_decompress->GetInfo (APE_DECOMPRESS_TOTAL_BLOCKS);
		blocks += samples;
		break;
	}
	data->p_decompress->Seek (blocks);

	return blocks;
}

}
