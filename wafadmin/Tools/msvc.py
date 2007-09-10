#! /usr/bin/env python
# encoding: utf-8
# Carlos Rafael Giani, 2006 (dv)
# Tamas Pal, 2007 (folti)
# Visual C support - beta, needs more testing

import os, sys
import re, os.path, string
import optparse
import Utils, Action, Params, Object, Runner
from Params import debug, error, fatal, warning, set_globals

import ccroot
from ccroot import read_la_file
from os.path import exists

set_globals('EXT_RC_C','.rc')

def resource_compiler(self, node):
	"""Compiler hook for compiling res files out of rc files. These resource
	files contains icons, forms information and generic executable informations
	(like binary version product number and so on for windows executables.
	The res file created by rc.exe but linked to the binary as an object."""
	rctask=self.create_task('rc', nice=101)
	rctask.set_inputs(node)
	newnode=node.change_ext('.res')
	rctask.set_outputs([newnode])
	# FIXME: currently it works, but fails to add a the compiled file to
	# (cc|cpp)_link's reporting line ([<step>] * cpp_link ...)
	self.p_compiletasks.append(rctask)

def msvc_linker(task):
	"""Special linker for MSVC with support for embedding manifests into DLL's
	and executables compiled by Visual Studio 2005 or probably later. Without
	the manifest file, the binaries are unusable.
	See: http://msdn2.microsoft.com/en-us/library/ms235542(VS.80).aspx
	Problems with this tool: it is always called whether MSVC creates manifests or not."""
	e=task.m_env
	linker=e['LINK_CXX']
	srcf=e['CPPLNK_SRC_F']
	trgtf=e['CPPLNK_TGT_F']
	if not linker:
		linker=e['LINK_CC']
		srcf=e['CCLNK_SRC_F']
		trgtf=e['CCLNK_TGT_F']
	linkflags=e.get_flat('LINKFLAGS')
	libdirs=e.get_flat('_LIBDIRFLAGS')
	libs=e.get_flat('_LIBFLAGS')

	subsystem=''
	if task.m_subsystem:
		subsystem='/subsystem:%s' % task.m_subsystem
	outfile=task.m_outputs[0].bldpath(e)
	manifest=outfile+'.manifest'

	objs=" ".join(['"%s"' % a.abspath(e) for a in task.m_inputs])

	cmd="%s %s %s%s %s%s %s %s %s" % (linker,subsystem,srcf,objs,trgtf,outfile, linkflags, libdirs,libs)
	ret=Runner.exec_command(cmd)
	if ret: return ret
	if os.path.exists(manifest):
		debug('manifesttool', 'msvc')
		mtool=task.m_env['MT']
		if not mtool:
			return 0
		mode=''
		# embedding mode. Different for EXE's and DLL's.
		# see: http://msdn2.microsoft.com/en-us/library/ms235591(VS.80).aspx
		if task.m_type == 'program':
			mode='1'
		elif task.m_type == 'shlib':
			mode='2'

		debug('embedding manifest','msvcobj')
		flags=task.m_env['MTFLAGS']
		if flags:
			flags=string.join(flags,' ')
		else:
			flags=''

		cmd='%s %s -manifest "%s" -outputresource:"%s";#%s' % (mtool, flags,
			manifest, outfile, mode)
		ret=Runner.exec_command(cmd)
	return ret

g_msvc_type_vars=['CCFLAGS', 'CXXFLAGS', 'LINKFLAGS', 'obj_ext']

