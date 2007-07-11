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

#include <stdlib.h>
#include <string.h>

#include "asf.h"
#include "asfint.h"
#include "byteio.h"
#include "utf.h"
#include "header.h"
#include "guid.h"
#include "debug.h"


static void
asf_parse_read_object(asf_object_t *obj, uint8_t *data)
{
	asf_byteio_getGUID(&obj->guid, data);
	obj->type = asf_guid_get_type(&obj->guid);
	obj->size = asf_byteio_getQWLE(data + 16);
	obj->datalen = 0;
	obj->data = NULL;
	obj->next = NULL;

	if (obj->type == GUID_UNKNOWN) {
		debug_printf("unknown object: %x-%x-%x-%02x%02x%02x%02x%02x%02x%02x%02x, %ld bytes",
		             obj->guid.v1, obj->guid.v2, obj->guid.v3, obj->guid.v4[0],
		             obj->guid.v4[1], obj->guid.v4[2], obj->guid.v4[3], obj->guid.v4[4],
		             obj->guid.v4[5], obj->guid.v4[6], obj->guid.v4[7], (long) obj->size);
	}
}

static int
asf_parse_headerext(asf_object_headerext_t *header, uint8_t *buf, uint64_t buflen)
{
	int64_t datalen;
	uint8_t *data;

	if (header->size < 46) {
		/* invalide size for headerext */
		return ASF_ERROR_OBJECT_SIZE;
	}

	asf_byteio_getGUID(&header->reserved1, buf + 24);
	header->reserved2 = asf_byteio_getWLE(buf + 40);
	header->datalen = asf_byteio_getDWLE(buf + 42);

	if (header->datalen != header->size - 46) {
		/* invalid header extension data length value */
		return ASF_ERROR_INVALID_LENGTH;
	}
	header->data = buf + 46;

	debug_printf("parsing header extension subobjects");

	datalen = header->datalen;
	data = header->data;
	while (datalen > 0) {
		asf_object_t *current;

		if (datalen < 24) {
			/* not enough data for reading object */
			break;
		}

		current = malloc(sizeof(asf_object_t));
		if (!current) {
			return ASF_ERROR_OUTOFMEM;
		}

		asf_parse_read_object(current, data);
		if (current->size > datalen || current->size < 24) {
			/* invalid object size */
			break;
		}
		current->data = data + 24;

		/* add to list of subobjects */
		if (!header->first) {
			header->first = current;
			header->last = current;
		} else {
			header->last->next = current;
			header->last = current;
		}

		data += current->size;
		datalen -= current->size;
	}

	if (datalen != 0) {
		/* data size didn't match */
		return ASF_ERROR_INVALID_LENGTH;
	}

	debug_printf("header extension subobjects parsed successfully");

	return header->size;;
}

int
asf_parse_header(asf_file_t *file)
{
	asf_object_header_t *header;
	asf_stream_t *stream;
	uint8_t hdata[30];
	int tmp;

	file->header = NULL;
	stream = &file->stream;

	tmp = asf_byteio_read(hdata, 30, stream);
	if (tmp < 0) {
		return tmp;
	}

	file->header = malloc(sizeof(asf_object_header_t));
	header = file->header;
	if (!header) {
		return ASF_ERROR_OUTOFMEM;
	}

	asf_parse_read_object((asf_object_t *) header, hdata);

	if (header->size < 30) {
		/* invalid size for header object */
		return ASF_ERROR_OBJECT_SIZE;
	}

	header->subobjects = asf_byteio_getDWLE(hdata + 24);
	header->reserved1 = hdata[28];
	header->reserved2 = hdata[29];
	header->ext = NULL;
	header->first = NULL;
	header->last = NULL;

	if (header->subobjects > 0) {
		uint64_t datalen;
		uint8_t *data;
		int i;

		header->datalen = header->size - 30;
		header->data = malloc(header->datalen * sizeof(uint8_t));
		if (!header->data) {
			return ASF_ERROR_OUTOFMEM;
		}

		tmp = asf_byteio_read(header->data, header->datalen, stream);
		if (tmp < 0) {
			return tmp;
		}

		debug_printf("starting to read subobjects");

		datalen = header->datalen;
		data = header->data;
		for (i=0; i<header->subobjects; i++) {
			asf_object_t *current;

			if (datalen < 24) {
				/* not enough data for reading object */
				break;
			}

			current = malloc(sizeof(asf_object_t));
			if (!current) {
				return ASF_ERROR_OUTOFMEM;
			}

			asf_parse_read_object(current, data);
			if (current->size > datalen || current->size < 24) {
				/* invalid object size */
				break;
			}
			if (current->type == GUID_HEADER_EXTENSION) {
				int ret;
				asf_object_headerext_t *headerext;

				/* we handle header extension separately because it has
				 * some subobjects as well */
				current = realloc(current, sizeof(asf_object_headerext_t));
				headerext = (asf_object_headerext_t *) current;
				headerext->first = NULL;
				headerext->last = NULL;
				ret = asf_parse_headerext(headerext, data, datalen);

				if (ret < 0) {
					/* error parsing header extension */
					return ret;
				}					
				header->ext = headerext;
			} else {
				current->data = data + 24;

				/* add to list of subobjects */
				if (!header->first) {
					header->first = current;
					header->last = current;
				} else {
					header->last->next = current;
					header->last = current;
				}
			}

			data += current->size;
			datalen -= current->size;
		}

		if (i != header->subobjects || datalen != 0) {
			/* header data doesn't match given subobject count */
			return ASF_ERROR_INVALID_VALUE;
		}

		debug_printf("%d subobjects read successfully", i);
	}

	tmp = asf_parse_header_validate(file, file->header);
	if (tmp < 0) {
		/* header read ok but doesn't validate correctly */
		return tmp;
	}

	debug_printf("header validated correctly");

	return header->size;
}

