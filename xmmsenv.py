import SCons

import os;
import sys;

class XmmsEnvironment(SCons.Environment.Environment):
	pass
	def __init__(self, **kw):
		SCons.Environment.Environment.__init__(self)
		self.flag_groups = {}
		apply(self.Replace, (), kw)

	def XmmsPlugin(self,target,source):
		self.SharedLibrary(target, source)
		self.Install("/usr/local/lib/xmms/",self['LIBPREFIX']+target+self['SHLIBSUFFIX'])

	def AddFlagsToGroup(self, group, flags):
		self.flag_groups[group] = flags

	def HasGroup(self,group):
		return self.flag_groups.has_key(group)

	def CheckAndAddFlagsToGroup(self, group, cmd, fail=0):
		res = os.popen(cmd).read().strip()
		if res == "":
			if fail != 0:
				print "Could not find "+group+". Aborting!"
				sys.exit(1)
			return 0
		self.AddFlagsToGroup(group, res)
		return 1

	def AddFlagsFromGroup(self, group):
		self.ParseConfigFlagsString(self.flag_groups[group])

	def ParseConfigFlagsString(self, flags ):
		"""We want our own ParseConfig, that supports some more
		flags, and that takes the argument as a string"""

		params = SCons.Util.Split(flags)
		i = 0
		while( i < len( params ) ):
			arg = params[i]
			switch = arg[0:1]
			opt = arg[1:2]

			if switch == '-':
				if opt == 'L':
					self.Append( LIBPATH = [ arg[2:] ] )
	    			elif opt == 'l':
					self.Append( LIBS = [ arg[2:] ] )
		    		elif opt == 'I':
					self.Append( CPPPATH = [ arg[2:] ] )
		    		elif opt == 'D':
					self.Append( CPPFLAGS = [ arg ] )
	    			elif arg[1:] == 'pthread':
					self.Append( LINKFLAGS = [ arg ] )
					self.Append( CPPFLAGS = [ arg ] )
	    			elif arg[1:4] == 'Wl,':
					self.Append( LINKFLAGS = [ arg ] )
		    		else:
					print 'garbage in flags: ' + arg
					sys.xit(1)
			else:
				print 'garbage error: ' + arg

			i = i + 1