# importlibs provided by MSVC/Platform SDK. Do NOT search them....
nm = """
aclui activeds ad1 adptif adsiid advapi32 asycfilt authz bhsupp bits bufferoverflowu cabinet
cap certadm certidl ciuuid clusapi comctl32 comdlg32 comsupp comsuppd comsuppw comsuppwd comsvcs
credui crypt32 cryptnet cryptui d3d8thk daouuid dbgeng dbghelp dciman32 ddao35 ddao35d
ddao35u ddao35ud delayimp dhcpcsvc dhcpsapi dlcapi dnsapi dsprop dsuiext dtchelp
faultrep fcachdll fci fdi framedyd framedyn gdi32 gdiplus glauxglu32 gpedit gpmuuid
gtrts32w gtrtst32hlink htmlhelp httpapi icm32 icmui imagehlp imm32 iphlpapi iprop
kernel32 ksguid ksproxy ksuser libcmt libcmtd libcpmt libcpmtd loadperf lz32 mapi
mapi32 mgmtapi minidump mmc mobsync mpr mprapi mqoa mqrt msacm32 mscms mscoree
msdasc msimg32 msrating mstask msvcmrt msvcurt msvcurtd mswsock msxml2 mtx mtxdm
netapi32 nmapinmsupp npptools ntdsapi ntdsbcli ntmsapi ntquery odbc32 odbcbcp
odbccp32 oldnames ole32 oleacc oleaut32 oledb oledlgolepro32 opends60 opengl32
osptk parser pdh penter pgobootrun pgort powrprof psapi ptrustm ptrustmd ptrustu
ptrustud qosname rasapi32 rasdlg rassapi resutils riched20 rpcndr rpcns4 rpcrt4 rtm
rtutils runtmchk scarddlg scrnsave scrnsavw secur32 sensapi setupapi sfc shell32
shfolder shlwapi sisbkup snmpapi sporder srclient sti strsafe svcguid tapi32 thunk32
traffic unicows url urlmon user32 userenv usp10 uuid uxtheme vcomp vcompd vdmdbg
version vfw32 wbemuuid  webpost wiaguid wininet winmm winscard winspool winstrm
wintrust wldap32 wmiutils wow32 ws2_32 wsnmp32 wsock32 wst wtsapi32 xaswitch xolehlp
"""
g_msvc_systemlibs={}
for x in nm.split(): g_msvc_systemlibs[x] = 1

g_msvc_flag_vars = [
'STATICLIB', 'LIB', 'LIBPATH', 'LINKFLAGS', 'RPATH', 'INCLUDE',
'CXXFLAGS', 'CCFLAGS', 'CPPPATH', 'CPPLAGS', 'CXXDEFINES']
"main msvc variables"

