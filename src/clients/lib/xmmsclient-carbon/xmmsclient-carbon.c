#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>

#include "xmms/xmmsclient.h"
#include "internal/client_ipc.h"
#include "internal/xmmsclient_int.h"

gboolean 
xmmsc_ipc_setup_with_carbon (xmmsc_connection_t *c)
{
	EventLoopRef evLoopRef;
	CFRunLoopRef runLoopRef;
	 
	evLoopRef = GetMainEventLoop ();
	runLoopRef = (CFRunLoopRef) GetCFRunLoopFromEventLoop (evLoopRef);

	return xmmsc_ipc_setup_with_cf (c, runLoopRef);
}
