""" 
This reactor is a special one for XMMS2, it must be used in order
assure correctness.

In your twisted program before you import anything else do:
from xmmstwisted import xmmsreactor
xmmsreactor.install()

"""

from twisted.internet import selectreactor

class XMMSReactor(selectreactor.SelectReactor):
	def __init__(self):
		self.xmms = None
		self.io = None
		selectreactor.SelectReactor.__init__(self)

	def doIteration(self, timeout, reads=selectreactor.reads, writes=selectreactor.writes):

		if self.xmms and self.io:
			if self.xmms.want_ioout():
				self.io.startWriting()
			else:
				self.io.stopWriting()

		selectreactor.SelectReactor.doIteration(self, timeout, reads, writes)

def install():
	reactor = XMMSReactor()
	from twisted.internet.main import installReactor
	installReactor(reactor)

