/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003, 2004 Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include <ruby.h>

#include "rb_xmmsclient_main.h"
#include "rb_xmmsclient.h"
#include "rb_result.h"

void Init_xmmsclient (void)
{
#ifdef HAVE_ECORE
	rb_require ("ecore");
#endif

	mXmmsClient = rb_define_module ("XmmsClient");

	Init_XmmsClient ();
	Init_Result ();
}
