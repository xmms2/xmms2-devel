/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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




/** @file
 * Takes care of unix-signals.
 */


#include "xmms/unixsignal.h"
#include "xmms/util.h"
#include "xmms/object.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <string.h>
#include <glib.h>
#include <unistd.h>


static gpointer 
sigwaiter(gpointer data){
	sigset_t signals;
	int caught;

	memset (&signals, 0, sizeof (sigset_t));
	sigaddset (&signals, SIGINT);
	sigaddset (&signals, SIGTERM);
	
	while (1337) {
		sigwait (&signals, &caught);
		switch (caught){
		case SIGINT:
			XMMS_DBG ("Got SIGINT!");
			exit (0);
			break;
		case SIGTERM:
			XMMS_DBG ("Got SIGTERM! Bye!");
			exit (0);
			break;
		}
	}
}

void
xmms_signal_init() {
	g_thread_create (sigwaiter, NULL, FALSE, NULL);
}
