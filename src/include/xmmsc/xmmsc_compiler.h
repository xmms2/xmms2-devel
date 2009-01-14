/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#ifndef XMMSC_COMPILER_H
#define XMMSC_COMPILER_H

#ifdef _MSC_VER
#define inline __inline
#endif

#if defined (__GNUC__) && __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 1)
#define XMMS_DEPRECATED __attribute__((deprecated))
#else
#define XMMS_DEPRECATED
#endif

#endif
