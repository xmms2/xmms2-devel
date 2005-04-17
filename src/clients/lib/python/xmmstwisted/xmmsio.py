"""
XMMSIO - for use to communicate with XMMS2 daemon.
"""

# system imports
import sys, os, select, errno

#xmms imports
import xmmsclient

# Sibling Imports
from twisted.internet import abstract, fdesc, protocol
from twisted.internet import selectreactor

		

class XMMSIO(abstract.FileDescriptor):
	connected = 1

	def __init__(self, xmms):
		abstract.FileDescriptor.__init__(self)
		self.xmms = xmms
		self.startReading()

	def fileno(self):
		return self.xmms.get_fd()
    
	def doWrite(self):
		self.xmms.ioout()
        
	def doRead(self):
		ret = self.xmms.ioin()

	def connectionLost(self, reason):
		print "want to close"

