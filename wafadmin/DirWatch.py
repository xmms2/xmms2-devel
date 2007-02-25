#! /usr/bin/env python
# encoding: utf-8
# Matthias Jahn <jahn.matthias@freenet.de>, 2006

"DirWatch chooses a supported backend (fam, gamin or fallback) it is mainly a wrapper script without own methods beside this"

from Params import debug
import GaminAdaptor, FamAdaptor, FallbackAdaptor
import os

class WatchObject:
	def __init__( self, idxName, namePath, isDir, callBackThis, handleEvents ):
		"""watch object to handle a watch
		@param idxName: unique name for ref
		@param dirList: path to watch
		@param isDir: directory True or False
		@param callBackThis: is called if something in dirs in dirlist has events (handleEvents) callBackThis(idxName, changedFilePath)
		@param handleEvents: events to handle possible are 'changed', 'deleted', 'created', 'exist' suspendDirWatch after a handled change
		"""
		self.__adaptor = None
		self.__fr = None
		self.__idxName = idxName
		self.__name = namePath
		self.__isDir = isDir
		self.__callBackThis = callBackThis
		self.__handleEvents = handleEvents

	def __del__( self ):
		self.unwatch()

	def watch( self, adaptor ):
		"""start watching
		@param adaptor: dirwatch adaptor for backend
		"""
		self.__adaptor = adaptor
		if self.__fr != None:
			self.unwatch()
		if self.__isDir:
			self.__fr = self.__adaptor.watch_directory( self.__name, self.__idxName )
		else:
			self.__fr = self.__adaptor.watch_file( self.__name, self.__idxName )

	def unwatch( self ):
		"""stop watching"""
		if self.__fr:
			self.__fr = self.__adaptor.stop_watch( self.__name )

	def get_events( self ):
		"""returns all events to care"""
		return self.__handleEvents

	def get_callback( self ):
		"""returns the callback methode"""
		return self.__callBackThis

	def get_fullpath( self, fileName ):
		"""returns the full path dir + filename"""
		return os.path.join( self.__name, fileName )

	def __str__( self ):
		if self.__isDir:
			return 'DIR %s: ' %  self.__name
		else:
			return 'FILE %s: ' % self.__name

class DirectoryWatcher:
	"""DirWatch chooses a supported backend (fam, gamin or fallback)
	it is mainly a wrapper script without own methods beside this
	"""
	def __init__( self ):
		self.__adaptor = None
		self.__watcher = {}
		self.__loops = True
		self.connect()

	def __del__ ( self ):
		self.disconnect()

	def __raise_disconnected( self ):
		raise( "Already disconnected" )

	def disconnect( self ):
		if  self.__adaptor:
			self.suspend_all_watch()
		self.__adaptor = None

	def connect( self ):
		if  self.__adaptor:
			self.disconnect()
		if FamAdaptor.support:
			debug( "using FamAdaptor" )
			self.__adaptor = FamAdaptor.FamAdaptor( self.__processDirEvents )
			if self.__adaptor == None:
				raise "something is strange"
		elif GaminAdaptor.support:
			debug( "using GaminAdaptor" )
			self.__adaptor = GaminAdaptor.GaminAdaptor(self.__processDirEvents)
		else:
			debug( "using FallbackAdaptor" )
			self.__adaptor = FallbackAdaptor.FallbackAdaptor(self.__processDirEvents)

	def add_watch( self, idxName, callBackThis, dirList, handleEvents = ['changed', 'deleted', 'created'] ):
		"""add dirList to watch.
		@param idxName: unique name for ref
		@param callBackThis: is called if something in dirs in dirlist has events (handleEvents) callBackThis(idxName, changedFilePath)
		@param dirList: list of dirs to watch
		@param handleEvents: events to handle possible are 'changed', 'deleted', 'created', 'exist' suspendDirWatch after a handled change
		"""
		self.remove_watch( idxName )
		self.__watcher[idxName] = []
		for directory in dirList:
			watchObject = WatchObject( idxName, os.path.abspath( directory ), 1, callBackThis, handleEvents )
			self.__watcher[idxName].append( watchObject )
		self.resume_watch( idxName )

	def remove_watch( self, idxName ):
		"""remove DirWatch with name idxName"""
		if self.__watcher.has_key( idxName ):
			self.suspend_watch( idxName )
			del self.__watcher[idxName]

	def remove_all_watch( self ):
		"""remove all DirWatcher"""
		self.__watcher = {}

	def suspend_watch( self, idxName ):
		"""suspend DirWatch with name idxName. No dir/filechanges will be reacted until resume"""
		if self.__watcher.has_key( idxName ):
			for watchObject in self.__watcher[idxName]:
				watchObject.unwatch()

	def suspend_all_watch( self ):
		"""suspend all DirWatcher ... they could be resumed with resume_all_watch"""
		for idxName in self.__watcher.keys():
			self.suspend_watch( idxName )

	def resume_watch( self, idxName ):
		"""resume a DirWatch that was supended with suspendDirWatch or suspendAllDirWatch"""
		for watchObject in self.__watcher[idxName]:
			watchObject.watch( self.__adaptor )

	def resume_all_watch( self ):
		""" resume all DirWatcher"""
		for idxName in self.__watcher.keys():
			self.resume_watch( idxName )

	def __processDirEvents( self, pathName, event, idxName ):
		if event in self.__watcher[idxName][0].get_events():
			#self.disconnect()
			self.suspend_watch(idxName)
			__watcher = self.__watcher[idxName][0]
			__watcher.get_callback()( idxName, __watcher.get_fullpath( pathName ), event )
			#self.connect()
			self.resume_watch( idxName )

	def request_end_loop( self ):
		"""sets a flag that stops the loop. it do not stop the loop directly!"""
		self.__loops = False

	def loop( self ):
		"""wait for dir events and start handling of them"""
		try:
			self.__loops = True
			while ( self.__loops ) and ( self.__adaptor != None ) :
				self.__adaptor.wait_for_event()
				while self.__adaptor.event_pending():
					self.__adaptor.handle_events()
					if not self.__loops:
						break
		except KeyboardInterrupt:
			self.request_end_loop()


class Test:
	def __init__( self ):
		self.fam_test = DirectoryWatcher()
		self.fam_test.add_watch( "tmp Test", self.thisIsCalledBack, ["/tmp"] )
		self.fam_test.loop()
#		self.fam_test.loop()

	def thisIsCalledBack( self, idxName, pathName, event ):
		print "idxName=%s, Path=%s, Event=%s " % ( idxName, pathName, event )
		self.fam_test.resume_watch( idxName )

if __name__ == "__main__":
	Test()
