import xmmsclient_binding
import time

x = xmmsclient_binding.XMMS ()
x.connect ("/tmp/xmms-dbus-tru")

def handlepltm (res) :
	print "%d" % (res.uint())
	res.restart ()
	
r = x.PlaybackPlaytime ()
r.setnotifier (handlepltm)

x.loop ()
