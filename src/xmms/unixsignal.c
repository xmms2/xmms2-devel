/** @file
 * Takes care of unix-signals.
 */


#include "xmms/unixsignal.h"
#include "xmms/util.h"
#include "xmms/object.h"
#include "xmms/core.h"

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
