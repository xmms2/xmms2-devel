#include <CoreFoundation/CoreFoundation.h>

#include "xmms/xmmsclient.h"
#include "internal/client_ipc.h"
#include "internal/xmmsclient_int.h"

static void 
xmmsc_ipc_cf_event_callback (CFSocketRef s, 
			     CFSocketCallBackType type, 
			     CFDataRef address, 
			     const void *data, 
			     void *info)
{
	CFSocketContext context;

	context.version = 0;
	CFSocketGetContext (s, &context);

	if (type == kCFSocketCloseOnInvalidate) {
		xmmsc_ipc_error_set (context.info, 
				     "Remote host disconnected");
		xmmsc_ipc_disconnect (context.info);
	} else if (type == kCFSocketReadCallBack) {
		xmmsc_ipc_io_in_callback (context.info);
	}
}


gboolean
xmmsc_ipc_setup_with_cf (xmmsc_connection_t *c,
			 CFRunLoopRef runLoopRef)
{
	CFRunLoopSourceRef runLoopSourceRef;
	CFSocketContext context;
	CFSocketRef sockRef;

	context.version = 0;
	context.info = c->ipc;
	context.retain = NULL; 
	context.release = NULL;
	context.copyDescription = NULL;

	sockRef = CFSocketCreateWithNative (kCFAllocatorDefault, 
					    xmmsc_ipc_fd_get (c->ipc),
					    kCFSocketReadCallBack,
					    &xmmsc_ipc_cf_event_callback,
					    &context);

	runLoopSourceRef = CFSocketCreateRunLoopSource (kCFAllocatorDefault, 
							sockRef, 4);

	CFRunLoopAddSource (runLoopRef, runLoopSourceRef, kCFRunLoopDefaultMode);

	return TRUE;
}
