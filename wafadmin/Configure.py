#! /usr/bin/env python
# encoding: utf-8

"Configuration system"

import os, types, imp, cPickle, md5, sys, re, inspect
import shlex
import Params, Environment, Runner, Build, Utils, libtool_config
from Params import error, fatal, warning

g_maxlen = 40
"""initial length of configuration messages"""

g_debug  = 0
"""enable/disable debug"""

g_stdincpath = ['/usr/include/', '/usr/local/include/']
"""standard include paths"""

g_stdlibpath = ['/usr/lib/', '/usr/local/lib/', '/lib']
"""standard library search paths"""

#####################
## Helper functions

def find_file(filename, path_list):
	"""find a file in a list of paths
@param filename: name of the file to search for
@param path_list: list of directories to search
@return: the first occurrence filename or '' if filename could not be found
"""
	if type(path_list) is types.StringType:
		lst = path_list.split()
	else:
		lst = path_list
	for directory in lst:
		if os.path.exists( os.path.join(directory, filename) ):
			return directory
	return ''

def find_file_ext(filename, path_list):
	"""find a file in a list of paths using fnmatch
@param filename: name of the file to search for
@param path_list: list of directories to search
@return: the first occurrence filename or '' if filename could not be found
"""
	import os, fnmatch;
	if type(path_list) is types.StringType:
		lst = path_list.split()
	else:
		lst = path_list
	for directory in lst:
		for path, subdirs, files in os.walk( directory ):
			for name in files:
				if fnmatch.fnmatch( name, filename ):
					return path
	return ''

def find_program_impl(lenv, filename, path_list=None, var=None):
	"""find a program in folders path_lst, and sets lenv[var]
@param lenv: environment
@param filename: name of the program to search for
@param path_list: list of directories to search for filename
@param var: environment value to be checked for in lenv or os.environ
@return: either the value that is referenced with [var] in lenv or os.environ
         or the first  occurrence filename or '' if filename could not be found
"""
	if not path_list:
		path_list = []
	elif type(path_list) is types.StringType:
		path_list = path_list.split()

	if var:
		if lenv[var]:
			return lenv[var]
		elif var in os.environ:
			lenv[var] = os.environ[var]
			return os.environ[var]

	if lenv['WINDOWS']: filename += '.exe'
	if not path_list: path_list = os.environ['PATH'].split(os.pathsep)
	for directory in path_list:
		if os.path.exists( os.path.join(directory, filename) ):
			ret = os.path.join(directory, filename)
			if var: lenv[var] = ret
			return ret
	return ''

###############
## ENUMERATORS

class enumerator_base:
	def __init__(self, conf):
		self.conf        = conf
		self.env         = conf.env
		self.define      = ''
		self.mandatory   = 0
		self.message     = ''

	def error(self):
		if self.message:
			fatal(self.message)
		else:
			fatal('A mandatory check failed. Make sure all dependencies are ok and can be found.')

	def update_hash(self, md5hash):
		classvars = vars(self)
		for (var, value) in classvars.iteritems():
			if callable(var):          continue
			if value == self:          continue
			if value == self.env:      continue
			if value == self.conf:     continue
			md5hash.update(str(value))

	def update_env(self, hashtable):
		# skip this if hashtable is only a string
		if not type(hashtable) is types.StringType:
			for name in hashtable.keys():
				self.env[name] = hashtable[name]

	def validate(self):
		try: self.names = self.names.split()
		except: pass

	def hash(self):
		m = md5.new()
		self.update_hash(m)
		return m.digest()

	def run_cache(self, retvalue):
		pass

	def run(self):
		self.validate()
		if not Params.g_options.nocache:
			newhash = self.hash()
			try:
				ret = self.conf.m_cache_table[newhash]
				self.run_cache(ret)
				return ret
			except KeyError:
				pass

		ret = self.run_test()

		if self.mandatory and not ret:
			self.error()

		if not Params.g_options.nocache:
			self.conf.m_cache_table[newhash] = ret
		return ret

	# Override this method, not run()!
	def run_test(self):
		return 0

class configurator_base(enumerator_base):
	def __init__(self, conf):
		enumerator_base.__init__(self, conf)
		self.uselib = ''

class program_enumerator(enumerator_base):
	def __init__(self,conf):
		enumerator_base.__init__(self, conf)

		self.name = ''
		self.path = []
		self.var  = None

	def error(self):
		errmsg = 'program %s cannot be found' % self.name
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)

	def run_cache(self, retval):
		self.conf.check_message('program %s (cached)' % self.name, '', retval, option=retval)
		if self.var: self.env[self.var] = retval

	def run_test(self):
		ret = find_program_impl(self.env, self.name, self.path, self.var)
		self.conf.check_message('program', self.name, ret, ret)
		if self.var: self.env[self.var] = ret
		return ret

