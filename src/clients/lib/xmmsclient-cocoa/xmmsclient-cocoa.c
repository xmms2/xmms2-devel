#include <CoreFoundation/CoreFoundation.h>
#include <Cocoa/Cocoa.h>

#include "xmms/xmmsclient.h"
#include "internal/client_ipc.h"
#include "internal/xmmsclient_int.h"

gboolean 
xmmsc_ipc_setup_with_cocoa (xmmsc_connection_t *c)
{
	CFRunLoopRef runLoopRef;

	runLoopRef = [[ NSRunLoop currentRunLoop ] getCFRunLoop ];

	return xmmsc_ipc_setup_with_cf (c, runLoopRef);
}
