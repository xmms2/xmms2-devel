import xmmsclient
import sys

def pt_callback(self) :
	msec = self.get_uint()
	print "\r%02d:%02d" % (msec / 60000, (msec / 1000) % 60), 
	sys.stdout.flush()
	self.restart()

xc = xmmsclient.XMMS ()
xc.connect()
	
xc.signal_playback_playtime(pt_callback)

xc.loop()

