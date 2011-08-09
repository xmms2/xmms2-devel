# encoding: utf-8
"""

 A GLib connector for PyGTK - 
	to use with your cool PyGTK xmms2 client.
 Tobias Rundstrom <tru@xmms.org>
 Raphaël Bois <virtualdust@gmail.com>

 Just create the GLibConnector() class with a xmmsclient as argument

"""

from gobject import io_add_watch, IO_OUT, IO_IN, source_remove

class GLibConnector:
	def __init__(self, xmms):
		self.in_id = None
		self.out_id = None
		self.reconnect(xmms)

	def need_out(self, i):
		if self.xmms.want_ioout() and self.out_id is None:
			self.out_id = io_add_watch(self.xmms.get_fd(), IO_OUT, self.handle_out)

	def handle_in(self, source, cond):
		if cond == IO_IN:
			return self.xmms.ioin()

		return True

	def handle_out(self, source, cond):
		if cond == IO_OUT and self.xmms.want_ioout():
			self.xmms.ioout()
		if not self.xmms.want_ioout():
			self.out_id = None

		return not self.out_id is None

	def reconnect(self, xmms=None):
		self.disconnect()
		if not xmms is None:
			self.xmms = xmms
		self.xmms.set_need_out_fun(self.need_out)
		self.in_id = io_add_watch(self.xmms.get_fd(), IO_IN, self.handle_in)
		self.out_id = None

	def disconnect(self):
		if not self.in_id is None:
			source_remove(self.in_id)
			self.in_id = None
		if not self.out_id is None:
			source_remove(self.out_id)
			self.out_id = None
