#! /usr/bin/env python
# encoding: utf-8
# Matthias Jahn <jahn.matthias@freenet.de> 2006

"""Fam WatchMonitor depends on python-fam ... it works with fam or gamin demon"""

import select, errno
try:
	import _fam
	# check if fam or gamin runs and accepts connenction
	test = _fam.open()
	test.close()
	test = None
	support = True
except:
	support = False

class FamAdaptor:
	"""fam helper class for use with DirWatcher"""
	def __init__( self, eventHandler ):
		""" creates the fam adaptor class
		@param eventHandler: callback method for event handling"""
		self.__fam = _fam.open()
		self.__eventHandler = eventHandler # callBack function
		self.__watchHandler = {} # {name : famId}

	def __del__( self ):
		if self.__fam:
			for handle in self.__watchHandler.keys():
				self.stop_watch( handle )
			self.__fam.close()

	def __check_fam(self):
		if self.__fam == None:
			raise "fam not init"

	def watch_directory( self, name, idxName ):
		self.__check_fam()
		if self.__watchHandler.has_key( name ):
			raise "dir allready watched"
		# set famId
		self.__watchHandler[name] = self.__fam.monitorDirectory( name, idxName )
		return(self.__watchHandler[name])

	def watch_file( self, name, idxName ):
		self.__check_fam()
		if self.__watchHandler.has_key( name ):
			raise "file allready watched"
		# set famId
		self.__watchHandler[name] = self.__fam.monitorFile( name, idxName )
		return(self.__watchHandler[name])

	def stop_watch( self, name ):
		self.__check_fam()
		if self.__watchHandler.has_key( name ):
			self.__watchHandler[name].cancelMonitor()
			del self.__watchHandler[name]
		return None

	def wait_for_event( self ):
		self.__check_fam()
		try:
			select.select( [self.__fam], [], [] )
		except select.error, er:
			errnumber, strerr = er
			if errnumber != errno.EINTR:
				raise strerr

	def event_pending( self ):
		self.__check_fam()
		return self.__fam.pending()

	def handle_events( self ):
		self.__check_fam()
		fe = self.__fam.nextEvent()
		#pathName, event, idxName
		self.__eventHandler(fe.filename, fe.code2str(), fe.userData)

