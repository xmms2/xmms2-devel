#! /usr/bin/env python
# encoding: utf-8
# Matthias Jahn <jahn.matthias@freenet.de>, 2007 (pmarat)

import os, sys, imp, types
import optparse
import Utils, Action, Params, checks, Configure

def __list_possible_compiler(plattform):
	c_compiler = {
'win32':  ['msvc', 'g++'],
'cygwin': ['g++'],
'darwin': ['g++'],
'aix5':   ['g++'],
'linux':  ['g++', 'sunc++'],
'sunos':  ['sunc++', 'g++'],
'irix':   ['g++'],
'hpux':   ['g++'],
'default': ['g++']
	}
	try:
		return(c_compiler[plattform])
	except KeyError:
		return(c_compiler["default"])

def setup(env):
	pass

def detect(conf):
	test_for_compiler = Params.g_options.check_cxx_compiler
	for cxx_compiler in test_for_compiler.split():
		if conf.check_tool(cxx_compiler):
			conf.check_message("%s" %cxx_compiler, '', True)
			conf.env["COMPILER_CXX"] = "%s" %cxx_compiler #store the choosed c++ compiler
			return (1)
		conf.check_message("%s" %cxx_compiler, '', False)
	conf.env["COMPILER_CXX"] = None
	return (0)

def set_options(opt):
	detected_plattform = checks.detect_platform(None)
	possible_compiler_list = __list_possible_compiler(detected_plattform)
	test_for_compiler = str(" ").join(possible_compiler_list)
	cxx_compiler_opts = opt.add_option_group("C++ Compiler Options")
	try:
		cxx_compiler_opts.add_option('--check-cxx-compiler', default="%s" % test_for_compiler,
			help='On this platform (%s) following C++ Compiler will be checked default: "%s"' % 
								(detected_plattform, test_for_compiler),
			dest="check_cxx_compiler")
	except optparse.OptionConflictError:
		# the g++ tool might have added that option already
		pass
	for cxx_compiler in test_for_compiler.split():
		opt.tool_options('%s' % cxx_compiler, option_group=cxx_compiler_opts)