# ezzel meg szopni fogsz
class msvcobj(ccroot.ccroot):
	def __init__(self, type='program', subtype=None):
		ccroot.ccroot.__init__(self, type, subtype)
		self.s_default_ext = ['.rc']

		self.ccflags=''
		self.cxxflags=''
		self.cppflags=''

		self._incpaths_lst=[]
		self._bld_incpaths_lst=[]

		self.m_linktask=None
		self.m_deps_linktask=[]

		self.m_type_initials = 'cc'
		self.subsystem = ''

		global g_msvc_flag_vars
		self.p_flag_vars = g_msvc_flag_vars

		global g_msvc_type_vars
		self.p_type_vars = g_msvc_type_vars
		self.libpaths=[]

	def apply(self):
		ccroot.ccroot.apply(self)
		# FIXME: /Wc, and /Wl, handling came here...

	def apply_defines(self):
		tree = Params.g_build
		clst = self.to_list(self.defines)+self.to_list(self.env['CCDEFINES'])
		cpplst = self.to_list(self.defines)+self.to_list(self.env['CXXDEFINES'])
		cmilst = []
		cppmilst = []

		# now process the local defines
		for defi in clst:
			if not defi in cmilst:
				cmilst.append(defi)

		for defi in cpplst:
			if not defi in cppmilst:
				cppmilst.append(defi)

		# CXXDEFINES_USELIB
		libs = self.to_list(self.uselib)
		for l in libs:
			val = self.env['CXXDEFINES_'+l]
			if val: cmilst += self.to_list(val)
			val = self.env['CCDEFINES_'+l]
			if val: cppmilst += val
		self.env['DEFLINES'] = ["define %s" % ' '.join(x.split('=', 1)) for x in cmilst]
		self.env['DEFLINES'] = self.env['DEFLINES'] + ["define %s" % ' '.join(x.split('=', 1)) for x in cppmilst]

		y = self.env['CCDEFINES_ST']
		self.env['_CCDEFFLAGS'] = [y%x for x in cmilst]
		y = self.env['CXXDEFINES_ST']
		self.env['_CXXDEFFLAGS'] = [y%x for x in cppmilst]

	def is_syslib(self,libname):
		global g_msvc_systemlibs
		if g_msvc_systemlibs.has_key(libname):
			return True
		return False

	def find_lt_names(self,libname,is_static=False):
		"Win32/MSVC specific code to glean out information from libtool la files."
		lt_names=[
			'lib%s.la' % libname,
			'%s.la' % libname,
		]

		for path in self.libpaths:
			for la in lt_names:
				laf=os.path.join(path,la)
				dll=None
				if exists(laf):
					ltdict=read_la_file(laf)
					lt_libdir=None
					if ltdict.has_key('libdir') and ltdict['libdir'] != '':
						lt_libdir=ltdict['libdir']
					if not is_static and ltdict.has_key('library_names') and ltdict['library_names'] != '':
						dllnames=ltdict['library_names'].split()
						dll=dllnames[0].lower()
						dll=re.sub('\.dll$', '', dll)
						return [lt_libdir,dll,False]
					elif ltdict.has_key('old_library') and ltdict['old_library'] != '':
						olib=ltdict['old_library']
						if exists(os.path.join(path,olib)):
							return [path,olib,True]
						elif lt_libdir != '' and exists(os.path.join(lt_libdir,olib)):
							return [lt_libdir,olib,True]
						else:
							return [None,olib,True]
					else:
						fatal('invalid libtool object file: %s' % laf)
		return [None,None,None]

	def getlibname(self,libname,is_static=False):
		lib=libname.lower()
		lib=re.sub('\.lib$','',lib)

		if self.is_syslib(lib):
			return lib+'.lib'

		lib=re.sub('^lib','',lib)

		if lib == 'm':
			return None

		[lt_path,lt_libname,lt_static]=self.find_lt_names(lib,is_static)

		if lt_path != None and lt_libname != None:
			if lt_static == True:
				# file existance check has been made by find_lt_names
				return os.path.join(lt_path,lt_libname)

		if lt_path != None:
			_libpaths=[lt_path] + self.libpaths
		else:
			_libpaths=self.libpaths

		static_libs=[
			'%ss.lib' % lib,
			'lib%ss.lib' % lib,
			'%s.lib' %lib,
			'lib%s.lib' % lib,
			]

		dynamic_libs=[
			'lib%s.dll.lib' % lib,
			'lib%s.dll.a' % lib,
			'%s.dll.lib' % lib,
			'%s.dll.a' % lib,
			'lib%s_d.lib' % lib,
			'%s_d.lib' % lib,
			'%s.lib' %lib,
			]

		libnames=static_libs
		if not is_static:
			libnames=dynamic_libs + static_libs

		for path in _libpaths:
			for libn in libnames:
				if os.path.exists(os.path.join(path,libn)):
					debug('lib found: %s' % os.path.join(path,libn), 'msvc')
					return libn

		return None

	def apply_obj_vars(self):
		debug('apply_obj_vars called for msvcobj', 'msvc')
		env = self.env
		app = env.append_unique

		cpppath_st       = env['CPPPATH_ST']
		lib_st           = env['LIB_ST']
		staticlib_st     = env['STATICLIB_ST']
		libpath_st       = env['LIBPATH_ST']
		staticlibpath_st = env['STATICLIBPATH_ST']

		self.addflags('CCFLAGS', self.ccflags)
		self.addflags('CXXFLAGS', self.cxxflags)
		self.addflags('CPPFLAGS', self.cppflags)

		# local flags come first
		# set the user-defined includes paths
		if not self._incpaths_lst: self.apply_incpaths()
		for i in self._bld_incpaths_lst:
			app('_CCINCFLAGS', cpppath_st % i.bldpath(env))
			app('_CCINCFLAGS', cpppath_st % i.srcpath(env))
			app('_CXXINCFLAGS', cpppath_st % i.bldpath(self.env))
			app('_CXXINCFLAGS', cpppath_st % i.srcpath(self.env))

		# set the library include paths
		for i in env['CPPPATH']:
			app('_CCINCFLAGS', cpppath_st % i)
			app('_CXXINCFLAGS', cpppath_st % i)

		# this is usually a good idea
		app('_CCINCFLAGS', cpppath_st % '.')
		app('_CCINCFLAGS', cpppath_st % env.variant())
		app('_CXXINCFLAGS', cpppath_st % '.')
		app('_CXXINCFLAGS', cpppath_st % self.env.variant())
		try:
			tmpnode = Params.g_build.m_curdirnode
			app('_CCINCFLAGS', cpppath_st % tmpnode.bldpath(env))
			app('_CCINCFLAGS', cpppath_st % tmpnode.srcpath(env))
			app('_CXXINCFLAGS', cpppath_st % tmpnode.bldpath(self.env))
			app('_CXXINCFLAGS', cpppath_st % tmpnode.srcpath(self.env))
		except:
			pass

		for i in env['RPATH']:   app('LINKFLAGS', i)
		for i in env['LIBPATH']:
			app('LINKFLAGS', libpath_st % i)
			if not self.libpaths.count(i):
				self.libpaths.append(i)
		for i in env['LIBPATH']:
			app('LINKFLAGS', staticlibpath_st % i)
			if not self.libpaths.count(i):
				self.libpaths.append(i)

		# i doubt that anyone will make a fully static binary anyway
		if not env['FULLSTATIC']:
			if env['STATICLIB'] or env['LIB']:
				app('LINKFLAGS', env['SHLIB_MARKER'])

		if env['STATICLIB']:
			app('LINKFLAGS', env['STATICLIB_MARKER'])
			for i in env['STATICLIB']:
				debug('libname: %s' % i,'msvc')
				libname=self.getlibname(i,True)
				debug('libnamefixed: %s' % libname,'msvc')
				if libname != None:
					app('LINKFLAGS', libname)

		if self.env['LIB']:
			for i in env['LIB']:
				debug('libname: %s' % i,'msvc')
				libname=self.getlibname(i)
				debug('libnamefixed: %s' % libname,'msvc')
				if libname != None:
					app('LINKFLAGS', libname)

	def apply_core (self):
		ccroot.ccroot.apply_core(self)
		self.m_linktask.m_type=self.m_type
		self.m_linktask.m_subsystem=self.subsystem

