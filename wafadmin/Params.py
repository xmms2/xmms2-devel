#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Main parameters"

import os, sys, types, inspect, md5, base64, stat
import Utils

# =================================== #
# Fixed constants, change with care

g_version="1.1.1"
g_rootname = ''
if sys.platform=='win32':
	# get the first two letters (c:)
	g_rootname = os.getcwd()[:2]

g_dbfile='.wafpickle'
"name of the db file"

g_preprocess = 1
"use the c/c++ preprocessor"

g_excludes = ['.svn', 'CVS', 'wafadmin', '.arch-ids']
"exclude from dist"

g_strong_hash = 1
"hash method: md5 (1:default) or integers"

g_timestamp = 0
"if 1: do not look at the file contents for dependencies"

g_autoconfig = 0
"reconfigure the project automatically"

if g_strong_hash: sig_nil = 'iluvcuteoverload'
else: sig_nil = 17

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
"yellow not readable on white backgrounds? -> bug in *YOUR* terminal"

g_colors = {
'BOLD'  :'\033[01;1m',
'RED'   :'\033[01;91m',
'REDP'  :'\033[01;33m',
'GREEN' :'\033[01;92m',
'YELLOW':'\033[01;93m',
'BLUE'  :'\033[01;94m',
'CYAN'  :'\033[01;96m',
'NORMAL':'\033[0m'
}
"colors used for printing messages"

def reset_colors():
	global g_colors
	for k in g_colors.keys():
		g_colors[k]=''

if (sys.platform=='win32' or 'NOCOLOR' in os.environ
	or os.environ.get('TERM', 'dumb') == 'dumb'):
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

def vsig(s):
	"used for displaying signatures"
	if type(s) is types.StringType:
		n = base64.encodestring(s)
		return n[:-2]
	else:
		return str(s)

##
# functions to use
def hash_sig(o1, o2):
	return None
def h_file():
	return None
def h_string(s):
	return None
def h_list(lst):
	return None
##
# hash files
def h_md5_file(filename):
	f = file(filename,'rb')
	m = md5.new()
	readBytes = 1024 # read 1024 bytes per time
	while (readBytes):
		readString = f.read(readBytes)
		m.update(readString)
		readBytes = len(readString)
	f.close()
	return m.digest()
def h_md5_file_tstamp(filename):
	st = os.stat(filename)
	if stat.S_ISDIR(st.st_mode): raise OSError
	tt = st.st_mtime
	m = md5.new()
	m.update(str(tt)+filename)
	return m.digest()
def h_simple_file(filename):
	f = file(filename,'rb')
	s = f.read().__hash__()
	f.close()
	return s
def h_simple_file_tstamp(filename):
	st = os.stat(filename)
	if stat.S_ISDIR(st.st_mode): raise OSError
	m = md5.new()
	return hash( (st.st_mtime, filename) )
##
# hash signatures
def hash_sig_weak(o1, o2):
	return hash( (o1, o2) )
def hash_sig_strong(o1, o2):
	m = md5.new()
	m.update(o1)
	m.update(o2)
	return m.digest()
##
# hash string
def h_md5_str(str):
	m = md5.new()
	m.update( str )
	return m.digest()
def h_simple_str(str):
	return str.__hash__()
##
# hash lists
def h_md5_lst(lst):
	m = md5.new()
	m.update(str(lst))
	return m.digest()
def h_simple_lst(lst):
	return hash(str(lst))
##
#def set_hash(hash, tstamp):
if g_strong_hash:
	hash_sig = hash_sig_strong
	h_string = h_md5_str
	h_list = h_md5_lst
	if g_timestamp: h_file = h_md5_file_tstamp
	else: h_file = h_md5_file
else:
	hash_sig = hash_sig_weak
	h_string = h_simple_str
	h_list = h_simple_lst
	if g_timestamp: h_file = h_simple_file_tstamp
	else: h_file = h_simple_file

