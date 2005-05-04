#!/usr/bin/python

import xmmsclient
import sys

xmms = xmmsclient.XMMS ()
xmms.connect (None)

def fmt_time (tme) :
	return "%02d:%02d" % (int (tme)/60000, (int (tme)/1000)%60)

class Command :
	islistfunc = 0

	def __init__ (self) :
		global xmms
		self.xmms = xmms

	def func (self) :
		pass

	def listfunc (self) :
		pass
	
class cmd_play (Command) :
	def func (self) :
		return self.xmms.PlaybackStart ()
	
class cmd_stop (Command) :
	def func (self) :
		return self.xmms.PlaybackStop ()
	
class cmd_pause (Command) :
	def func (self) :
		return self.xmms.PlaybackPause ()

class cmd_remove (Command) :
	def func (self, id) :
		return self.xmms.PlaylistRemove (id)

class cmd_add (Command) :
	def func (self, path) :
		return self.xmms.PlaylistAdd (path)

class cmd_list (Command) :
	def __init__ (self) :
		global xmms
		self.islistfunc = 1
		self.xmms = xmms

	def func (self) :
		return self.xmms.PlaylistList ()

	def listfunc (self, list) :
		r = self.xmms.PlaybackCurrentId ()
		r.wait ()
		id = r.uint ()
		for i in list :
			r = self.xmms.PlaylistMediainfo (i)
			r.wait ()
			m = r.mediainfo ()
			if id == m["id"] :
				print "->",
			else :
				print "  ",

			print "[%d] " % (m["id"]),
			if (m.has_key ("artist")) :
				print "%s - %s" % (m["artist"], m["title"]),
			else :
				print m[m.rfind ("/")+1:],

			print "(%s)" % fmt_time (m["duration"])


class cmd_next (Command) :
	def func (self) :
		r = self.xmms.PlaylistSetNext (0, 1)
		r.wait ()
		if r.iserror () :
			print "Error: " + r.error()
			return
		return self.xmms.PlaybackNext ()

class cmd_prev (Command) :
	def func (self) :
		r = self.xmms.PlaylistSetNext (0, -1)
		r.wait ()
		if r.iserror () :
			print "Error: " + r.error()
			return
		return self.xmms.PlaybackNext ()


cmds = {"add" : cmd_add(),
	"play" : cmd_play(),
	"next" : cmd_next(),
	"prev" : cmd_prev(),
	"stop" : cmd_stop(),
	"pause" : cmd_pause(),
	"remove" : cmd_remove(),
	"list" : cmd_list()}

cmd = sys.argv[1]
arg = None
if (len (sys.argv) > 2) :
	arg = sys.argv[2]

if cmds.has_key (cmd) :
	command = cmds[cmd]
	if arg :
		r = command.func (arg)
	else :
		r = command.func ()

	if r :
		r.wait ()

	if command.islistfunc :
		command.listfunc (r.uintlist())