class function_enumerator(enumerator_base):
	def __init__(self,conf):
		enumerator_base.__init__(self, conf)

		self.function      = ''
		self.define        = ''

		self.headers       = []
		self.header_code   = ''
		self.custom_code   = ''

		self.include_paths = []
		self.libs          = []
		self.lib_paths     = []

	def error(self):
		errmsg = 'function %s cannot be found' % self.function
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)

	def validate(self):
		if not self.define:
			self.define = self.function.upper()

	def run_cache(self, retval):
		self.conf.check_message('function %s (cached)' % self.function, '', retval, option='')
		self.conf.add_define(self.define, retval)

	def run_test(self):
		ret = 0 # not found

		oldlibpath = self.env['LIBPATH']
		oldlib = self.env['LIB']

		code = []
		code.append(self.header_code)
		code.append('\n')
		for header in self.headers:
			code.append('#include <%s>\n' % header)

		if self.custom_code:
			code.append('int main(){%s\nreturn 0;}\n' % self.custom_code)
		else:
			code.append('int main(){\nvoid *p;\np=(void*)(%s);\nreturn 0;\n}\n' % self.function)

		self.env['LIB'] = self.libs
		self.env['LIBPATH'] = self.lib_paths

		obj               = check_data()
		obj.code          = "\n".join(code)
		obj.includes      = self.include_paths
		obj.env           = self.env

		ret = int(self.conf.run_check(obj))
		self.conf.check_message('function %s' % self.function, '', ret, option='')

		self.conf.add_define(self.define, ret)

		self.env['LIB'] = oldlib
		self.env['LIBPATH'] = oldlibpath

		return ret

class library_enumerator(enumerator_base):
	"find a library in a list of paths"
	def __init__(self, conf):
		enumerator_base.__init__(self, conf)

		self.name = ''
		self.path = []
		self.code = 'int main() {return 0;}'
		self.uselib = '' # to set the LIB_NAME and LIBPATH_NAME
		self.nosystem = 0 # do not use standard lib paths
		self.want_message = 1

	def error(self):
		errmsg = 'library %s cannot be found' % self.name
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)

	def run_cache(self, retval):
		if self.want_message:
			self.conf.check_message('library %s (cached)' % self.name, '', retval, option=retval)
		self.update_env(retval)

	def validate(self):
		if not self.path:
			self.path = g_stdlibpath
		else:
			if not self.nosystem:
				self.path += g_stdlibpath

	def run_test(self):
		ret=''

		name = self.env['shlib_PREFIX']+self.name+self.env['shlib_SUFFIX']
		ret  = find_file(name, self.path)

		if not ret:
			name = self.env['staticlib_PREFIX']+self.name+self.env['staticlib_SUFFIX']
			ret  = find_file(name, self.path)

		if self.want_message:
			self.conf.check_message('library '+self.name, '', ret, option=ret)
		if self.uselib:
			self.env['LIB_'+self.uselib] = self.name
			self.env['LIBPATH_'+self.uselib] = ret

		return ret

class header_enumerator(enumerator_base):
	"find a header in a list of paths"
	def __init__(self,conf):
		enumerator_base.__init__(self, conf)

		self.name   = []
		self.path   = []
		self.define = []
		self.nosystem = 0
		self.want_message = 1

	def validate(self):
		if not self.path:
			self.path = g_stdincpath
		else:
			if not self.nosystem:
				self.path += g_stdincpath

	def error(self):
		errmsg = 'cannot find %s in %s' % (self.name, str(self.path))
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)

	def run_cache(self, retval):
		if self.want_message:
			self.conf.check_message('header %s (cached)' % self.name, '', retval, option=retval)
		if self.define: self.env[self.define] = retval

	def run_test(self):
		ret = find_file(self.name, self.path)
		if self.want_message:
			self.conf.check_message('header', self.name, ret, ret)
		if self.define: self.env[self.define] = ret
		return ret

## ENUMERATORS END
###################

###################
## CONFIGURATORS

