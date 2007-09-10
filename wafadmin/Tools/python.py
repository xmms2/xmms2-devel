#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2007 (ita)
# Gustavo Carneiro (gjc), 2007

"Python support"

import os, sys
import Object, Action, Utils, Runner, Params, Common
from pproc import *

class pyobj(Object.genobj):
	s_default_ext = ['.py']
	def __init__(self, env=None):
		Object.genobj.__init__(self, 'other')
		self.pyopts = ''

		self.inst_var = 'PYTHONDIR'
		self.inst_dir = ''
		self.prio = 50

		self.env = env
		if not self.env: self.env = Params.g_build.m_allenvs['default']
		self.pyc = self.env['PYC']
		self.pyo = self.env['PYO']

	def apply(self):
		find_source_lst = self.path.find_source_lst

		envpyo = self.env.copy()
		envpyo['PYCMD']

		# first create the nodes corresponding to the sources
		for filename in self.to_list(self.source):
			node = find_source_lst(Utils.split_path(filename))
			if node is None:
				Params.fatal("Python source '%s' not found" % filename)

			base, ext = os.path.splitext(filename)
			#node = self.path.find_build(filename)
			if not ext in self.s_default_ext:
				fatal("unknown file "+filename)

			if self.pyc:
				task = self.create_task('pyc', self.env, self.prio)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.pyc'))
			if self.pyo:
				task = self.create_task('pyo', self.env, self.prio)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.pyo'))

	def install(self):
		for i in self.m_tasks:
			current = Params.g_build.m_curdirnode
			lst=[a.relpath_gen(current) for a in i.m_outputs]
			Common.install_files(self.inst_var, self.inst_dir, lst)
			lst=[a.relpath_gen(current) for a in i.m_inputs]
			Common.install_files(self.inst_var, self.inst_dir, lst)
			#self.install_results(self.inst_var, self.inst_dir, i)

def setup(env):
	Object.register('py', pyobj)
	Action.simple_action('pyc', '${PYTHON} ${PYFLAGS} -c ${PYCMD} ${SRC} ${TGT}', color='BLUE')
	Action.simple_action('pyo', '${PYTHON} ${PYFLAGS_OPT} -c ${PYCMD} ${SRC} ${TGT}', color='BLUE')

def _get_python_variables(python_exe, variables, imports=['import sys']):
	"""Run a python interpreter and print some variables"""
	program = list(imports)
	program.append('')
	for v in variables:
		program.append("print repr(%s)" % v)
	output = Popen([python_exe, "-c", '\n'.join(program)], stdout=PIPE).communicate()[0].split("\n")
	return_values = []
	for s in output:
		# FIXME eval ????
		if s: return_values.append(eval(s.rstrip()))
		else: break
	return return_values

def check_python_headers(conf):
	"""Check for headers and libraries necessary to extend or embed python.

	If successful, xxx_PYEXT and xxx_PYEMBED variables are defined in the
    enviroment (for uselib).  PYEXT should be used for compiling
    python extensions, while PYEMBED should be used by programs that
    need to embed a python interpreter.

	Note: this test requires that check_python_version was previously
	executed and successful."""

	python = conf.env['PYTHON']
	assert python, ("python is %r !" % (python,))

	v = 'prefix CC SYSLIBS SHLIBS LIBDIR LIBPL INCLUDEPY Py_ENABLE_SHARED'.split()
	(python_prefix, python_CC, python_SYSLIBS, python_SHLIBS,
	 python_LIBDIR, python_LIBPL, INCLUDEPY, Py_ENABLE_SHARED) = \
		_get_python_variables(python, ["get_config_var('%s')" % x for x in v], ['from distutils.sysconfig import get_config_var'])
	python_includes = [INCLUDEPY]

	## Check for python libraries for embedding
	if python_SYSLIBS is not None:
		for lib in python_SYSLIBS.split():
			libname = lib[2:] # strip '-l'
			conf.env.append_value('LIB_PYEMBED', libname)
	if python_SHLIBS is not None:
		for lib in python_SHLIBS.split():
			libname = lib[2:] # strip '-l'
			conf.env.append_value('LIB_PYEMBED', libname)
	lib = conf.create_library_configurator()
	lib.name = 'python' + conf.env['PYTHON_VERSION']
	lib.uselib = 'PYTHON'
	lib.code = '''
#ifdef __cplusplus
extern "C" {
#endif
 void Py_Initialize(void);
 void Py_Finalize(void);
#ifdef __cplusplus
}
#endif
int main(int argc, char *argv[]) { Py_Initialize(); Py_Finalize(); return 0; }
'''
	if python_LIBDIR is not None:
		lib.path = [python_LIBDIR]
		result = lib.run()
	else:
		result = 0

	## try again with -L$python_LIBPL (some systems don't install the python library in $prefix/lib)
	if not result:
		if python_LIBPL is not None:
			lib.path = [python_LIBPL]
			result = lib.run()
		else:
			result = 0

	## try again with -L$prefix/libs, and pythonXY name rather than pythonX.Y (win32)
	if not result:
		lib.path = [os.path.join(python_prefix, "libs")]
		lib.name = 'python' + conf.env['PYTHON_VERSION'].replace('.', '')
		result = lib.run()

	if result:
		conf.env['LIBPATH_PYEMBED'] = lib.path
		conf.env.append_value('LIB_PYEMBED', lib.name)

	if sys.platform == 'win32' or (Py_ENABLE_SHARED is not None
					and sys.platform != 'darwin'):
		conf.env['LIBPATH_PYEXT'] = conf.env['LIBPATH_PYEMBED']
		conf.env['LIB_PYEXT'] = conf.env['LIB_PYEMBED']

	## Check for Python headers
	header = conf.create_header_configurator()
	header.path = python_includes
	header.name = 'Python.h'
	header.define = 'HAVE_PYTHON_H'
	header.uselib = 'PYEXT'
	header.code = "#include <Python.h>\nint main(int argc, char *argv[]) { Py_Initialize(); Py_Finalize(); return 0; }"
	result = header.run()
	if not result: return result

	conf.env['CPPPATH_PYEXT'] = python_includes
	conf.env['CPPPATH_PYEMBED'] = python_includes

	return result


