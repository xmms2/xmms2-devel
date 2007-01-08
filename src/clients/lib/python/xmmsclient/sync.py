
import xmmsapi

class XMMSSync:
	"""
	A wrapper for the xmmsclient.XMMS class which simplifies synchronous
	communication with the XMMS2 daemon.
	
	Instances of this class may be used just like regular xmmsclient.XMMS
	objects, except that instead of returning an XMMSResult instance, the
	value associated with the result is returned.  If the XMMSResult
	indicates an error, an XMMSError is raised instead of returning the
	value.
	"""
	# This implementation avoids using nested function definitions, as they
	# are not supported by Pyrex.
	def __init__(self, clientname=None, xmms=None):
		"""
		This constructor takes two optional arguments. If xmms is omitted
		it will create a new underlying XMMS class otherwise it will use
		the one supplied. Clientname is the name of the client and will
		default to "Unnamed Python Client"
		"""
		if not xmms:
			self.__xmms = xmmsapi.XMMS(clientname)
		else:
			self.__xmms = xmms

	def __getattr__(self, name):
		attr = getattr(self.__xmms, name)
		if callable(attr):
			return XMMSSyncMethod(attr)
		else:
			return attr


class XMMSSyncMethod:
	"""
	A factory which uses a bound XMMS object method to create a callable
	object that wraps the XMMS method for more convenient synchronous use.
	This is meant for use by the XMMSSync class.
	"""
	def __init__(self, method):
		self.method = method

	def __call__(self, *args):
		ret = self.method(*args)
		if isinstance(ret, xmmsapi.XMMSResult):
			ret.wait()
			if ret.iserror():
				raise XMMSError(ret.get_error())
			return ret.value()
		return ret


class XMMSError(Exception):
	"""
	Thrown when a synchronous method call on an XMMS client object fails.
	"""
	pass