class cfgtool_configurator(configurator_base):
	def __init__(self,conf):
		configurator_base.__init__(self, conf)

		self.uselib   = ''
		self.define   = ''
		self.binary   = ''

		self.tests    = {}

	def error(self):
		errmsg = '%s cannot be found' % self.binary
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)

	def validate(self):
		if not self.binary:
			raise ValueError, "no binary given in cfgtool!"
		if not self.uselib:
			raise ValueError, "no uselib given in cfgtool!"
		if not self.define and self.uselib:
			self.define = 'HAVE_'+self.uselib

		if not self.tests:
			self.tests['--cflags'] = 'CCFLAGS'
			self.tests['--cflags'] = 'CXXFLAGS'
			self.tests['--libs']   = 'LINKFLAGS'

	def run_cache(self, retval):
		if retval:
			self.update_env(retval)
			self.conf.add_define(self.define, 1)
		else:
			self.conf.add_define(self.define, 0)
		self.conf.check_message('config-tool %s (cached)' % self.binary, '', retval, option='')

	def run_test(self):
		retval = {}
		found = 1

		try:
			ret = os.popen('%s %s 2>/dev/null' % (self.binary, self.tests.keys()[0] )).close()
			if ret: raise ValueError, "error"

			for flag in self.tests:
				var = self.tests[flag] + '_' + self.uselib
				cmd = '%s %s 2>/dev/null' % (self.binary, flag)
				retval[var] = [os.popen(cmd).read().strip()]

			self.update_env(retval)
		except ValueError:
			retval = {}
			found = 0

		self.conf.add_define(self.define, found)
		self.conf.check_message('config-tool ' + self.binary, '', found, option = '')
		return retval

class pkgconfig_configurator(configurator_base):
	""" pkgconfig_configurator is a frontend to pkg-config variables:
	- name: name of the .pc file  (has to be set at least)
	- version: atleast-version to check for
	- path: override the pkgconfig path (PKG_CONFIG_PATH)
	- uselib: name that could be used in tasks with obj.uselib if not set uselib = upper(name)
	- define: name that will be used in config.h if not set define = HAVE_+uselib
	- variables: list of addional variables to be checked for, for example variables='prefix libdir'
"""
	def __init__(self, conf):
		configurator_base.__init__(self,conf)

		self.name        = '' # name of the .pc file
		self.version     = '' # version to check
		self.path        = '' # PKG_CONFIG_PATH
		self.uselib = '' # can be set automatically
		self.define = '' # can be set automatically
		self.binary      = '' # name and path for pkg-config

		# You could also check for extra values in a pkg-config file.
		# Use this value to define which values should be checked
		# and defined. Several formats for this value are supported:
		# - string with spaces to separate a list
		# - list of values to check (define name will be upper(uselib"_"value_name))
		# - a list of [value_name, override define_name]
		self.variables   = []

	def error(self):
		if self.version:
			errmsg = 'pkg-config cannot find %s >= %s' % (self.name, self.version)
		else:
			errmsg = 'pkg-config cannot find %s' % self.name
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)


	def validate(self):
		if not self.uselib:
			self.uselib = self.name.upper()
		if not self.define:
			self.define = 'HAVE_'+self.uselib

	def run_cache(self, retval):
		if self.version:
			self.conf.check_message('package %s >= %s (cached)' % (self.name, self.version), '', retval, option='')
		else:
			self.conf.check_message('package %s (cached)' % self.name, '', retval, option='')
		if retval:
			self.conf.add_define(self.define, 1)
		else:
			self.conf.add_define(self.define, 0)
		self.update_env(retval)

	def run_test(self):
		pkgpath = self.path
		pkgbin = self.binary
		uselib = self.uselib

		# check if self.variables is a string with spaces
		# to separate the variables to check for
		# if yes convert variables to a list
		if type(self.variables) is types.StringType:
			self.variables = str(self.variables).split()

		if not pkgbin:
			pkgbin = 'pkg-config'
		if pkgpath:
			pkgpath = 'PKG_CONFIG_PATH=$PKG_CONFIG_PATH:' + pkgpath
		pkgcom = '%s %s' % (pkgpath, pkgbin)

		retval = {}

		try:
			if self.version:
				cmd = "%s --atleast-version=%s \"%s\"" % (pkgcom, self.version, self.name)
				ret = os.popen(cmd).close()
				Params.debug("pkg-config cmd '%s' returned %s" % (cmd, ret))
				self.conf.check_message('package %s >= %s' % (self.name, self.version), '', not ret)
				if ret: raise ValueError, "error"
			else:
				cmd = "%s \"%s\"" % (pkgcom, self.name)
				ret = os.popen(cmd).close()
				Params.debug("pkg-config cmd '%s' returned %s" % (cmd, ret))
				self.conf.check_message('package %s' % (self.name), '', not ret)
				if ret:
					raise ValueError, "error"

			cflags_I = shlex.split(os.popen('%s --cflags-only-I \"%s\"' % (pkgcom, self.name)).read())
			cflags_other = shlex.split(os.popen('%s --cflags-only-other \"%s\"' % (pkgcom, self.name)).read())
			retval['CCFLAGS_'+uselib] = cflags_other
			retval['CXXFLAGS_'+uselib] = cflags_other
			retval['CPPPATH_'+uselib] = []
			for incpath in cflags_I:
				assert incpath[:2] == '-I' or incpath[:2] == '/I'
				retval['CPPPATH_'+uselib].append(incpath[2:]) # strip '-I' or '/I'

			#env['LINKFLAGS_'+uselib] = os.popen('%s --libs %s' % (pkgcom, self.name)).read().strip()
			# Store the library names:
			modlibs = os.popen('%s --libs-only-l \"%s\"' % (pkgcom, self.name)).read().strip().split()
			retval['LIB_'+uselib] = []
			for item in modlibs:
				retval['LIB_'+uselib].append( item[2:] ) #Strip '-l'

			# Store the library paths:
			modpaths = os.popen('%s --libs-only-L \"%s\"' % (pkgcom, self.name)).read().strip().split()
			retval['LIBPATH_'+uselib] = []
			for item in modpaths:
				retval['LIBPATH_'+uselib].append( item[2:] ) #Strip '-l'

			# Store only other:
			modother = os.popen('%s --libs-only-other \"%s\"' % (pkgcom, self.name)).read().strip().split()
			retval['LINKFLAGS_'+uselib] = []
			for item in modother:
				if str(item).endswith(".la"):
					la_config = libtool_config.libtool_config(item)
					libs_only_L = la_config.get_libs_only_L()
					libs_only_l = la_config.get_libs_only_l()
					for entry in libs_only_l:
						retval['LIB_'+uselib].append( entry[2:] ) #Strip '-l'
					for entry in libs_only_L:
						retval['LIBPATH_'+uselib].append( entry[2:] ) #Strip '-L'
				else:
					retval['LINKFLAGS_'+uselib].append( item ) #do not strip anything

			for variable in self.variables:
				var_defname = ''
				# check if variable is a list
				if (type(variable) is types.ListType):
					# is it a list of [value_name, override define_name] ?
					if len(variable) == 2 and variable[1]:
						# if so use the overrided define_name as var_defname
						var_defname = variable[1]
					# convert variable to a string that name the variable to check for.
					variable = variable[0]

				# if var_defname was not overrided by the list containing the define_name
				if not var_defname:
					var_defname = uselib + '_' + variable.upper()

				retval[var_defname] = os.popen('%s --variable=%s \"%s\"' % (pkgcom, variable, self.name)).read().strip()

			self.conf.add_define(self.define, 1)
			self.update_env(retval)
		except ValueError:
			retval = {}
			self.conf.add_define(self.define, 0)

		return retval

