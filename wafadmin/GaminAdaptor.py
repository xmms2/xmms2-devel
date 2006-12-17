#! /usr/bin/env python
# encoding: utf-8
# Oscar Blumberg 2006 (nael)
# Matthias Jahn <jahn.matthias@freenet.de>

"""Depends on python gamin and on gamin demon"""

import select, errno
try:
	import gamin
	# check if gamin runs and accepts connenction
	test = gamin.WatchMonitor()
	test.disconnect()
	test = None
	support = True
except:
	support = False

class GaminAdaptor:
	"""gamin helper class for use with DirWatcher"""
	def __init__( self, eventHandler ):
		""" creates the gamin wrapper
		@param eventHandler: callback method for event handling"""
		self.__gamin = gamin.WatchMonitor()
		self.__eventHandler = eventHandler # callBack function
		self.__watchHandler = {} # {name : famId}

	def __del__( self ):
		"""clean remove"""
		if self.__gamin:
			for handle in self.__watchHandler.keys():
				self.stop_watch( handle )
			self.__gamin.disconnect()
			self.__gamin = None

	def __check_gamin(self):
		"""is gamin connected"""
		if self.__gamin == None:
			raise "gamin not init"

	def __code2str( self, event ):
		"""convert event numbers to string"""
		gaminCodes = {
			1:"changed",
			2:"deleted",
			3:"StartExecuting",
			4:"StopExecuting",
			5:"created",
			6:"moved",
			7:"acknowledge",
			8:"exists",
			9:"endExist"
		}
		try:
			return gaminCodes[event]
		except:
			return "unknown"

	def __eventhandler_helper(self, pathName, event, idxName):
		"""local eventhandler helps to convert event numbers to string"""
		self.__eventHandler(pathName, self.__code2str(event), idxName)

	def watch_directory( self, name, idxName ):
		self.__check_gamin()
		if self.__watchHandler.has_key( name ):
			raise "dir allready watched"
		# set gaminId
		self.__watchHandler[name] = self.__gamin.watch_directory( name, self.__eventhandler_helper, idxName )
		return(self.__watchHandler[name])

	def watch_file( self, name, idxName ):
		self.__check_gamin()
		if self.__watchHandler.has_key( name ):
			raise "file allready watched"
		# set famId
		self.__watchHandler[name] = self.__gamin.watch_directory( name, self.__eventhandler_helper, idxName )
		return(self.__watchHandler[name])

	def stop_watch( self, name ):
		self.__check_gamin()
		if self.__watchHandler.has_key( name ):
			self.__gamin.stop_watch(name)
			del self.__watchHandler[name]
		return None

	def wait_for_event( self ):
		self.__check_gamin()
		try:
			select.select([self.__gamin.get_fd()], [], [])
		except select.error, er:
			errnumber, strerr = er
			if errnumber != errno.EINTR:
				raise strerr

	def event_pending( self ):
		self.__check_gamin()
		return self.__gamin.event_pending()

	def handle_events( self ):
		self.__check_gamin()
		self.__gamin.handle_events()

