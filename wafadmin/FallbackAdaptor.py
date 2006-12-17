#! /usr/bin/env python
# encoding: utf-8
# Matthias Jahn <jahn.matthias@freenet.de> 2006

"""
Fallback WatchMonitor should work anywhere ..;-)
this do not depends on gamin or fam instead it polls for changes
it works at least under linux ... windows or other  *nix are untested
"""

import os, time

support = True

class Fallback:
	class Helper:
		def __init__( self, callBack, userdata ):
			self.currentFiles = {}
			self.oldFiles = {}
			self.__firstRun = True
			self.callBack = callBack
			self.userdata = userdata

		def isFirstRun( self ):
			if self.__firstRun:
				self.__firstRun = False
				return True
			else:
				return False

	def __init__( self ):
		self.__dirs = {}
		#event lists for changed and deleted
		self.__changeLog = {}

	def __traversal( self, dirName ):
		"""Traversal function for directories
Basic principle: all_files is a dictionary mapping paths to
modification times.  We repeatedly crawl through the directory
tree rooted at 'path', doing a stat() on each file and comparing
the modification time.
"""
		files = os.listdir( dirName )
		firstRun = self.__dirs[dirName].isFirstRun()

		for filename in files:
			path = os.path.join( dirName, filename )
			try:
				fileStat = os.stat( path )
			except os.error:
				# If a file has been deleted since the lsdir
				# scanning the directory and now, we'll get an
				# os.error here.  Just ignore it -- we'll report
				# the deletion on the next pass through the main loop.
				continue
			modifyTime = self.__dirs[dirName].oldFiles.get( path )
			if modifyTime is not None:
				# Record this file as having been seen
				del self.__dirs[dirName].oldFiles[path]
				# File's mtime has been changed since we last looked at it.
				if fileStat.st_mtime > modifyTime:
					self.__changeLog[path] = 'changed'
			else:
				if firstRun:
					self.__changeLog[path] = 'exists'
				else:
					# No recorded modification time, so it must be
					# a brand new file
					self.__changeLog[path] = 'created'
			# Record current mtime of file.
			self.__dirs[dirName].currentFiles[path] = fileStat.st_mtime

	def watch_directory( self, namePath, callBack, idxName ):
		self.__dirs[namePath] = self.Helper( callBack, idxName )
		return self

	def unwatch_directory( self, namePath ):
		if self.__dirs.get( namePath ):
			del self.__dirs[namePath]

	def event_pending( self ):
		for dirName in self.__dirs.keys():
			self.__dirs[dirName].oldFiles = self.__dirs[dirName].currentFiles.copy()
			self.__dirs[dirName].currentFiles = {}
			self.__traversal( dirName )
			for deletedFile in self.__dirs[dirName].oldFiles.keys():
				self.__changeLog[deletedFile] = 'deleted'
				del self.__dirs[dirName].oldFiles[deletedFile]
		return len( self.__changeLog )

	def handle_events( self ):
		pathName = self.__changeLog.keys()[0]
		event = self.__changeLog[pathName]
		dirName = os.path.dirname( pathName )
		self.__dirs[dirName].callBack( pathName, event, self.__dirs[dirName].userdata )
		del self.__changeLog[pathName]

class FallbackAdaptor:
	def __init__( self, eventHandler ):
		self.__fallback = Fallback()
		self.__eventHandler = eventHandler # callBack function
		self.__watchHandler = {} # {name : famId}

	def __del__( self ):
		if self.__fallback:
			for handle in self.__watchHandler.keys():
				self.stop_watch( handle )
			self.__fallback = None

	def __check_fallback(self):
		if self.__fallback == None:
			raise "fallback not init"

	def watch_directory( self, name, idxName ):
		self.__check_fallback()
		if self.__watchHandler.has_key( name ):
			raise "dir allready watched"
		# set famId
		self.__watchHandler[name] = self.__fallback.watch_directory( name, self.__eventHandler, idxName )
		return(self.__watchHandler[name])

	def watch_file( self, name, idxName ):
		self.__check_fallback()
		if self.__watchHandler.has_key( name ):
			raise "file allready watched"
		# set famId
		self.__watchHandler[name] = self.__fallback.watch_directory( name, self.__eventHandler, idxName )
		return(self.__watchHandler[name])

	def stop_watch( self, name ):
		self.__check_fallback()
		if self.__watchHandler.has_key( name ):
			self.__fallback.unwatch_directory(name)
			del self.__watchHandler[name]
		return None

	def wait_for_event( self ):
		self.__check_fallback()
		time.sleep( 1 )

	def event_pending( self ):
		self.__check_fallback()
		return self.__fallback.event_pending()

	def handle_events( self ):
		self.__check_fallback()
		self.__fallback.handle_events()