class test_configurator(configurator_base):
	def __init__(self, conf):
		configurator_base.__init__(self, conf)
		self.name = ''
		self.code = ''
		self.flags = ''
		self.define = ''
		self.uselib = ''
		self.want_message = 0

	def error(self):
		errmsg = 'test program would not run'
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)

	def run_cache(self, retval):
		if self.want_message:
			self.conf.check_message('custom code (cached)', '', 1, option=retval['result'])

	def validate(self):
		if not self.code:
			fatal('test configurator needs code to compile and run!')

	def run_test(self):
		obj = check_data()
		obj.code = self.code
		obj.env  = self.env
		obj.uselib = self.uselib
		obj.flags = self.flags
		obj.execute = 1
		ret = self.conf.run_check(obj)

		if self.want_message:
			if ret: data = ret['result']
			else: data = ''
			self.conf.check_message('custom code', '', ret, option=data)

		return ret

class library_configurator(configurator_base):
	def __init__(self,conf):
		configurator_base.__init__(self,conf)

		self.name = ''
		self.path = []
		self.define = ''
		self.uselib = ''

		self.code = 'int main(){ return 0; }'

	def error(self):
		errmsg = 'library %s cannot be linked' % self.name
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)

	def run_cache(self, retval):
		self.conf.check_message('library %s (cached)' % self.name, '', retval)
		if retval:
			self.update_env(retval)
			self.conf.add_define(self.define, 1)
		else:
			self.conf.add_define(self.define, 0)

	def validate(self):
		if not self.path:
			self.path = ['/usr/lib/', '/usr/local/lib', '/lib']

		if not self.uselib:
			self.uselib = self.name.upper()
		if not self.define:
			self.define = 'HAVE_'+self.uselib

		if not self.uselib:
			fatal('uselib is not defined')
		if not self.code:
			fatal('library enumerator must have code to compile')

	def run_test(self):
		oldlibpath = self.env['LIBPATH']
		oldlib = self.env['LIB']

		# try the enumerator to find the correct libpath
		test = self.conf.create_library_enumerator()
		test.name = self.name
		test.want_message = 0
		test.path = self.path
		test.env = self.env
		ret = test.run()

		if ret:
			self.env['LIBPATH_'+self.uselib] = ret

		self.env['LIB_'+self.uselib] = self.name

		val = {}

		#self.env['LIB'] = self.name
		#self.env['LIBPATH'] = self.lib_paths

		obj               = check_data()
		obj.code          = self.code
		obj.env           = self.env
		obj.uselib        = self.uselib
		obj.libpath       = self.path

		ret = int(self.conf.run_check(obj))
		self.conf.check_message('library %s' % self.name, '', ret)

		self.conf.add_define(self.define, ret)

		if ret:
			val['LIBPATH_'+self.uselib] = self.env['LIBPATH_'+self.uselib]
			val['LIB_'+self.uselib] = self.env['LIB_'+self.uselib]
			val[self.define] = ret

		self.env['LIB'] = oldlib
		self.env['LIBPATH'] = oldlibpath

		if not ret: return {}
		return val

