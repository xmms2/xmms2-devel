/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#ifndef __RB_COLLECTION_H
#define __RB_COLLECTION_H

/* FIXME: This is not nice but there's no very clean way around the ugly
 * warnings, glibc does about the same but on compile time (this could be moved
 * to waf?) */
#if defined(__x86_64__)
#  define XPOINTER_TO_INT(p)      ((int)  (long)  (p))
#  define XPOINTER_TO_UINT(p)     ((unsigned int)  (unsigned long)  (p))
#  define XINT_TO_POINTER(i)      ((void *)  (long)  (i))
#  define XUINT_TO_POINTER(u)     ((void *)  (unsigned long)  (u))
#else
#  define XPOINTER_TO_INT(p)      ((int)  (p))
#  define XPOINTER_TO_UINT(p)     ((unsigned int)  (p))
#  define XINT_TO_POINTER(i)      ((void *)  (i))
#  define XUINT_TO_POINTER(u)     ((void *)  (u))
#endif

void Init_Collection (VALUE mXmms);

VALUE TO_XMMS_CLIENT_COLLECTION (xmmsc_coll_t *coll);
xmmsc_coll_t *FROM_XMMS_CLIENT_COLLECTION (VALUE rbcoll);

#endif /* __RB_COLLECTION_H */
