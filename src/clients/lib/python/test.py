import xmmsclient
import sys

class MediainfoRes (xmmsclient.XMMSResult) :
	def Callback (self) :
		print self.GetHashTable ()

class PlaytimeRes (xmmsclient.XMMSResult) :
	def Callback (self) :
		msec = self.GetUInt ()
		print "\r%02d:%02d" % (msec / 60000, (msec / 1000) % 60), 
		sys.stdout.flush ()
		self.Restart ()

class CurrentIDRes (xmmsclient.XMMSResult) :
	def __init__ (self) :
		self.xc = None

	def Callback (self) :
		id = self.GetUInt ()
		self.xc.PlaylistGetMediainfo (id, MediainfoRes)

xc = xmmsclient.XMMS ()
xc.Connect ()
	
xc.SignalPlaybackPlaytime (PlaytimeRes)
res = xc.BroadcastPlaybackCurrentID (CurrentIDRes)
res.xc = xc

xc.PythonLoop ()

