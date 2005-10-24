/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2005 Daniel Svensson, <daniel@nittionio.nu> 
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

#ifndef __MUSEPACK_H__
#define __MUSEPACK_H__

#include <mpcdec/mpcdec.h>
#include <mpcdec/reader.h>

typedef struct xmms_mpc_data_St {
	mpc_decoder decoder;
	mpc_reader reader;
	mpc_streaminfo info;
} xmms_mpc_data_t;

#endif

