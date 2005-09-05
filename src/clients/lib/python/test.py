import xmmsclient
import sys
import signal

def pt_callback(self) :
	msec = self.get_uint()
	print "\r%02d:%02d" % (msec / 60000, (msec / 1000) % 60), 
	sys.stdout.flush()
	self.restart()

def sigint_callback(signal, frame) :
	xc.exit_loop()

signal.signal(signal.SIGINT, sigint_callback)

xc = xmmsclient.XMMS ()
xc.connect()
	
res = xc.signal_playback_playtime(pt_callback)

try :
	xc.loop()
except :
	pass

res.disconnect_signal()

# force cleanup
res = None
xc = None
