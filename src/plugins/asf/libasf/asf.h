/*  libasf - An Advanced Systems Format media file parser
 *  Copyright (C) 2006-2007 Juho Vähä-Herttua
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ASF_H
#define ASF_H


/* used int types for different platforms */
#if defined (_WIN32) && !defined (__MINGW_H)
typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef long int32_t;
typedef unsigned long uint32_t;

typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif


struct asf_stream_s {
	int32_t (*read)(void *opaque, void *buffer, int32_t size);
	int32_t (*write)(void *opaque, void *buffer, int32_t size);
	int64_t (*seek)(void *opaque, int64_t offset);
	void *opaque;
};
typedef struct asf_stream_s asf_stream_t;

struct asf_metadata_entry_s {
	char *key;
	char *value;
};
typedef struct asf_metadata_entry_s asf_metadata_entry_t;

struct asf_metadata_s {
	char *title;
	char *artist;
	char *copyright;
	char *description;
	char *rating;
	uint16_t extended_count;
	asf_metadata_entry_t *extended;
};
typedef struct asf_metadata_s asf_metadata_t;

struct asf_payload_s {
	uint8_t stream_number;
	uint32_t media_object_number;
	uint32_t media_object_offset;

	uint32_t replicated_length;
	uint8_t *replicated_data;

	uint32_t datalen;
	uint8_t *data;

	uint32_t pts;
};
typedef struct asf_payload_s asf_payload_t;

struct asf_packet_s {
	uint8_t ec_length;
	uint8_t *ec_data;
	uint8_t ec_data_size;

	uint32_t length;
	uint32_t padding_length;
	uint32_t send_time;
	uint16_t duration;

	uint16_t payload_count;
	asf_payload_t *payloads;
	uint16_t payloads_size;

	uint32_t datalen;
	uint8_t *payload_data;
	uint32_t payload_data_size;
};
typedef struct asf_packet_s asf_packet_t;

enum asf_stream_type_e {
	ASF_STREAM_TYPE_NONE     = 0x00,
	ASF_STREAM_TYPE_AUDIO    = 0x01,
	ASF_STREAM_TYPE_VIDEO    = 0x02,
	ASF_STREAM_TYPE_COMMAND  = 0x04,
	ASF_STREAM_TYPE_UNKNOWN  = 0xff
};
typedef enum asf_stream_type_e asf_stream_type_t;

struct asf_waveformatex_s {
	uint16_t codec_id;
	uint16_t channels;
	uint32_t rate;
	uint32_t bitrate;
	uint16_t blockalign;
	uint16_t bitspersample;
	uint16_t datalen;
	uint8_t *data;
};
typedef struct asf_waveformatex_s asf_waveformatex_t;

struct asf_bitmapinfoheader_s {
	uint32_t data_size;
	uint32_t width;
	uint32_t height;
	uint16_t reserved;
	uint16_t bpp;
	uint32_t codec;
	uint32_t image_size;
	uint32_t hppm;
	uint32_t vppm;
	uint32_t colors;
	uint32_t important_colors;
	uint8_t *data;
};
typedef struct asf_bitmapinfoheader_s asf_bitmapinfoheader_t;

struct asf_stream_properties_s {
	asf_stream_type_t type;
	void *properties;
};
typedef struct asf_stream_properties_s asf_stream_properties_t;

typedef struct asf_file_s asf_file_t;

enum asf_error_e {
	ASF_ERROR_INTERNAL       = -1,  /* incorrect input to API calls */
	ASF_ERROR_OUTOFMEM       = -2,  /* some malloc inside program failed */
	ASF_ERROR_EOF            = -3,  /* unexpected end of file */
	ASF_ERROR_IO             = -4,  /* error reading or writing to file */
	ASF_ERROR_INVALID_LENGTH = -5,  /* length value conflict in input data */
	ASF_ERROR_INVALID_VALUE  = -6,  /* other value conflict in input data */
	ASF_ERROR_INVALID_OBJECT = -7,  /* ASF object missing or in wrong place */
	ASF_ERROR_OBJECT_SIZE    = -8,  /* invalid ASF object size (too small) */
	ASF_ERROR_SEEKABLE       = -9,  /* file not seekable */
	ASF_ERROR_SEEK           = -10  /* file is seekable but seeking failed */
};

asf_file_t *asf_open_file(const char *filename);
asf_file_t *asf_open_cb(asf_stream_t *stream);
void asf_close(asf_file_t *file);

int asf_init(asf_file_t *file);
asf_packet_t *asf_packet_create();
int asf_get_packet(asf_file_t *file, asf_packet_t *packet);
int64_t asf_seek_to_msec(asf_file_t *file, int64_t msec);
void asf_free_packet(asf_packet_t *packet);

asf_metadata_t *asf_get_metadata(asf_file_t *file);
void asf_free_metadata(asf_metadata_t *metadata);

uint8_t asf_get_stream_count(asf_file_t *file);
asf_stream_properties_t *asf_get_stream_properties(asf_file_t *file, uint8_t track);

uint64_t asf_get_file_size(asf_file_t *file);
uint64_t asf_get_creation_date(asf_file_t *file);
uint64_t asf_get_data_packets(asf_file_t *file);
uint64_t asf_get_duration(asf_file_t *file);
uint32_t asf_get_max_bitrate(asf_file_t *file);

#endif
