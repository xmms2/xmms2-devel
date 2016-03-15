
import xmmsapi

class XmmsSync(xmmsapi.XmmsProxy):
	"""
	A wrapper for the `xmmsclient.Xmms` class which simplifies synchronous
	communication with the XMMS2 daemon.

	Instances of this class may be used just like regular `xmmsclient.Xmms`
	objects, except that instead of returning an `XmmsResult` instance, the
	value associated with the result is returned.  If the `XmmsResult`
	indicates an error, an `XmmsError` is raised instead of returning the
	value.

	>>> import xmmsclient
	>>> xc = xmmsclient.XmmsSync('clientname')
	>>> xc.connect()
	>>> xc.playback_start()
	"""
	def __init__(self, clientname=None, xmms=None):
		"""
		This constructor takes two optional arguments. If xmms is omitted
		it will create a new underlying XMMS class otherwise it will use
		the one supplied. Clientname is the name of the client and will
		default to "Unnamed Python Client"
		"""
		if xmms is None:
			xmms = xmmsapi.Xmms(clientname)
		super(XmmsSync, self).__init__(xmms)

	def __getattr__(self, name):
		try:
			return super(XmmsSync, self).__getattr__(name)
		except AttributeError:
			pass

		attr = getattr(self._xmms, name)
		if hasattr(attr, '__call__'):
			def _(*args, **kwargs):
				ret = attr(*args, **kwargs)
				if isinstance(ret, xmmsapi.XmmsResult):
					ret.wait()
					if ret.is_error():
						raise ret.xvalue.get_error()
					return ret.value()
				return ret
			try:
				_.__doc__ = attr.__doc__
				_.func_name = '<sync version of %s>' % name
			except:
				pass
			return _
		else:
			return attr

	def __dir__(self):
		return dir(self._xmms)

from xmmsvalue import XmmsError