class msvccc(msvcobj):
	def __init__(self, type='program', subtype=None):
		msvcobj.__init__(self, type, subtype)
		self.s_default_ext = self.s_default_ext + ['.c']
		self.m_type_initials = 'cc'

class msvccpp(msvcobj):
	def __init__(self, type='program', subtype=None):
		msvcobj.__init__(self, type, subtype)
		self.m_type_initials = 'cpp'
		self.s_default_ext = self.s_default_ext + ['.cpp', '.cc', '.cxx','.C']

def setup(env):
	static_link_str = '${STLIBLINK_CXX} ${CPPLNK_SRC_F}${SRC} ${CPPLNK_TGT_F}${TGT}'
	cc_str = '${CC} ${CCFLAGS} ${CPPFLAGS} ${_CCINCFLAGS} ${_CCDEFFLAGS} ${CC_SRC_F}${SRC} ${CC_TGT_F}${TGT}'
	cc_link_str = '${LINK_CC} ${CCLNK_SRC_F}${SRC} ${CCLNK_TGT_F}${TGT} ${LINKFLAGS} ${_LIBDIRFLAGS} ${_LIBFLAGS}'
	cpp_str = '${CXX} ${CXXFLAGS} ${CPPFLAGS} ${_CXXINCFLAGS} ${_CXXDEFFLAGS} ${CXX_SRC_F}${SRC} ${CXX_TGT_F}${TGT}'
	cpp_link_str = '${LINK_CXX} ${CPPLNK_SRC_F}${SRC} ${CPPLNK_TGT_F}${TGT} ${LINKFLAGS} ${_LIBDIRFLAGS} ${_LIBFLAGS}'

	rc_str='${RC} ${RCFLAGS} /fo ${TGT} ${SRC}'

	Action.simple_action('cc', cc_str, color='GREEN')
	Action.simple_action('cpp', cpp_str, color='GREEN')
	Action.simple_action('ar_link_static', static_link_str, color='YELLOW')

	Action.Action('cc_link', vars=['LINK_CC', 'CCLNK_SRC_F', 'CCLNK_TGT_F', 'LINKFLAGS', '_LIBDIRFLAGS', '_LIBFLAGS','MT','MTFLAGS'] , color='YELLOW', func=msvc_linker)
	Action.Action('cpp_link', vars=[ 'LINK_CXX', 'CPPLNK_SRC_F', 'CPPLNK_TGT_F', 'LINKFLAGS', '_LIBDIRFLAGS', '_LIBFLAGS' ] , color='YELLOW', func=msvc_linker)
	Action.simple_action('rc', rc_str, color='GREEN')

	Object.register('cc', msvccc)
	Object.register('cpp', msvccpp)

	try: Object.hook('cc','RC_EXT',resource_compiler)
	except: pass

	try: Object.hook('cpp','RC_EXT',resource_compiler)
	except: pass

