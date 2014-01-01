/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team and Ma Xuan
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

#ifndef XMMS_MAC_SOURCE_ADAPTER_H
#define XMMS_MAC_SOURCE_ADAPTER_H

#include <mac/All.h>
#include <mac/MACLib.h>
#include <mac/IO.h>

#include <glib.h>

extern "C" {

#include <xmms/xmms_plugin.h>
#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>

}

class CSourceAdapter : public CIO
{
public:
	CSourceAdapter (xmms_xform_t *xform);
	~CSourceAdapter () {};

	// open / close
	int Open (const wchar_t * pName) { return ERROR_SUCCESS; }
	int Close () { return ERROR_SUCCESS; }

	// read / write
	int Read (void * pBuffer, unsigned int nBytesToRead,
	          unsigned int * pBytesRead);
	int Write (const void * pBuffer, unsigned int nBytesToWrite,
	           unsigned int * pBytesWritten) { return ERROR_SUCCESS; };

	// seek
	int Seek (int nDistance, unsigned int nMoveMode);

	// other functions
	int SetEOF () { return ERROR_SUCCESS; };

	// creation / destruction
	int Create (const wchar_t * pName) { return ERROR_SUCCESS; }
	int Delete () { return ERROR_SUCCESS; }

	// attributes
	int GetPosition ();
	int GetSize ();
	int GetName (wchar_t * pBuffer) { return 0; }
	int GetHandle () { return 0; }

private:
	xmms_xform_t *xform;
};

#endif
