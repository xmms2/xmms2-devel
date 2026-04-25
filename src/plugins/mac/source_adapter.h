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

#ifndef XMMS_MAC_SOURCE_ADAPTER_H
#define XMMS_MAC_SOURCE_ADAPTER_H

#include <MAC/All.h>
#include <MAC/MACLib.h>
#include <MAC/IAPEIO.h>

#include <glib.h>

extern "C" {

#include <xmms/xmms_plugin.h>
#include <xmms/xmms_xformplugin.h>
#include <xmms/xmms_log.h>

}

class CSourceAdapter : public APE::IAPEIO
{
public:
	CSourceAdapter (xmms_xform_t *xform);
	~CSourceAdapter () {};

	// open / close
	int Open(const APE::str_utfn * pName, bool bOpenReadOnly = false) APE_OVERRIDE
	{ return ERROR_SUCCESS; }

	int Close () APE_OVERRIDE
	{ return ERROR_SUCCESS; }

	// read / write
	int Read(void * pBuffer, unsigned int nBytesToRead, unsigned int * pBytesRead) APE_OVERRIDE;
	int Write(const void * pBuffer, unsigned int nBytesToWrite, unsigned int * pBytesWritten = APE_NULL) APE_OVERRIDE
	{ return ERROR_SUCCESS; }

	// seek
	int Seek(APE::int64 nPosition, APE::SeekMethod nMethod) APE_OVERRIDE;

	// creation / destruction
	int Create (const APE::str_utfn * pName) APE_OVERRIDE
	{ return ERROR_SUCCESS; }
	int Delete () APE_OVERRIDE
	{ return ERROR_SUCCESS; }

	// other functions
	int SetEOF () APE_OVERRIDE
	{ return ERROR_SUCCESS; };
	unsigned char * GetBuffer(int * pnBufferBytes) APE_OVERRIDE
	{ return APE_NULL; }

	// attributes
	APE::int64 GetPosition () APE_OVERRIDE;
	APE::int64 GetSize () APE_OVERRIDE;

private:
	xmms_xform_t *xform;
};

#endif