class framework_configurator(configurator_base):
	def __init__(self,conf):
		configurator_base.__init__(self,conf)

		self.name = ''
		self.custom_code = ''
		self.code = 'int main() {return 0;}'

		self.define = '' # HAVE_something

		self.path = []
		self.uselib = ''
		self.remove_dot_h = False

	def error(self):
		errmsg = 'framework %s cannot be found via compiler, try pass -F' % self.name
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)

	def validate(self):
		if not self.uselib:
			self.uselib = self.name.upper()
		if not self.define:
			self.define = 'HAVE_'+self.uselib
		if not self.code:
			self.code = "#include <%s>\nint main(){return 0;}\n"
		if not self.uselib:
			self.uselib = self.name.upper()

	def run_cache(self, retval):
		self.conf.check_message('framework %s (cached)' % self.name, '', retval)
		self.update_env(retval)
		if retval:
			self.conf.add_define(self.define, 1)
		else:
			self.conf.add_define(self.define, 0)

	def run_test(self):
		oldlkflags = []
		oldccflags = []
		oldcxxflags = []

		oldlkflags += self.env['LINKFLAGS']
		oldccflags += self.env['CCFLAGS']
		oldcxxflags += self.env['CXXFLAGS']

		code = []
		if self.remove_dot_h:
			code.append('#include <%s/%s>\n' % (self.name, self.name))
		else:
			code.append('#include <%s/%s.h>\n' % (self.name, self.name))

		code.append('int main(){%s\nreturn 0;}\n' % self.custom_code)

		linkflags = []
		linkflags += ['-framework', self.name]
		linkflags += ['-F%s' % p for p in self.path]
		cflags = ['-F%s' % p for p in self.path]

		myenv = self.env.copy()
		myenv['LINKFLAGS'] += linkflags

		obj               = check_data()
		obj.code          = "\n".join(code)
		obj.env           = myenv
		obj.uselib        = self.uselib
		obj.flags		  += " ".join (cflags)

		ret = int(self.conf.run_check(obj))
		self.conf.check_message('framework %s' % self.name, '', ret, option='')
		self.conf.add_define(self.define, ret)

		val = {}
		if ret:
			val["LINKFLAGS_" + self.uselib] = linkflags
			val["CCFLAGS_" + self.uselib] = cflags
			val["CXXFLAGS_" + self.uselib] = cflags
			val[self.define] = ret

		self.env['LINKFLAGS'] = oldlkflags
		self.env['CCFLAGS'] = oldccflags
		self.env['CXXFLAGS'] = oldcxxflags

		self.update_env(val)

		return val

class header_configurator(configurator_base):
	def __init__(self,conf):
		configurator_base.__init__(self,conf)

		self.name = ''
		self.path = []
		self.header_code = ''
		self.custom_code = ''
		self.code = 'int main() {return 0;}'

		self.define = '' # HAVE_something

		self.libs = []
		self.lib_paths = []
		self.uselib = ''

	def error(self):
		errmsg = 'header %s cannot be found via compiler' % self.name
		if self.message: errmsg += '\n%s' % self.message
		fatal(errmsg)

	def validate(self):
		#try: self.names = self.names.split()
		#except: pass
		if not self.define:
			if self.name:
				reg=re.compile('[/\\\\.-]', re.M)
				self.define = 'HAVE_'+'_'.join(reg.split(self.name)).upper()
			elif self.uselib: self.define = 'HAVE_'+self.uselib

		if not self.code:
			self.code = "#include <%s>\nint main(){return 0;}\n"
		if not self.define:
			fatal('no define given')

	def run_cache(self, retvalue):
		self.conf.check_message('header %s (cached)' % self.name, '', retvalue)
		if retvalue:
			self.update_env(retvalue)
			self.conf.add_define(self.define, 1)
		else:
			self.conf.add_define(self.define, 0)

	def run_test(self):
		ret = {} # not found

		oldlibpath = self.env['LIBPATH']
		oldlib = self.env['LIB']

		# try the enumerator to find the correct includepath
		if self.uselib:
			test = self.conf.create_header_enumerator()
			test.name = self.name
			test.want_message = 0
			test.path = self.path
			test.env = self.env
			ret = test.run()

			if ret:
				self.env['CPPPATH_'+self.uselib] = ret

		code = []
		code.append(self.header_code)
		code.append('\n')
		code.append('#include <%s>\n' % self.name)

		code.append('int main(){%s\nreturn 0;}\n' % self.custom_code)

		self.env['LIB'] = self.libs
		self.env['LIBPATH'] = self.lib_paths

		obj               = check_data()
		obj.code          = "\n".join(code)
		obj.includes      = self.path
		obj.env           = self.env
		obj.uselib        = self.uselib

		ret = int(self.conf.run_check(obj))
		self.conf.check_message('header %s' % self.name, '', ret, option='')

		self.conf.add_define(self.define, ret)

		self.env['LIB'] = oldlib
		self.env['LIBPATH'] = oldlibpath

		val = {}
		if ret:
			val['CPPPATH_'+self.uselib] = self.env['CPPPATH_'+self.uselib]
			val[self.define] = ret

		if not ret: return {}
		return val

