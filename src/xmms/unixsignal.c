/** @file
 * Takes care of unix-signals.
 */


#include "util.h"
#include "object.h"
#include "unixsignal.h"

#include <stdlib.h>
#include <signal.h>
#include <glib.h>


static gpointer 
sigwaiter(gpointer data){
	sigset_t signals;
	int caught;

	memset (&signals, 0, sizeof (sigset_t));
	sigaddset (&signals, SIGINT);
	
	while (1337) {
		sigwait (&signals, &caught);
		switch (caught){
		case SIGINT:
			XMMS_DBG ("Got SIGINT!");
			xmms_object_emit (XMMS_OBJECT (data), "eos-reached", NULL);
			break;
		}
	}
}

void
xmms_signal_init(xmms_object_t *obj) {
	g_thread_create (sigwaiter, obj, FALSE, NULL);
}