def quote_str(str):
		return (str.strip().find(' ') > 0 and '"%s"' % str or str).replace('""', '"')

def detect(conf):

	comp = conf.find_program('CL', var='CXX')
	if not comp:
		return 0;

	link = conf.find_program('LINK')
	if not link:
		return 0;

	stliblink = conf.find_program('LIB')
	if not stliblink:
		return 0;

	rescompiler = conf.find_program('RC')
	if not rescompiler:
		return 0

	manifesttool = conf.find_program('MT')

	v = conf.env

	# c/c++ compiler - check for whitespace, and if so, add quotes
	v['CC']                 = quote_str(comp)
	v['CXX']                 = v['CC']

	v['CPPFLAGS']            = ['/W3', '/nologo', '/c', '/EHsc', '/errorReport:prompt']
	v['CCDEFINES']          = ['WIN32'] # command-line defines
	v['CXXDEFINES']          = ['WIN32'] # command-line defines

	v['_CCINCFLAGS']        = []
	v['_CCDEFFLAGS']        = []
	v['_CXXINCFLAGS']        = []
	v['_CXXDEFFLAGS']        = []

	v['CC_SRC_F']           = ''
	v['CC_TGT_F']           = '/Fo'
	v['CXX_SRC_F']           = ''
	v['CXX_TGT_F']           = '/Fo'

	v['CPPPATH_ST']          = '/I%s' # template for adding include paths

	# Subsystem specific flags
	v['CPPFLAGS_CONSOLE']		= ['/SUBSYSTEM:CONSOLE']
	v['CPPFLAGS_NATIVE']		= ['/SUBSYSTEM:NATIVE']
	v['CPPFLAGS_POSIX']			= ['/SUBSYSTEM:POSIX']
	v['CPPFLAGS_WINDOWS']		= ['/SUBSYSTEM:WINDOWS']
	v['CPPFLAGS_WINDOWSCE']	= ['/SUBSYSTEM:WINDOWSCE']

	# CRT specific flags
	v['CPPFLAGS_CRT_MULTITHREADED'] =						['/MT']
	v['CPPFLAGS_CRT_MULTITHREADED_DLL'] =				['/MD']
	v['CPPDEFINES_CRT_MULTITHREADED'] =					['_MT']
	v['CPPDEFINES_CRT_MULTITHREADED_DLL'] =			['_MT', '_DLL']

	v['CPPFLAGS_CRT_MULTITHREADED_DBG'] =				['/MTd']
	v['CPPFLAGS_CRT_MULTITHREADED_DLL_DBG'] =		['/MDd']
	v['CPPDEFINES_CRT_MULTITHREADED_DBG'] =					['_DEBUG', '_MT']
	v['CPPDEFINES_CRT_MULTITHREADED_DLL_DBG'] =			['_DEBUG', '_MT', '_DLL']

	# compiler debug levels
	v['CCFLAGS']            = ['/TC']
	v['CCFLAGS_OPTIMIZED']  = ['/O2', '/DNDEBUG']
	v['CCFLAGS_RELEASE']    = ['/O2', '/DNDEBUG']
	v['CCFLAGS_DEBUG']      = ['/Od', '/RTC1', '/D_DEBUG', '/ZI']
	v['CCFLAGS_ULTRADEBUG'] = ['/Od', '/RTC1', '/D_DEBUG', '/ZI']

	v['CXXFLAGS']            = ['/TP']
	v['CXXFLAGS_OPTIMIZED']  = ['/O2', '/DNDEBUG']
	v['CXXFLAGS_RELEASE']    = ['/O2', '/DNDEBUG']
	v['CXXFLAGS_DEBUG']      = ['/Od', '/RTC1', '/D_DEBUG', '/ZI']
	v['CXXFLAGS_ULTRADEBUG'] = ['/Od', '/RTC1', '/D_DEBUG', '/ZI']


	# linker
	v['STLIBLINK_CXX']       = '\"%s\"' % stliblink
	v['LINK_CXX']            = '\"%s\"' % link
	v['LINK_CC']             = v['LINK_CXX']
	v['LIB']                 = []

	v['CPPLNK_TGT_F']        = '/OUT:'
	v['CCLNK_TGT_F']         = v['CPPLNK_TGT_F']
	v['CPPLNK_SRC_F']        = ' '
	v['CCLNK_SRC_F']         = v['CCLNK_SRC_F']

	v['LIB_ST']              = '%s.lib' # template for adding libs
	v['LIBPATH_ST']          = '/LIBPATH:%s' # template for adding libpaths
	v['STATICLIB_ST']        = '%s.lib'
	v['STATICLIBPATH_ST']    = '/LIBPATH:%s'
	v['CCDEFINES_ST']       = '/D%s'
	v['CXXDEFINES_ST']       = '/D%s'
	v['_LIBDIRFLAGS']        = ''
	v['_LIBFLAGS']           = ''

	v['SHLIB_MARKER']        = ''
	v['STATICLIB_MARKER']    = ''

	# manifest tool. Not required for VS 2003 and below. Must have for VS 2005 and later
	if manifesttool:
		v['MT'] = quote_str (manifesttool)
		v['MTFLAGS']=['/NOLOGO']

	# MSVC Resource compiler
	v['RC'] = quote_str(rescompiler)
	v['RCFLAGS'] = ['/r']
	v['RC_EXT'] = ['.rc']

	# linker debug levels
	v['LINKFLAGS']           = ['/NOLOGO', '/MACHINE:X86', '/ERRORREPORT:PROMPT']
	v['LINKFLAGS_OPTIMIZED'] = ['']
	v['LINKFLAGS_RELEASE']   = ['/OPT:REF', '/OPT:ICF', '/INCREMENTAL:NO']
	v['LINKFLAGS_DEBUG']     = ['/DEBUG', '/INCREMENTAL','msvcrtd.lib']
	v['LINKFLAGS_ULTRADEBUG'] = ['/DEBUG', '/INCREMENTAL','msvcrtd.lib']

	try:
		debuglevel = Params.g_options.debug_level
	except AttributeError:
		debuglevel = 'DEBUG'
	else:
		debuglevel = debuglevel.upper()
	v['CCFLAGS']   += v['CCFLAGS_'+debuglevel]
	v['CXXFLAGS']  += v['CXXFLAGS_'+debuglevel]
	v['LINKFLAGS'] += v['LINKFLAGS_'+debuglevel]

	def addflags(var):
		try:
			c = os.environ[var]
			if c:
				# stripping leading and trailing and whitespace and ", Windows cmd is a bit stupid ...
				c=c.strip('" ')
				for cv in c.split():
					v[var].append(cv)
		except:
			pass

	addflags('CXXFLAGS')
	addflags('CPPFLAGS')
	addflags('LINKFLAGS')

	if not v['DESTDIR']: v['DESTDIR']=''

	# shared library
	v['shlib_CCFLAGS']  = ['']
	v['shlib_CXXFLAGS'] = ['']
	v['shlib_LINKFLAGS']= ['/DLL']
	v['shlib_obj_ext']  = ['.obj']
	v['shlib_PREFIX']   = ''
	v['shlib_SUFFIX']   = '.dll'
	v['shlib_IMPLIB_SUFFIX'] = ['.lib']
	v['shlib_INST_VAR'] = 'PREFIX'
	v['shlib_INST_DIR'] = 'lib'

	# static library
	v['staticlib_LINKFLAGS'] = ['']
	v['staticlib_obj_ext'] = ['.obj']
	v['staticlib_PREFIX']  = ''
	v['staticlib_SUFFIX']  = '.lib'
	v['staticlib_INST_VAR'] = 'PREFIX'
	v['staticlib_INST_DIR'] = 'lib'

	# program
	v['program_obj_ext']  = ['.obj']
	v['program_SUFFIX']   = '.exe'
	v['program_INST_VAR'] = 'PREFIX'
	v['program_INST_DIR'] = 'bin'

	return 1

def set_options(opt):
	try:
		opt.add_option('-d', '--debug-level',
		action = 'store',
		default = 'debug',
		help = 'Specify the debug level. [Allowed values: ultradebug, debug, release, optimized]',
		dest = 'debug_level')
	except optparse.OptionConflictError:
		pass

