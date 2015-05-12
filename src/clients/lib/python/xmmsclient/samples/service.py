#!/usr/bin/env python
# encoding: utf-8

from xmmsclient import *
from xmmsclient.service import *

class SampleService(XmmsServiceNamespace):
	"""Cool service"""
	namespace_path = ('org', 'xmms2', 'example',)

	answer = service_constant(42)
	somestring = service_constant("Hello, world!")

	hello = service_broadcast(doc = "Send hello to the world")

	@service_method()
	def getTheAnswer(self):
		"""Answer to life the universe and everything"""
		return self.answer

	@service_method(
		positional = (
			method_arg('message', 'string', "Message to emit", default = "Hello, world!"),
			),
		)
	def sayHello(self, message = "Hello, world!"):
		"""Request hello broadcast to be emitted"""
		self.hello.emit(message)


	class nested(XmmsServiceNamespace):
		"""A namespace in the service"""
		@service_method(
			positional = (
				method_arg('arg1', 'integer', "Argument 1"),
				method_arg('arg2', 'string', "Argument 2"),
				method_varg(),
				),
			named = (
				method_varg(),
				),
			)
		def test(self, arg1, arg2, *args, **kargs):
			"""Some test method"""
			return "sample.answer=%d; arg1=%d; arg2=%s; *=%r; **=%r" % (
				self.parent.getTheAnswer(),
				arg1,
				arg2,
				args,
				kargs,
				)


if __name__ == '__main__':
	import sys
	xs = XmmsLoop("ServiceClientTest")
	xs.connect()
	my_service = SampleService(xs)
	my_service.register()
	sys.stderr.write("Service registered (local id: %d)\n" % xs.client_id)
	sys.stderr.write("Ctrl+C to exit...\n")
	try:
		xs.loop()
	except KeyboardInterrupt:
		pass
