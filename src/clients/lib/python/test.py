import xmmsclient
import sys

class MediainfoRes (xmmsclient.XMMSResult) :
	def callback(self) :
		print self.get_hashtable()

class PlaytimeRes (xmmsclient.XMMSResult) :
	def callback(self) :
		msec = self.get_uint()
		print "\r%02d:%02d" % (msec / 60000, (msec / 1000) % 60), 
		sys.stdout.flush()
		self.restart()

class CurrentIDRes (xmmsclient.XMMSResult) :
	def __init__ (self) :
		self.xc = None

	def callback (self) :
		id = self.get_uint()
		self.xc.medialib_get_info(id, MediainfoRes)

xc = xmmsclient.XMMS ()
xc.connect()
	
xc.signal_playback_playtime(PlaytimeRes)
#res = xc.broadcast_playback_current_id(CurrentIDRes)
#res.xc = xc

xc.python_loop()

