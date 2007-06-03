#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Utility functions"

import os, sys, imp, types, string, re
import Params

g_trace = 0
g_debug = 0
g_error = 0

def waf_version(mini = "0.0.1", maxi = "100.0.0"):
	"throws an exception if the waf version is wrong"
	min_lst = map(int, mini.split('.'))
	max_lst = map(int, maxi.split('.'))
	waf_lst = map(int, Params.g_version.split('.'))

	mm = min(len(min_lst), len(waf_lst))
	for (a, b) in zip(min_lst[:mm], waf_lst[:mm]):
		if a < b:
			break
		if a > b:
			Params.fatal("waf version should be at least %s (%s found)" % (mini, Params.g_version))

	mm = min(len(max_lst), len(waf_lst))
	for (a, b) in zip(max_lst[:mm], waf_lst[:mm]):
		if a > b:
			break
		if a < b:
			Params.fatal("waf version should be at most %s (%s found)" % (maxi, Params.g_version))

def error(msg):
	Params.niceprint(msg, 'ERROR', 'Configuration')

def reset():
	import Params, Task, preproc, Scripting, Object
	Params.g_build = None
	Task.g_tasks = Task.TaskManager()
	preproc.parse_cache = {}
	Scripting.g_inroot = 1
	Object.g_allobjs = []

def to_list(sth):
	if type(sth) is types.ListType:
		return sth
	else:
		return sth.split()

def options(**kwargs):
	pass

g_loaded_modules = {}
"index modules by absolute path"

g_module=None
"the main module is special"

def load_module(file_path, name='wscript'):
	"this function requires an absolute path"
	try:
		return g_loaded_modules[file_path]
	except KeyError:
		pass

	module = imp.new_module(name)

	try:
		file = open(file_path, 'r')
	except OSError:
		Params.fatal('The file %s could not be opened!' % file_path)

	import Common
	d = module.__dict__
	d['install_files'] = Common.install_files
	d['install_as'] = Common.install_as
	d['symlink_as'] = Common.symlink_as

	module_dir = os.path.dirname(file_path)
	sys.path.insert(0, module_dir)
	exec file in module.__dict__
	sys.path.remove(module_dir)
	if file: file.close()

	g_loaded_modules[file_path] = module

	return module

def set_main_module(file_path):
	"Load custom options, if defined"
	global g_module
	g_module = load_module(file_path, 'wscript_main')

	# remark: to register the module globally, use the following:
	# sys.modules['wscript_main'] = g_module

def to_hashtable(s):
	tbl = {}
	lst = s.split('\n')
	for line in lst:
		if not line:
			continue
		mems = line.split('=')
		tbl[mems[0]] = mems[1]
	return tbl

def copyobj(obj):
	cp = obj.__class__()
	for at in obj.__dict__.keys():
		setattr(cp, at, getattr(obj, at))
	return cp

def get_term_cols():
	return 55

try:
	import struct, fcntl, termios
	def machin():
		lines, cols = struct.unpack("HHHH", \
		fcntl.ioctl(sys.stdout.fileno(),termios.TIOCGWINSZ , \
		struct.pack("HHHH", 0, 0, 0, 0)))[:2]
		return cols
	get_term_cols = machin
except:
	pass

def split_path(path):
	"Split path into components. Supports UNC paths on Windows"
	if 'win' == sys.platform[:3] :
		h,t = os.path.splitunc(path)
		if not h: return __split_dirs(t)
		return [h] + __split_dirs(t)[1:]
	return __split_dirs(path)

def __split_dirs(path):
	h,t = os.path.split(path)
	if not h: return [t]
	if h == path: return [h]
	if not t: return __split_dirs(h)
	else: return __split_dirs(h) + [t]


def join_path(*path):
	return os.path.join(*path)

def join_path_list(path_lst):
	return join_path(*path_lst)

def is_absolute_path(path):
	""" more thorough absoluate path check """
	isabs = os.path.isabs(path)
	if not isabs and sys.platform == 'win32':
		isabs = (len(path) > 1 and path.find(':') > 0)
	elif not isabs and sys.platform != 'win32':
		isabs = re.search(r'^[\"\']/', path.strip(), re.M) != None
	return isabs

_path_to_preprocessor_name_translation = None
def path_to_preprocessor_name(path):
	"""Converts a file path like foo/zbr-xpto.h to a C preprocessor
	name like FOO_ZBR_XPTO_H"""
	global _path_to_preprocessor_name_translation
	if _path_to_preprocessor_name_translation is None:
		## make a translation table mapping everything except
		## alfanumeric chars to '_'
		invalid_chars = [chr(x) for x in xrange(256)]
		for valid in string.digits + string.uppercase:
			invalid_chars.remove(valid)
		_path_to_preprocessor_name_translation = string.maketrans(
			''.join(invalid_chars), '_'*len(invalid_chars))

	return string.translate(string.upper(path), _path_to_preprocessor_name_translation)
