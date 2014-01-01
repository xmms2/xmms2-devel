/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-ecore.h>

#include <ruby.h>
#include <xmmsc/xmmsc_stdbool.h>

#include "rb_xmmsclient.h"

void Init_xmmsclient_ecore (void);

static VALUE
c_add_to_ecore_mainloop (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	xmms->ecore_handle = xmmsc_mainloop_ecore_init (xmms->real);

	return self;
}

static VALUE
c_remove_from_ecore_mainloop (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	xmmsc_mainloop_ecore_shutdown (xmms->real, xmms->ecore_handle);

	return self;
}

void
Init_xmmsclient_ecore (void)
{
	VALUE m, c;

	rb_require ("xmmsclient");
	rb_require ("ecore");

	m = rb_const_get (rb_cModule, rb_intern ("Xmms"));
	c = rb_const_get (m, rb_intern ("Client"));

	rb_define_method (c, "add_to_ecore_mainloop",
	                  c_add_to_ecore_mainloop, 0);
	rb_define_method (c, "remove_from_ecore_mainloop",
	                  c_remove_from_ecore_mainloop, 0);
}
