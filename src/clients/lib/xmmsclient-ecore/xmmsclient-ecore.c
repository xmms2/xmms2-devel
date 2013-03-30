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

#include <Ecore.h>

#include <stdio.h>

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-ecore.h>

static Eina_Bool
on_fd_data (void *udata, Ecore_Fd_Handler *handler)
{
	xmmsc_connection_t *c = udata;
	int ret = 0;

	if (ecore_main_fd_handler_active_get (handler, ECORE_FD_ERROR)) {
		xmmsc_io_disconnect (c);
		return ret;
	}

	if (ecore_main_fd_handler_active_get (handler, ECORE_FD_READ))
		ret = xmmsc_io_in_handle (c);

	if (ecore_main_fd_handler_active_get (handler, ECORE_FD_WRITE))
		ret = xmmsc_io_out_handle (c);

	return ret;
}

static void
on_prepare (void *udata, Ecore_Fd_Handler *handler)
{
	xmmsc_connection_t *c = udata;
	int flags = ECORE_FD_READ | ECORE_FD_ERROR;

	if (xmmsc_io_want_out (c))
		flags |= ECORE_FD_WRITE;

	ecore_main_fd_handler_active_set (handler, flags);
}

void *
xmmsc_mainloop_ecore_init (xmmsc_connection_t *c)
{
	Ecore_Fd_Handler *fdh;
	int flags = ECORE_FD_READ | ECORE_FD_ERROR;

	if (xmmsc_io_want_out (c))
		flags |= ECORE_FD_WRITE;

	fdh = ecore_main_fd_handler_add (xmmsc_io_fd_get (c), flags,
	                                 on_fd_data, c, NULL, NULL);
	ecore_main_fd_handler_prepare_callback_set (fdh, on_prepare, c);

	return fdh;
}

void
xmmsc_mainloop_ecore_shutdown (xmmsc_connection_t *c, void *udata)
{
	ecore_main_fd_handler_del (udata);
}
