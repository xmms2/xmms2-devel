# encoding: utf-8

import Utils, Logs
import Configure
import TaskGen, Task
import re, os

#Task.simple_task_type('cythonc', '${CYTHON} ${CYTHON_INCS} -q -o ${TGT} ${SRC}', color='BLUE', before='cc')

#Strip comments and strings (in case cimports appear in a multiline string)
strip_re=re.compile('#[^\r\n]*|"""(?:[^"\\\\]|\\\\.|"(?!""))*"""|\'(?:\\\\.|[^\'\\\\])*\'|"(?:\\\\.|[^"\\\\])*"')
cimport_re=re.compile('^cimport[ \t]+(?P<cimport>.*)$|^from[ \t]+(?P<from>.*)[ \t]+cimport[ \t]+.*$')

def extract_deps(node, env):
	cython_inc_dirs = [node.parent]
	cython_inc_dirs += env['CYTHON_INC_PATHS']
	c = node.read(env)
	c = strip_re.sub('', c)
	c = c.replace(';', '\n') # regexp requires only one instruction per line.
	raw_deps = set()
	for l in map(lambda a: a.strip(), c.split('\n')):
		if not l:
			continue
		m = cimport_re.match(l)
		if m:
			d = m.groupdict()
			s = ((d['cimport'] or "") + (d['from'] or "")).strip()
			if s:
				raw_deps.update(map(lambda a: a.strip(), s.split(',')))
	deps = []
	for rd in map(lambda l: l.split('.'), raw_deps):
		for inc in cython_inc_dirs:
			path = rd[:]
			path[-1] = path[-1]+'.pxd'
			dnode = inc.find_resource(path)
			if not dnode: #Maybe a directory with a __init__.pxd file.
				path = rd[:] + ['__init__.pxd']
				dnode = inc.find_resource(path)
			if dnode:
				if not dnode in deps:
					deps.append(dnode)
				break
	return deps

def scan_deps(task):
	"""
	Scan pyx and pxd files for dependencies.
	Adds dependencies only for local headers.
	"""
	deps = []
	scan = task.inputs[:]
	while scan:
		ds = extract_deps(scan.pop(), task.env)
		for _d in ds:
			if not _d in deps:
				scan.append(_d)
				deps.append(_d)
	return (deps, [])

def cython_run(task):
	pargs = [task.env['CYTHON']]
	pargs.extend(task.env['_CYTHON_INCFLAGS'])
	pargs.extend(['-o', task.outputs[0].bldpath(task.env), task.inputs[0].srcpath(task.env)])
	r = task.exec_command(pargs, cwd=getattr(task, 'cwd', None))

	if r or not task.env['CYTHON_NO_TIMESTAMP']:
		return r

	# strip timestamp
	src_file = task.outputs[0].bldpath(task.env)
	copy_file = src_file+'.cytmp'
	src = None
	copy = None
	try:
		src = open(src_file, 'rb')
		copy = open(copy_file, 'wb')
		i = 0
		for l in src:
			if i == 0:
				l = re.sub(' on .* \\*/', ' */', l)
			# TODO: strip all comment lines
			copy.write(l)
	finally:
		if src:
			src.close()
		if copy:
			copy.close()
	try:
		os.remove(src_file)
	except OSError:
		pass
	except IOError:
		pass
	else:
		os.rename(copy_file, src_file)
	return 0

f = TaskGen.declare_chain(
		name = 'cythonc',
		action = cython_run,
		ext_in = '.pyx',
		ext_out = '.c',
		reentrant = True,
		after='apply_cython_extra',
		scan=scan_deps)

@TaskGen.feature('cython')
@TaskGen.before('apply_core')
def default_cython(self):
	Utils.def_attrs(self,
			cython_includes = '',
			no_timestamp = False)

@TaskGen.feature('cython')
@TaskGen.after('apply_core')
def apply_cython_incpaths(self):
	"""Used to build _CYTHONINCFLAGS, and the dependencies scanner (when ready).
	"""
	for path in self.to_list(self.cython_includes):
		if os.path.isabs(path):
			self.env.append_value('CYTHONPATH', path)
		else:
			node = self.path.find_dir(path)
			if node:
				self.env.append_value('CYTHON_INC_PATHS', node)

	# XXX broken for pathes with spaces. Must escape \ and " and add double quotes.
	self.env['CYTHON_INCS'] = " ".join(map(lambda s: "-I"+s, self.env['CYTHON_PATHS']))

@TaskGen.feature('cython')
@TaskGen.after('apply_cython_incpaths')
def apply_cython_flags(self):
	INC_FMT='-I%s'
	for i in self.env['CYTHON_INC_PATHS']:
		self.env.append_unique('_CYTHON_INCFLAGS', INC_FMT % i.bldpath(self.env))
		self.env.append_unique('_CYTHON_INCFLAGS', INC_FMT % i.srcpath(self.env))
	for i in self.env['CYTHONPATH']:
		self.env.append_unique('_CYTHON_INCFLAGS', INC_FMT % i)

@TaskGen.feature('cython')
@TaskGen.after('apply_cython_flags')
def apply_cython_extra(self):
	self.env['CYTHON_NO_TIMESTAMP'] = self.no_timestamp


cython_ver_re = re.compile('Cython version ([0-9.]+)')
def check_cython_version(self, version=None, minver=None, maxver=None, mandatory=False):
	log_s = []
	if version:
		log_s.append("version=%r"%version)
		minver=version
		maxver=version
	else:
		if minver:
			log_s.append("minver=%r"%minver)
		if maxver:
			log_s.append("maxver=%r"%maxver)
	if isinstance(minver, str):
		minver = minver.split('.')
	if isinstance(maxver, str):
		maxver = maxver.split('.')
	if minver:
		minver = tuple(map(int, minver))
	if maxver:
		maxver = tuple(map(int, maxver))

	o = Utils.cmd_output('%s -V 2>&1' % self.env['CYTHON']).strip()
	m = cython_ver_re.match(o)
	self.check_message_1('Checking for cython version')
	if not m:
		if mandatory:
			self.fatal("Can't parse cython version")
		else:
			Utils.pprint('YELLOW', 'No version found')
			check = False
	else:
		v = m.group(1)
		ver = tuple(map(int, v.split('.')))
		check = (not minver or minver <= ver) and (not maxver or maxver >= ver)
		self.log.write('  cython %s\n  -> %r\n' % (" ".join(log_s), v))
		if check:
			Utils.pprint('GREEN', v)
			self.env['CYTHON_VERSION'] = ver
		else:
			Utils.pprint('YELLOW', 'wrong version %s' % v)
			if mandatory:
				self.fatal('failed (%s)' % m.group(1))
	return check

def detect(conf):
	cython = conf.find_program('cython', var='CYTHON')
	if not cython:
		return False
	# Register version checking method in configure class.
	Configure.conf(check_cython_version)
	conf.env['CYTHON_EXT'] = ['.pyx']
	return True

def set_options(opt):
	pass
