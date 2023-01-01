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

#include "source_adapter.h"

CSourceAdapter::CSourceAdapter (xmms_xform_t *xform)
{
	this->xform = xform;
}

int
CSourceAdapter::Read (void * pBuffer, unsigned int nBytesToRead,
                      unsigned int * pBytesRead)
{
	int ret = 0;
	xmms_error_t error;

	xmms_error_reset (&error);

	ret = xmms_xform_read (xform,
	                       (gchar *)pBuffer,
	                       nBytesToRead,
	                       &error);
	*pBytesRead = ret;

	return (error.code == XMMS_ERROR_NONE) ? ERROR_SUCCESS : ERROR_IO_READ;
}

int
CSourceAdapter::Seek (int nDistance, unsigned int nMoveMode)
{
	xmms_xform_seek_mode_t whence;
	xmms_error_t error;
	gint pos;

	xmms_error_reset (&error);

	switch (nMoveMode)
	{
	case FILE_BEGIN:
		whence = XMMS_XFORM_SEEK_SET;
		break;
	case FILE_CURRENT:
		whence = XMMS_XFORM_SEEK_CUR;
		break;
	case FILE_END:
		whence = XMMS_XFORM_SEEK_END;
		break;
	}

	pos = xmms_xform_seek (xform, nDistance, whence, &error);

	return (error.code == XMMS_ERROR_NONE) ? ERROR_SUCCESS : -1;
}

int
CSourceAdapter::GetPosition ()
{
	gint pos;
	xmms_error_t error;

	xmms_error_reset (&error);

	pos = xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &error);

	return (error.code == XMMS_ERROR_NONE) ? pos : -1;
}

int
CSourceAdapter::GetSize ()
{
	const gchar *metakey;
	gint32 size = -1;

	metakey = XMMS_MEDIALIB_ENTRY_PROPERTY_SIZE;
	xmms_xform_metadata_get_int (xform, metakey, &size);

	return size;
}
