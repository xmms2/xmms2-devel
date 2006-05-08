/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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




/*
 * tar plugin
 * 
 * Written by Anders Gustafsson - andersg@0x63.nu
 *
 */


#include "xmms/plugin.h"
#include "xmms/decoder.h"
#include "xmms/util.h"
#include "xmms/output.h"
#include "xmms/transport.h"
#include "xmms/magic.h"
#include "xmms/xmms.h"

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <tar.h>

/*
 * Type definitions
 */

typedef struct xmms_tar_decoder_data_St {
	xmms_decoder_t *subdecoder;	
} xmms_tar_decoder_data_t;

typedef struct xmms_tar_transport_data_St {
	xmms_transport_t *parenttransport; 
	guint startpos;
	guint length;
	guint relpos;
} xmms_tar_transport_data_t;

/* see tar.h for more information about the tar-format */
union tar_record {
	gchar data[512];
	struct {
		gchar name[100];
		gchar mode[8];
		gchar uid[8];
		gchar gid[8];
		gchar size[12];
		gchar mtime[12];
		gchar chksum[8];
		gchar linkflag;
		gchar linkname[100];
		gchar magic[8];
		gchar uname[32];
		gchar gname[32];
		gchar devmajor[8];
		gchar devminor[8];
		gchar prefix[155];
	} header;
};
#define RECORDSIZE sizeof(union tar_record)


/*
 * Function prototypes
 */

static gboolean xmms_tar_can_handle (const gchar *mimetype);
static gboolean xmms_tar_new (xmms_decoder_t *decoder, const gchar *mimetype);
static gboolean xmms_tar_decode_block (xmms_decoder_t *decoder);
static void xmms_tar_destroy (xmms_decoder_t *decoder);

static gboolean xmms_tar_open (xmms_transport_t *transport, const gchar *uri);
static void xmms_tar_close (xmms_transport_t *transport);
static gint xmms_tar_read (xmms_transport_t *transport, gchar *buffer, guint len);
static gint xmms_tar_size (xmms_transport_t *transport);
static gint xmms_tar_seek (xmms_transport_t *transport, guint offset, gint whence);

/*
 * static variables
 */

static xmms_plugin_t *transport_plugin=NULL;

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *decoder_plugin;

	decoder_plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_DECODER, "tardec",
					  "TAR decoder " XMMS_VERSION,
					  "tar mekk mekk");

	transport_plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "tartransp",
					    "TAR transport " XMMS_VERSION,
					    "tar mekk mekk");


	xmms_plugin_info_add (decoder_plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (decoder_plugin, "Author", "XMMS Team");
	xmms_plugin_info_add (transport_plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (transport_plugin, "Author", "XMMS Team");


	xmms_plugin_method_add (decoder_plugin,
				XMMS_PLUGIN_METHOD_CAN_HANDLE, xmms_tar_can_handle);
	xmms_plugin_method_add (decoder_plugin,
				XMMS_PLUGIN_METHOD_NEW, xmms_tar_new);
	xmms_plugin_method_add (decoder_plugin,
				XMMS_PLUGIN_METHOD_DECODE_BLOCK, xmms_tar_decode_block);
	xmms_plugin_method_add (decoder_plugin,
				XMMS_PLUGIN_METHOD_DESTROY, xmms_tar_destroy);

	xmms_plugin_properties_add (decoder_plugin, XMMS_PLUGIN_PROPERTY_SUBTUNES);


	xmms_plugin_method_add (transport_plugin,
				XMMS_PLUGIN_METHOD_OPEN, xmms_tar_open);
	xmms_plugin_method_add (transport_plugin,
				XMMS_PLUGIN_METHOD_CLOSE, xmms_tar_close);
	xmms_plugin_method_add (transport_plugin,
				XMMS_PLUGIN_METHOD_READ, xmms_tar_read);
	xmms_plugin_method_add (transport_plugin,
				XMMS_PLUGIN_METHOD_SIZE, xmms_tar_size);
	xmms_plugin_method_add (transport_plugin,
				XMMS_PLUGIN_METHOD_SEEK, xmms_tar_seek);

	xmms_plugin_properties_add (transport_plugin, XMMS_PLUGIN_PROPERTY_LOCAL);

	return decoder_plugin;
}

static gboolean
xmms_tar_new (xmms_decoder_t *decoder, const gchar *mimetype)
{
	xmms_tar_decoder_data_t *data;

	g_return_val_if_fail (decoder, FALSE);
	g_return_val_if_fail (mimetype, FALSE);

	data = g_new0 (xmms_tar_decoder_data_t, 1);

	/* */

	xmms_decoder_private_data_set (decoder, data);
	
	return TRUE;
}

static void
xmms_tar_destroy (xmms_decoder_t *decoder)
{
	xmms_tar_decoder_data_t *data;

	data = xmms_decoder_private_data_get (decoder);
	g_return_if_fail (data);

	xmms_decoder_destroy(data->subdecoder);

	g_free(data);
}

static gboolean
xmms_tar_can_handle (const gchar *mimetype)
{
	g_return_val_if_fail (mimetype, FALSE);
	
	if ((g_strcasecmp (mimetype, "application/x-tar") == 0))
		return TRUE;

	return FALSE;
}

static gboolean xmms_tar_open (xmms_transport_t *transport, const gchar *uri){
	xmms_tar_transport_data_t *data;
	union tar_record buf;
	gchar lenbuf[13];
	gchar *name = NULL;
	gint ret;
	gint namelen,len,pos=0;

	lenbuf[12]=0;

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, FALSE);
	while(42){
		ret = xmms_transport_read (data->parenttransport, buf.data, 512);
		if (ret != 512) {
			break;
		}
		pos += ret;

		if (buf.header.name[0] == 0) { /* end of archive */
			break;
		}

		memcpy(lenbuf,buf.header.size,12);
		len=strtol(lenbuf,NULL,8);

		/* Handle long filenames */
		if (buf.header.linkflag == 'L' && strcmp (buf.header.name, "././@LongLink") == 0) {
			len++;
			name=g_malloc(len+1);
			ret = xmms_transport_read (data->parenttransport, name, len);
			if (ret != len) {
				XMMS_DBG ("error reading longname");
				break;
			}
			name[len] = 0;

			pos += RECORDSIZE*((int)((len+(RECORDSIZE-1))/RECORDSIZE));
			
			xmms_transport_seek(data->parenttransport, pos, XMMS_TRANSPORT_SEEK_SET);
			continue;
		}

		if (buf.header.linkflag != REGTYPE && buf.header.linkflag != AREGTYPE) {
			continue;
		}

		if (!name) {
			name = g_strndup (buf.header.name, 100);
		}
		
		namelen = strlen (name);

		XMMS_DBG ("found file: %s (%d)", name,len);
		/* matcha fil */
		
		if (strncmp (name, uri, namelen) == 0 && (uri[namelen]==0 || uri[namelen]=='/')) {
			gchar *uribuf = g_strdup (uri);
			gchar *suburibuf = uribuf + namelen;
			if (*suburibuf == '/') {
				*suburibuf++ = 0;
			}
			XMMS_DBG (" uri: %s  -- suburi: %s", uribuf, suburibuf);
			xmms_transport_uri_set (transport, uribuf);
			xmms_transport_suburi_set (transport, g_strdup (suburibuf));
			
			g_free (name);
			data->startpos = pos;
			data->relpos = 0;
			data->length = len;
			return TRUE;
		}
	
		g_free (name);
		name = NULL;
		pos += RECORDSIZE*((int)((len+(RECORDSIZE-1))/RECORDSIZE));
		
		xmms_transport_seek(data->parenttransport, pos, XMMS_TRANSPORT_SEEK_SET);
	}

	xmms_tar_close (transport);
	return FALSE;
	
}