# CONFIGURATORS END
####################

class check_data:
	def __init__(self):

		self.env           = '' # environment to use

		self.code          = '' # the code to execute

		self.flags         = '' # the flags to give to the compiler

		self.uselib        = '' # uselib
		self.includes      = '' # include paths

		self.function_name = '' # function to check for

		self.lib           = []
		self.libpath       = [] # libpath for linking

		self.define   = '' # define to add if run is successful

		self.header_name   = '' # header name to check for

		self.execute       = 0  # execute the program produced and return its output
		self.options       = '' # command-line options

class Configure:
	def __init__(self, env=None, blddir='', srcdir=''):

		self.env       = None
		self.m_envname = ''

		self.m_blddir = blddir
		self.m_srcdir = srcdir

		self.m_allenvs = {}
		self.defines = {}
		self.configheader = 'config.h'
		self.cwd = os.getcwd()

		self.setenv('default')

		self.m_cache_table = {}

		self.lastprog = ''

		try:
			file = open(Utils.join_path(Params.g_homedir, '.wafcache', 'runs-%s.txt' % self._cache_platform()), 'rb')
			self.m_cache_table = cPickle.load(file)
			file.close()
		except:
			pass

		self._a=0
		self._b=0
		self._c=0
		self._quiet=0

		self.hash=0
		self.files=[]

	def set_env_name(self, name, env):
		"add a new environment called name"
		self.m_allenvs[name] = env
		return env

	def retrieve(self, name, fromenv=None):
		"retrieve an environment called name"
		try:
			env = self.m_allenvs[name]
			if fromenv: warning("The environment %s may have been configured already" % name)
			return env
		except:
			env = Environment.Environment()
			self.m_allenvs[name] = env
			return env

	def check_tool(self, input, tooldir=None):
		"load a waf tool"
		lst = Utils.to_list(input)
		ret = True
		if tooldir: tooldir = Utils.to_list(tooldir)
		for i in lst:
			try:
				file,name,desc = imp.find_module(i, tooldir)
			except:
				error("no tool named '%s' found" % i)
				return 0
			module = imp.load_module(i,file,name,desc)
			ret = ret and int(module.detect(self))
			self.env.append_value('tools', {'tool':i, 'tooldir':tooldir})
		return ret

	def setenv(self, name):
		"enable the environment called name"
		self.env     = self.retrieve(name)
		self.envname = name

	def find_program(self, program_name, path_list=[], var=None):
		"wrapper provided for convenience"
		ret = find_program_impl(self.env, program_name, path_list, var)
		self.check_message('program', program_name, ret, ret)
		return ret

	def store(self, file=''):
		"save the config results into the cache file"
		try:
			os.makedirs(Params.g_cachedir)
		except OSError:
			pass

		if not self.m_allenvs:
			fatal("nothing to store in Configure !")
		for key in self.m_allenvs:
			tmpenv = self.m_allenvs[key]
			tmpenv.store(os.path.join(Params.g_cachedir, key+'.cache.py'))

	def check_pkg(self, modname, destvar='', vnum='', pkgpath='', pkgbin=''):
		"wrapper provided for convenience"
		pkgconf = self.create_pkgconfig_configurator()

		if not destvar: destvar = modname.upper()

		pkgconf.uselib = destvar
		pkgconf.name = modname
		pkgconf.version = vnum
		pkgconf.path = pkgpath
		pkgconf.binary = pkgbin
		return pkgconf.run()

	def sub_config(self, dir):
		"executes the configure function of a wscript module"
		current = self.cwd

		self.cwd = os.path.join(self.cwd, dir)
		cur = os.path.join(self.cwd, 'wscript')

		try:
			mod = Utils.load_module(cur)
		except:
			msg = "no module or function configure was found in wscript\n[%s]:\n * make sure such a function is defined \n * run configure from the root of the project"
			fatal(msg % self.cwd)

		# TODO check
		#if not 'configure' in mod:
		#	fatal('the module has no configure function')

		ret = mod.configure(self)
		if Params.g_autoconfig:
			self.hash = Params.hash_sig_weak(self.hash, inspect.getsource(mod.configure).__hash__())
			self.files.append(os.path.abspath(cur))
		self.cwd = current
		return ret

	def cleanup(self):
		"called on shutdown"
		try:
			dir = Utils.join_path(Params.g_homedir, '.wafcache')
			try:
				os.makedirs(dir)
			except:
				pass

			file = open(Utils.join_path(Params.g_homedir, '.wafcache', 'runs-%s.txt' % self._cache_platform()), 'wb')
			cPickle.dump(self.m_cache_table, file)
			file.close()
		except:
			raise
			pass

	class IntDefine(int):
		"""Special int subclass to denote a literal integer being defined"""
		pass

	def add_define(self, define, value, quote=-1, comment=''):
		"""store a single define and its state into an internal list
		   for later writing to a config header file"""

		tbl = self.env['defines']
		if not tbl: tbl = {}

		# the user forgot to tell if the value is quoted or not
		if quote < 0:
			if type(value) is types.StringType:
				tbl[define] = '"%s"' % str(value)
			else:
				tbl[define] = value
		elif not quote:
			if isinstance(value, int):
				tbl[define] = self.IntDefine(value)
			else:
				tbl[define] = value
		else:
			tbl[define] = '"%s"' % str(value)

		if not define: raise "define must be .. defined"

		# add later to make reconfiguring faster
		self.env['defines'] = tbl
		self.env[define] = value

	def is_defined(self, define):
		try: return self.env['defines'].has_key(define)
		except: return None

	def get_define(self, define):
		"get the value of a previously stored define"
		try: return self.env['define']
		except: return 0

	def write_config_header(self, configfile='config.h', env=''):
		"save the defines into a file"
		if configfile == '': configfile = self.configheader

		lst=Utils.split_path(configfile)
		base = lst[:-1]

		if not env: env = self.env
		base = [self.m_blddir, env.variant()]+base
		dir = Utils.join_path(*base)
		try:
			os.makedirs(dir)
		except OSError:
			pass
		
		dir = Utils.join_path(dir, lst[-1])

		# remember config files - do not remove them on "waf clean"
		self.env.append_value('waf_config_files', os.path.abspath(dir))

		inclusion_guard_name = '_%s_WAF' % (Utils.path_to_preprocessor_name(configfile),)

		dest = open(dir, 'w')
		dest.write('/* configuration created by waf */\n')
		dest.write('#ifndef %s\n#define %s\n\n' % (inclusion_guard_name, inclusion_guard_name))

		# yes, this is special
		if not configfile in self.env['dep_files']:
			self.env['dep_files'] += [configfile]
		for key, value in env['defines'].iteritems():
			if value is None:
				dest.write('#define %s\n' % key)
			elif isinstance(value, self.IntDefine):
				dest.write('#define %s %i\n' % (key, value))
			elif value:
				dest.write('#define %s %s\n' % (key, value))
				#if addcontent:
				#	dest.write(addcontent);
			else:
				dest.write('/* #undef %s */\n' % key)
		dest.write('\n#endif /* %s */\n' % (inclusion_guard_name,))
		dest.close()

	def set_config_header(self, header):
		"set a config header file"
		self.configheader = header

	def check_message(self,type,msg,state,option=''):
		"print an checking message. This function is used by other checking functions"
		sr = 'Checking for ' + type + ' ' + msg

		lst = []
		lst.append(sr)

		global g_maxlen
		dist = len(sr)
		if dist > g_maxlen:
			g_maxlen = dist+1

		if dist < g_maxlen:
			diff = g_maxlen - dist
			while diff > 0:
				lst.append(' ')
				diff -= 1

		lst.append(':')
		print ''.join(lst),

		p = Params.pprint
		if state: p('GREEN', 'ok ' + option)
		else: p('YELLOW', 'not found')

	def check_message_custom(self,type,msg,custom,option=''):
		"""print an checking message. This function is used by other checking functions"""
		sr = 'Checking for ' + type + ' ' + msg

		lst = []
		lst.append(sr)

		global g_maxlen
		dist = len(sr)
		if dist > g_maxlen:
			g_maxlen = dist+1

		if dist < g_maxlen:
			diff = g_maxlen - dist
			while diff > 0:
				lst.append(' ')
				diff -= 1

		lst.append(':')
		print ''.join(lst),

		p = Params.pprint
		p('CYAN', custom)

	def hook(self, func):
		"attach the function given as input as new method"
		setattr(self.__class__, func.__name__, func)

	def mute_logging(self):
		"mutes the output temporarily"
		if Params.g_options.verbose: return
		# store the settings
		(self._a,self._b,self._c) = Params.get_trace()
		self._quiet = Runner.g_quiet
		# then mute
		if not g_debug:
			Params.set_trace(0,0,0)
			Runner.g_quiet = 1

	def restore_logging(self):
		"see mute_logging"
		if Params.g_options.verbose: return
		# restore the settings
		if not g_debug:
			Params.set_trace(self._a,self._b,self._c)
			Runner.g_quiet = self._quiet

	def create(self, enumerator = None, configurator = None):
		# Only one of these can be set (xor)
		if (enumerator and configurator) or (not enumerator and not configurator):
			raise KeyError, "either enumerator or configurator has to be set (not both)"

		try:
			if enumerator:
				return globals()["%s_enumerator" % enumerator](self)
			elif configurator:
				return globals()["%s_configurator" % configurator](self)
		except:
			pass

	def __getattr__(self,attr):
		def creator():
			return globals()[attr[7:]](self)
		if attr.startswith("create_"):
			return creator
		else:
			raise AttributeError, attr

	def pkgconfig_fetch_variable(self,pkgname,variable,pkgpath='',pkgbin='',pkgversion=0,env=None):
		if not env: env=self.env

		if not pkgbin: pkgbin='pkg-config'
		if pkgpath: pkgpath='PKG_CONFIG_PATH=$PKG_CONFIG_PATH:'+pkgpath
		pkgcom = '%s %s' % (pkgpath, pkgbin)
		if pkgversion:
			ret = os.popen("%s --atleast-version=%s %s" % (pkgcom, pkgversion, pkgname)).close()
			self.conf.check_message('package %s >= %s' % (pkgname, pkgversion), '', not ret)
			if ret:
				return '' # error
		else:
			ret = os.popen("%s %s" % (pkgcom, pkgname)).close()
			self.check_message('package %s ' % (pkgname), '', not ret)
			if ret:
				return '' # error

		return os.popen('%s --variable=%s %s' % (pkgcom, variable, pkgname)).read().strip()


	def run_check(self, obj, build_type = 'program', force_compiler = None):
		"""compile, link and run if necessary
@param obj: data of type check_data
@param build_type: could be program (default), shlib or staticlib
@param force_compiler: could be None (default), cc or cpp
@return: (False if a error during build happens) or ( (True if build ok) or 
(a {'result': ''} if execute was set))
"""
		# first make sure the code to execute is defined
		if not obj.code:
			error('run_check: no code to process in check')
			raise

		# create a small folder for testing
		dir = os.path.join(self.m_blddir, '.wscript-trybuild')

		# if the folder already exists, remove it
		for (root, dirs, filenames) in os.walk(dir):
			for f in list(filenames):
				os.remove(os.path.join(root, f))

		bdir = os.path.join( dir, '_testbuild_')
		try:
			os.makedirs(dir)
		except OSError:
			pass
		try:
			os.makedirs(bdir)
		except OSError:
			pass

		dest=open(os.path.join(dir, 'test.c'), 'w')
		dest.write(obj.code)
		dest.close()

		if obj.env: env = obj.env
		else: env = self.env.copy()

		# very important
		Utils.reset()

		back=os.path.abspath('.')

		bld = Build.Build()
		bld.m_allenvs.update(self.m_allenvs)
		bld.m_allenvs['default'] = env
		bld._variants=bld.m_allenvs.keys()
		bld.load_dirs(dir, bdir, isconfigure=1)


		for t in env['tools']: env.setup(**t)

		os.chdir(dir)

		# not sure yet when to call this:
		#bld.rescan(bld.m_srcnode)

		if (not force_compiler and env['CXX']) or force_compiler == "cpp":
			import cpp
			o=cpp.cppobj(build_type)
		else:
			import cc
			o=cc.ccobj(build_type)
		o.source   = 'test.c'
		o.target   = 'testprog'
		o.uselib   = obj.uselib
		o.cppflags = obj.flags
		o.includes = obj.includes

		# compile the program
		self.mute_logging()
		ret = bld.compile()
		self.restore_logging()

		# keep the name of the program to execute
		if obj.execute:
			lastprog = o.m_linktask.m_outputs[0].abspath(o.env)

		#if runopts is not None:
		#	ret = os.popen(obj.m_linktask.m_outputs[0].abspath(obj.env)).read().strip()

		os.chdir(back)
		Utils.reset()

		# if we need to run the program, try to get its result
		if obj.execute:
			if ret: return not ret
			try:
				data = os.popen('"%s"' %lastprog).read().strip()
				ret = {'result': data}
				return ret
			except:
				raise
				pass
		
		return not ret

	def _cache_platform(self):
		m = md5.new()
		m.update(sys.platform)
		return m.hexdigest()


