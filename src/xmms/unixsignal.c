/** @file
 * Takes care of unix-signals.
 */


#include "unixsignal.h"
#include "util.h"
#include "object.h"
#include "core.h"

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
	
	while (1337) {
		sigwait (&signals, &caught);
		switch (caught){
		case SIGINT:
			XMMS_DBG ("Got SIGINT!");
			xmms_core_play_next ();
			break;
		}
	}
}

void
xmms_signal_init() {
	g_thread_create (sigwaiter, NULL, FALSE, NULL);
}