def check_python_version(conf, minver=None):
	"""
	Check if the python interpreter is found matching a given minimum version.
	minver should be a tuple, eg. to check for python >= 2.4.2 pass (2,4,2) as minver.

	If successful, PYTHON_VERSION is defined as 'MAJOR.MINOR'
	(eg. '2.4') of the actual python version found, and PYTHONDIR is
	defined, pointing to the site-packages directory appropriate for
	this python version, where modules/packages/extensions should be
	installed.
	"""

	python = conf.env['PYTHON']
	assert python, ("python is %r !" % (python,))

	## Get python version string
	## Note: only works for python >= 2.0, but we don't want to
	## support python 1.x in 2007, do we? :)
	proc = Popen([python, "-c", "import sys; print repr(sys.version_info)"], stdout=PIPE)
	pyver_tuple = eval(proc.communicate()[0].rstrip())

	## compare python version with the minimum required
	result = (minver is None) or (pyver_tuple >= minver)

	if result:
		## define useful environment variables
		pyver = '.'.join(map(str, pyver_tuple[:2]))
		conf.env['PYTHON_VERSION'] = pyver

		if 'PYTHONDIR' in os.environ:
			dir = os.environ['PYTHONDIR']
		else:
			if sys.platform == 'win32':
				(python_LIBDEST,) = \
						_get_python_variables(python, ["get_config_var('LIBDEST')"],
						['from distutils.sysconfig import get_config_var'])
			else:
				python_LIBDEST = None
			if python_LIBDEST is None:
				python_LIBDEST = os.path.join(conf.env['PREFIX'], "lib", "python" + pyver)
			dir = os.path.join(python_LIBDEST, "site-packages")

		conf.add_define('PYTHONDIR', dir)
		conf.env['PYTHONDIR'] = dir

	## Feedback
	pyver_full = '.'.join(map(str, pyver_tuple[:3]))
	minver_str = '.'.join(map(str, minver))
	if minver is None:
		conf.check_message_custom('Python version', '', pyver_full)
	else:
		conf.check_message('Python version', ">= %s" % (minver_str,), result, option=pyver_full)

	return result

def detect(conf):
	python = conf.find_program('python', var='PYTHON')
	if not python: return 0

	v = conf.env

	v['PYCMD'] = '"import sys, py_compile;py_compile.compile(sys.argv[1], sys.argv[2])"'
	v['PYFLAGS'] = ''
	v['PYFLAGS_OPT'] = '-O'

	v['PYC'] = getattr(Params.g_options, 'pyc', 1)
	v['PYO'] = getattr(Params.g_options, 'pyo', 1)

	v['pyext_INST_VAR'] = 'PYTHONDIR'
	v['pyext_INST_DIR'] = ''

	v['pyembed_INST_VAR'] = v['program_INST_VAR']
	v['pyembed_INST_DIR'] = v['program_INST_DIR']

	v['pyext_PREFIX'] = ''

	if sys.platform == 'win32':
		v['pyext_SUFFIX'] = '.pyd'

	# now a small difference
	v['pyext_USELIB'] = 'PYTHON PYEXT'
	v['pyembed_USELIB'] = 'PYTHON PYEMBED'

	conf.hook(check_python_version)
	conf.hook(check_python_headers)

	return 1

def set_options(opt):
	opt.add_option('--nopyc', action = 'store_false', default = 1, help = 'no pyc files (configuration)', dest = 'pyc')
	opt.add_option('--nopyo', action = 'store_false', default = 1, help = 'no pyo files (configuration)', dest = 'pyo')

