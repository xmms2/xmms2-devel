# encoding: utf-8

from xmmsclient import XmmsLoop, XmmsValueC2C
from xmmsclient.service import *
import sys

class ServiceBrowse:
	def __init__(self, service):
		self._service = service
		self._results = dict()
		self._results[service._path] = service(cb = self.introspect_cb)

	def __del__(self):
		for n, r in self._results.items():
			r.disconnect()
		self._results.clear()

	def introspect_cb(self, ns, xval):
		res = self._results.pop(ns._path, None)

		if xval.is_error():
			# Occur when the target client disconnected without replying.
			return

		cxval = XmmsValueC2C(pyval = xval)
		payload = cxval.payload
		if payload and payload.is_error():
			sys.stderr.write("! Introspection error on #%d: %s\n" % (ns._clientid, payload.value()))
			return

		for attr in dir(ns):
			if attr and attr[0] == '_':
				continue
			obj = getattr(ns, attr)
			if isinstance(obj, XmmsServiceClient):
				self._results[obj._path] = obj(cb = self.introspect_cb)

		if not len(self._results):
			self.print_ns(self._service)

	def print_ns(self, ns, prefix = '+ '):
		name = ns._path and ns._path[-1] or ("Client #%d API" % ns._clientid)
		sys.stderr.write("%s%s\n" % (prefix, name))
		prefix = '  ' + prefix
		for attr in dir(ns):
			if not attr or attr[0] == '_':
				continue
			obj = getattr(ns, attr)
			if isinstance(obj, client_method):
				sys.stderr.write("%s%s()\n" % (prefix, attr))
			elif isinstance(obj, client_broadcast):
				sys.stderr.write("%s%s [broadcast]\n" % (prefix, attr))
			elif isinstance(obj, XmmsServiceClient):
				self.print_ns(obj, prefix)
			else:
				sys.stderr.write("%s%s = %r\n" % (prefix, attr, obj))

class ScWatch(XmmsLoop):
	def __init__(self, *args, **kargs):
		super(ScWatch, self).__init__(*args, **kargs)
		self._clients = dict()

	def add_clients(self, cids, ready = False):
		if isinstance(cids, int):
			cids = (cids,)
		for i in cids:
			if i == self.client_id:
				continue
			if i in self._clients:
				c, b = self._clients[i]
			else:
				c = XmmsServiceClient(self, i)
				b = None
			if ready:
				sys.stderr.write("#%02d ready\n" % i)
				b = ServiceBrowse(c)
			else:
				sys.stderr.write("#%02d connected\n" % i)
			self._clients[i] = (c, b)

	def del_clients(self, cids):
		if isinstance(cids, int):
			cids = (cids,)
		for i in cids:
			if i in self._clients:
				sys.stderr.write("#%02d disconnected\n" % i)
				del self._clients[i]

	def watch(self):
		r1 = self.broadcast_c2c_client_connected(cb = self.connected_cb)
		r2 = self.broadcast_c2c_client_disconnected(cb = self.disconnected_cb)
		r3 = self.broadcast_c2c_ready(cb = self.ready_cb)
		self.c2c_get_ready_clients(cb = self.ready_cb)
		try:
			self.loop()
		except KeyboardInterrupt:
			pass
		finally:
			r1.disconnect()
			r2.disconnect()
			r3.disconnect()

	def check_error(self, value):
		if value and value.is_error():
			sys.stderr.write("! %s\n" % value.get_error())
			return True
		return False

	def connected_cb(self, value):
		if self.check_error(value):
			return
		self.add_clients(value.value())

	def ready_cb(self, value):
		if self.check_error(value):
			return
		self.add_clients(value.value(), True)

	def disconnected_cb(self, value):
		if self.check_error(value):
			return
		self.del_clients(value.value())


if __name__ == '__main__':
	x = ScWatch(clientname = "ScWatch")
	x.connect()
	sys.stderr.write("Watching clients connections (local id: %d)\n" % x.client_id)
	sys.stderr.write("Ctrl+C to exit...\n")
	x.watch()
