#import <CoreFoundation/CoreFoundation.h>
#import <Cocoa/Cocoa.h>

#import "xmms/xmmsclient.h"
#import "xmms/xmmsclient-cocoa.h"
#import "internal/client_ipc.h"

gboolean 
xmmsc_ipc_setup_with_cocoa (xmmsc_connection_t *c)
{
	CFRunLoopRef runLoopRef;
	 
	runLoopRef = [[ NSRunLoop currentRunLoop ] getCFRunLoop ];

	return xmmsc_ipc_setup_with_cf (c, runLoopRef);
}