int
asf_parse_data(asf_file_t *file)
{
	asf_object_data_t *data;
	asf_stream_t *stream;
	uint8_t ddata[50];
	int tmp;

	file->data = NULL;
	stream = &file->stream;

	tmp = asf_byteio_read(ddata, 50, stream);
	if (tmp < 0) {
		return tmp;
	}

	file->data = malloc(sizeof(asf_object_data_t));
	data = file->data;
	if (!data) {
		return ASF_ERROR_OUTOFMEM;
	}

	asf_parse_read_object((asf_object_t *) data, ddata);

	if (data->size < 50) {
		/* invalid size for data object */
		return ASF_ERROR_OBJECT_SIZE;
	}

	asf_byteio_getGUID(&data->file_id, ddata + 24);
	data->total_data_packets = asf_byteio_getQWLE(ddata + 40);
	data->reserved = asf_byteio_getWLE(ddata + 48);
	data->packets_position = file->position + 50;

	if (!asf_guid_match(&data->file_id, &file->file_id)) {
		return ASF_ERROR_INVALID_VALUE;
	}

	/* if data->total_data_packets is non-zero (not a stream) and
	   the data packets count doesn't match, return error */
	if (data->total_data_packets &&
	    data->total_data_packets != file->data_packets_count) {
		return ASF_ERROR_INVALID_VALUE;
	}

	return 50;
}

int
asf_parse_index(asf_file_t *file)
{
	asf_object_index_t *index;
	asf_stream_t *stream;
	uint8_t idata[56];
	uint64_t entry_data_size;
	uint8_t *entry_data = NULL;
	int tmp, i;

	file->index = NULL;
	stream = &file->stream;

	tmp = asf_byteio_read(idata, 56, stream);
	if (tmp < 0) {
		return tmp;
	}

	file->index = malloc(sizeof(asf_object_index_t));
	index = file->index;
	if (!index) {
		return ASF_ERROR_OUTOFMEM;
	}

	asf_parse_read_object((asf_object_t *) index, idata);
	if (index->type != GUID_INDEX) {
		tmp = index->size;

		free(index);
		file->index = NULL;

		/* The guid type was wrong, just return the bytes to skip now */
		return tmp;
	}

	if (index->size < 56) {
		/* invalid size for index object */
		return ASF_ERROR_OBJECT_SIZE;
	}

	asf_byteio_getGUID(&index->file_id, idata + 24);
	index->entry_time_interval = asf_byteio_getQWLE(idata + 40);
	index->max_packet_count = asf_byteio_getDWLE(idata + 48);
	index->entry_count = asf_byteio_getDWLE(idata + 52);

	if (index->entry_count * 6 + 56 > index->size) {
		return ASF_ERROR_INVALID_LENGTH;
	}

	entry_data_size = index->entry_count * 6;
	entry_data = malloc(entry_data_size * sizeof(uint8_t));
	if (!entry_data) {
		free(index);
		return ASF_ERROR_OUTOFMEM;
	}
	tmp = asf_byteio_read(entry_data, entry_data_size, stream);
	if (tmp < 0) {
		free(index);
		free(entry_data);
		return tmp;
	}

	index->entries = malloc(index->entry_count * sizeof(asf_index_entry_t));
	if (!index->entries) {
		free(index);
		free(entry_data);
		return ASF_ERROR_OUTOFMEM;
	}

	for (i=0; i<index->entry_count; i++) {
		index->entries[i].packet_index = asf_byteio_getDWLE(entry_data + i*6);
		index->entries[i].packet_count = asf_byteio_getWLE(entry_data + i*6 + 4);
	}

	free(entry_data);

	return index->size;
}


