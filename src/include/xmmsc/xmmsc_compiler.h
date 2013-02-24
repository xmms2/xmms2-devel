/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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

/* for CLANG */
#ifndef __has_attribute
#	define __has_attribute(x) 0
#endif

#if __has_attribute (deprecated) || \
    (defined (__GNUC__) && __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 1))
#	define XMMS_DEPRECATED __attribute__((deprecated))
#else
#	define XMMS_DEPRECATED
#endif

#if __has_attribute (sentinel) || \
    defined (__GNUC__)
#	define XMMS_SENTINEL(x) __attribute__((sentinel(x)))
#else
#	define XMMS_SENTINEL(x)
#endif

#if defined (__GNUC__) && __GNUC__ >= 4
#	define XMMS_PUBLIC __attribute__((visibility ("default")))
#else
#	define XMMS_PUBLIC
#endif

#endif
