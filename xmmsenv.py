import SCons

import os;
import sys;



class XmmsEnvironment(SCons.Environment.Environment):
	pass
	def __init__(self, options=None, **kw):
		self.sys = os.popen("uname").read().strip()
		SCons.Environment.Environment.__init__(self, options=options)
		self.flag_groups = {}
		apply(self.Replace, (), kw)
		self.plugins = []
		self.install_prefix=self['PREFIX']
		self.pluginpath=self.install_prefix + "/lib/xmms/"
		self.binpath=self.install_prefix + "/bin/"
		self.libpath=self.install_prefix + "/lib/"
		self.sysconfdir=self['SYSCONFDIR']
		self.installdir=self['INSTALLDIR']
		self.mandir=self['MANDIR']
		self.Append(CPPPATH=['#src/include'])
		self['ENV']['TERM']=os.environ['TERM']
		self.Append(CPPFLAGS=['-DPKGLIBDIR=\\"'+self.pluginpath+'\\"'])
		self.Append(CPPFLAGS=['-DSYSCONFDIR=\\"'+self.sysconfdir+'\\"'])
		self.Append(LIBPATH=['/usr/lib'])
		self.Append(LIBPATH=['/usr/local/lib'])
		self.Append(CPPFLAGS=['-I/usr/local/include'])

		if self.sys == 'Linux':
			self.Append(CPPFLAGS='-DXMMS_OS_LINUX')
		elif self.sys == 'Darwin':
			self.Append(CPPFLAGS='-DXMMS_OS_DARWIN')
			self['SHLIBSUFFIX'] = '.dylib'
			self.Append(LIBPATH=['/sw/lib'])
			self.Append(CPPFLAGS=['-I/sw/include'])
		elif self.sys == 'OpenBSD':
			self.Append(CPPFLAGS='-DXMMS_OS_OPENBSD')

	def XmmsConfig(self,source):
		self.Install(self.installdir+self.sysconfdir, source)

	def XmmsManual(self,source):
		self.Install(self.installdir+self.mandir, source)

	def XmmsPlugin(self,target,source):
		if self.sys == 'Darwin':
			self['SHLINKFLAGS'] = '$LINKFLAGS -bundle -flat_namespace -undefined suppress'
		self.SharedLibrary(target, source)
		self.Install(self.installdir+self.pluginpath,self['LIBPREFIX']+target+self['SHLIBSUFFIX'])

	def XmmsProgram(self,target,source):
		self.Program(target,source)
		self.Install(self.installdir+self.binpath,target)

	def XmmsPython(self,target,source):
		if self.sys == 'Darwin':
			self['SHLINKFLAGS'] = '$LINKFLAGS -bundle -flat_namespace -undefined suppress'

		self.SharedLibrary (target, source, SHLIBPREFIX='', SHLIBSUFFIX=".so")
		self.Install(self.installdir+self.libpath, target+".so")

	def XmmsLibrary(self,target,source):
		if self.sys == 'Darwin':
			self['SHLINKFLAGS'] = '$LINKFLAGS -dynamiclib'

		self.SharedLibrary(target, source)
		self.Install(self.installdir+self.libpath, self['LIBPREFIX']+target+self['SHLIBSUFFIX'])

	
	def AddFlagsToGroup(self, group, flags):
		excluded = self['EXCLUDE'].split()
		try :
			i = excluded.index(group)
			print "Skipping " + group
		except :
			if self.flag_groups.has_key(group) :
				self.flag_groups[group] += flags
			else:
				self.flag_groups[group] = flags


	def HasGroup(self,group):
		return self.flag_groups.has_key(group)

	def CheckAndAddFlagsToGroupFromLibTool(self, group, ltfile):
		file_found = 0
		msg = "Checking for "+ltfile+"... "
		for path in self['LIBPATH']:
			if os.path.exists(os.path.join(path, ltfile)):
				file_found = 1
				flags = ""
				f=open(os.path.join(path, ltfile))
				for line in f.readlines():
					if line[0:6] == 'dlname':
						flags += " -l"+line[11:line.index('.so')]
					elif line[0:6] == 'libdir':
						flags += " -L"+line[8:-2]
				print msg + "yes"
		if file_found == 0:
			print msg + "no"
		else:
			self.AddFlagsToGroup(group,flags)

	def CheckAndAddFlagsToGroup(self, group, cmd, fail=0):
		print "Running " + cmd
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

	def CheckLibAndAddFlagsToGroup(self, group, library, function, depends="", fail=0):
		test_env = self.Copy()
		if depends != "":
			if not self.HasGroup(depends):
				return 0
			test_env.AddFlagsFromGroup(depends)

		my_conf = SCons.SConf.SConf( test_env )
		if my_conf.CheckLib(library, function, 0):
			self.AddFlagsToGroup(group, '-l'+library)
			my_conf.Finish()
			return 1
		elif fail != 0 :
			print "Could not find "+group+". Aborting!"
			sys.exit(1)
			
		my_conf.Finish()
		return 0

	def CheckProgramAndAddFlagsToGroup (self, group, program) :
		test_env = self.Copy ()
		my_conf = SCons.SConf.SConf (test_env)
		print "Checking for program " + program
		(s, o) = my_conf.TryAction (program)
		if (s) :
		    self.AddFlagsToGroup (group, "yes")
		    my_conf.Finish ()
		    return 1
		my_conf.Finish ()
		return 0

	def ParseConfigFlagsString(self, flags ):
		"""We want our own ParseConfig, that supports some more
		flags, and that takes the argument as a string"""

		params = self.Split(flags)
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
				elif arg[1:] == 'framework':
					self.Append( LINKFLAGS = [ arg ] )
					self.Append( LINKFLAGS = [ params[i+1] ] )
					i = i + 1
				elif arg[1:5] == 'arch':
					i = i + 1
		    		else:
					print 'garbage in flags: ' + arg
					sys.exit(1)
			elif arg[:3] == 'yes' :
				i = i + 3
				pass
			elif switch == '/':
				if arg[-3:] == '.la':
# Ok, this is a libtool file, someday maybe we should parse it.
# Hate this shit. This is the reason we switch from autobajs.
# Please do the same!
					print "I have found some autobajs in " + arg + ". I'm just guessing here."
					self.Append( LINKFLAGS = [ arg[:-3] + '.a' ] )
			else:
				print 'garbage error: ' + arg

			i = i + 1

