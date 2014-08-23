# encoding: utf-8
"""

 A GLib connector for PyGTK and Gobject introspection
 Tobias Rundstrom <tru@xmms.org>
 Raphaël Bois <virtualdust@gmail.com>

 GLibConnectorBase(xmms, gobject): create a glib connector using the provided
                                   gobject module.
 GLibConnector(xmms): create a glib connector using pygtk
 GiGLibConnector(xmms): create a glib connector using gobject introspection

"""
class GLibConnectorBase(object):
	def __init__(self, xmms, gobject):
		self.go = gobject
		self.in_id = None
		self.out_id = None
		self.reconnect(xmms)

	def need_out(self, i):
		if self.xmms.want_ioout() and self.out_id is None:
			self.out_id = self.go.io_add_watch(self.xmms.get_fd(), self.go.IO_OUT, self.handle_out)

	def handle_in(self, source, cond):
		if cond == self.go.IO_IN:
			return self.xmms.ioin()
		return True

	def handle_out(self, source, cond):
		if cond == self.go.IO_OUT and self.xmms.want_ioout():
			self.xmms.ioout()
		if not self.xmms.want_ioout():
			self.out_id = None
		return not self.out_id is None

	def reconnect(self, xmms=None):
		self.disconnect()
		if not xmms is None:
			self.xmms = xmms
		self.xmms.set_need_out_fun(self.need_out)
		self.in_id = self.go.io_add_watch(self.xmms.get_fd(), self.go.IO_IN, self.handle_in)
		self.out_id = None

	def disconnect(self):
		if not self.in_id is None:
			self.go.source_remove(self.in_id)
			self.in_id = None
		if not self.out_id is None:
			self.go.source_remove(self.out_id)
			self.out_id = None


class GLibConnector(GLibConnectorBase):
	def __init__(self, xmms):
		import gobject
		super(GLibConnector, self).__init__(xmms, gobject)


class GiGLibConnector(GLibConnectorBase):
	def __init__(self, xmms):
		from gi.repository import GObject as gobject
		super(GiGLibConnector, self).__init__(xmms, gobject)