static void
xmms_tar_close (xmms_transport_t *transport)
{
	xmms_tar_transport_data_t *data;
	g_return_if_fail (transport);

	data = xmms_transport_private_data_get (transport);
	g_return_if_fail (data);
	
	xmms_transport_close (data->parenttransport);
	g_free (data);
}


static gint 
xmms_tar_read (xmms_transport_t *transport, gchar *buffer, guint len){
	xmms_tar_transport_data_t *data;
	gint res;

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, FALSE);

	if (data->relpos >= data->length) {
		return -1;
	}
	
	if (data->relpos+len >= data->length) {
		len = data->length - data->relpos;
	}

	res = xmms_transport_read(data->parenttransport, buffer, len);
	if (res > 0) {
		data->relpos += res;
	}

	return res;
}
static gint xmms_tar_size (xmms_transport_t *transport){
	xmms_tar_transport_data_t *data;

	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, FALSE);

	return data->length;
}
static gint xmms_tar_seek (xmms_transport_t *transport, guint offset, gint whence){
	xmms_tar_transport_data_t *data;
	guint pos=-1;


	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	switch (whence) {
		case XMMS_TRANSPORT_SEEK_SET:
			pos = offset;
			break;
		case XMMS_TRANSPORT_SEEK_END:
			pos = data->length + offset;
			break;
		case XMMS_TRANSPORT_SEEK_CUR:
			pos = data->relpos + offset;
			break;
	}

	if (pos < 0 || pos >= data->length) {
		XMMS_DBG(" bad seek: %d %d %d %d", offset, whence, pos, data->length);
		return -1;
	}

	XMMS_DBG(" seeking to pos: %d",pos);
	xmms_transport_seek(data->parenttransport, data->startpos + pos, XMMS_TRANSPORT_SEEK_SET);
	data->relpos = pos;
	return data->relpos;
}

static gboolean
xmms_tar_decode_block (xmms_decoder_t *decoder)
{
	xmms_tar_decoder_data_t *data;
	xmms_decoder_decode_block_method_t decode_block;
	gboolean res;

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	if (!data->subdecoder) {
		xmms_transport_t *thistransport; 
		xmms_tar_transport_data_t *transportdata;
		xmms_output_t *output;
		const gchar *suburi;
		const gchar *mime;

		transportdata = g_new0 (xmms_tar_transport_data_t, 1);
		transportdata->parenttransport = xmms_decoder_transport_get (decoder);
		g_return_val_if_fail (transportdata->parenttransport, FALSE);

		suburi = xmms_transport_suburi_get (transportdata->parenttransport);

		thistransport = xmms_transport_open_plugin (transport_plugin, 
							    suburi, transportdata);
		if (!thistransport) {
			XMMS_DBG ("open failed");
			g_free (transportdata);
			return FALSE;
		}

		mime = xmms_magic_mime_from_file (xmms_transport_uri_get(thistransport));
		if (!mime) {
			XMMS_DBG ("unknown mime in suburi");
			g_free (transportdata);
			return FALSE;
		}
		
		output = xmms_decoder_output_get (decoder);
		g_return_val_if_fail (output, FALSE);

		data->subdecoder = xmms_decoder_new_stacked(output,
							    thistransport,
							    mime);

		g_return_val_if_fail (data->subdecoder, FALSE);

		xmms_object_parent_set (XMMS_OBJECT (data->subdecoder), XMMS_OBJECT (decoder));

	}

	decode_block = xmms_plugin_method_get (xmms_decoder_plugin_get (data->subdecoder), XMMS_PLUGIN_METHOD_DECODE_BLOCK);
	g_return_val_if_fail (decode_block, FALSE);

	res = decode_block (data->subdecoder);

	return res;

}

