#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Main parameters"

import os, sys, types, inspect, base64, stat
try: from hashlib import md5
except ImportError: from md5 import md5

import Utils

# =================================== #
# Fixed constants, change with care

g_version="1.2.0"
g_rootname = ''
if sys.platform=='win32':
	# get the first two letters (c:)
	g_rootname = os.getcwd()[:2]

g_dbfile='.wafpickle'
"name of the db file"

g_excludes = ['.svn', 'CVS', 'wafadmin', '.arch-ids']
"exclude from dist"

g_autoconfig = 0
"reconfigure the project automatically"

sig_nil = 'iluvcuteoverload'

# =================================== #
# Constants set on runtime

g_globals = {}
"global vars"

def set_globals(name, value):
	g_globals[name] = value
def globals(name):
	return g_globals.get(name, [])

g_cwd_launch = None
"directory from which waf was called"

g_tooldir=''
"Tools directory (used in particular by Environment.py)"

g_options = None
"Parsed command-line arguments in the options module"

g_commands = {}
"build, configure, .."

g_verbose = 0
"-v: warnings, -vv: developer info"

g_build = None
"only one build object is active at a time"

g_platform = sys.platform
"current platform"

g_cachedir = ''
"config cache directory"

g_homedir=''
for var in ['WAF_HOME', 'HOME', 'HOMEPATH']:
	if var in os.environ:
		#In windows, the home path is split into HOMEDRIVE and HOMEPATH
		if var == 'HOMEPATH' and 'HOMEDRIVE' in os.environ:
			g_homedir='%s%s' % (os.environ['HOMEDRIVE'], os.environ['HOMEPATH'])
		else:
			g_homedir=os.environ[var]
		break
g_homedir=os.path.abspath(g_homedir)
g_usecache = ''
try: g_usecache = os.path.abspath(os.environ['WAFCACHE'])
except KeyError: pass

# allow different names for lockfile
try: g_lockfile = os.environ['WAFLOCK']
except: g_lockfile = '.lock-wscript'

# =================================== #
# HELPERS

#g_col_names = ['BOLD', 'RED', 'REDP', 'GREEN', 'YELLOW', 'BLUE', 'CYAN', 'NORMAL']
#"color names"

g_col_scheme = [1, 91, 33, 92, 93, 94, 96, 0]

g_colors = {
'BOLD'  :'\033[01;1m',
'RED'   :'\033[01;91m',
'REDP'  :'\033[01;33m',
'GREEN' :'\033[01;92m',
'YELLOW':'\033[00;33m',
'BLUE'  :'\033[01;94m',
'CYAN'  :'\033[01;96m',
'NORMAL':'\033[0m'
}
"colors used for printing messages"

g_cursor_on ='\x1b[?25h'
g_cursor_off='\x1b[?25l'

def reset_colors():
	global g_colors
	for k in g_colors.keys():
		g_colors[k]=''
	g_cursor_on=''
	g_cursor_off=''

if (sys.platform=='win32') or ('NOCOLOR' in os.environ) \
	or (os.environ.get('TERM', 'dumb') == 'dumb') \
	or (not sys.stdout.isatty()):
	reset_colors()

def pprint(col, str, label=''):
	try: mycol=g_colors[col]
	except: mycol=''
	print "%s%s%s %s" % (mycol, str, g_colors['NORMAL'], label)

g_levels={
'Action' : 'GREEN',
'Build'  : 'CYAN',
'KDE'    : 'REDP',
'Node'   : 'GREEN',
'Object' : 'GREEN',
'Runner' : 'REDP',
'Task'   : 'GREEN',
'Test'   : 'GREEN',
}

g_zones = []

def set_trace(a, b, c):
	Utils.g_trace=a
	Utils.g_debug=b
	Utils.g_error=c

def get_trace():
	return (Utils.g_trace, Utils.g_debug, Utils.g_error)

def niceprint(msg, type='', module=''):
	#if not module:
	#	print '%s: %s'% (type, msg)
	#	return
	if type=='ERROR':
		print '%s: %s == %s == %s %s'% (type, g_colors['RED'], module, g_colors['NORMAL'], msg)
		return
	if type=='WARNING':
		print '%s: %s == %s == %s %s'% (type, g_colors['RED'], module, g_colors['NORMAL'], msg)
		return
	if type=='DEBUG':
		print '%s: %s == %s == %s %s'% (type, g_colors['CYAN'], module, g_colors['NORMAL'], msg)
		return
	if module in g_levels:
		print '%s: %s == %s == %s %s'% (type, g_colors[g_levels[module]], module, g_colors['NORMAL'], msg)
		return
	print 'TRACE: == %s == %s'% (module, msg)

def __get_module():
	try: return inspect.stack()[2][0].f_globals['__name__']
	except: return "unknown"

def debug(msg, zone=None):
	global g_zones, g_verbose
	if g_zones:
		if (not zone in g_zones) and (not '*' in g_zones):
			return
	elif not g_verbose>2:
		return
	module = __get_module()
	niceprint(msg, 'DEBUG', module)

def warning(msg, zone=0):
	module = __get_module()
	niceprint(msg, 'WARNING', module)

def error(msg):
	if not Utils.g_error: return
	module = __get_module()
	niceprint(msg, 'ERROR', module)

def fatal(msg, ret=1):
	module = __get_module()
	if g_verbose > 0:
		pprint('RED', '%s \n (error raised in module %s)' % (msg, module))
	else:
		pprint('RED', '%s' % msg)
	if g_verbose > 1:
		import traceback
		traceback.print_stack()
	sys.exit(ret)

# TODO rename vsig in view_sig
def vsig(s):
	"used for displaying signatures"
	if type(s) is types.StringType:
		n = base64.encodestring(s)
		return n[:-2]
	else:
		return str(s)

def hash_sig(o1, o2):
	"hash two signatures"
	m = md5()
	m.update(o1)
	m.update(o2)
	return m.digest()

def h_file(filename):
	f = file(filename,'rb')
	m = md5()
	readBytes = 1024 # read 1024 bytes per time
	while (readBytes):
		readString = f.read(readBytes)
		m.update(readString)
		readBytes = len(readString)
	f.close()
	return m.digest()

def h_string(str):
	m = md5()
	m.update(str)
	return m.digest()

def h_list(lst):
	m = md5()
	m.update(str(lst))
	return m.digest()

def hash_function_with_globals(prevhash, func):
	"""
	hash a function (object) and the global vars needed from outside
	ignore unhashable global variables (lists)

	prevhash -- previous hash value to be combined with this one;
	if there is no previous value, zero should be used here

	func -- a Python function object.
	"""
	assert type(func) is types.FunctionType
	for name, value in func.func_globals.iteritems():
		if type(value) in (types.BuiltinFunctionType, types.ModuleType, types.FunctionType, types.ClassType, types.TypeType):
			continue
		try:
			prevhash = hash( (prevhash, name, value) )
		except TypeError: # raised for unhashable elements
			pass
	return hash( (prevhash, inspect.getsource(func)) )

