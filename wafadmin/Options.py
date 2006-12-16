#! /usr/bin/env python
# encoding: utf-8
# Scott Newton, 2005 (scottn)
# Thomas Nagy, 2006 (ita)

"Custom command-line options"

import os, sys, imp, types
from optparse import OptionParser
import Params, Utils
from Params import debug, fatal, warning, error

# Such a command-line should work:  PREFIX=/opt/ DESTDIR=/tmp/ahoj/ waf configure
try:
	default_prefix = os.environ['PREFIX']
except:
	if sys.platform == 'win32': default_prefix='c:\\temp\\'
	else: default_prefix = '/usr/local/'

try:
	default_destdir = os.environ['DESTDIR']
except:
	default_destdir = ''


def create_parser():
	debug("create_parser is called", 'options')

	parser = OptionParser(usage = """waf [options] [commands ...]

* Main commands: configure build install clean dist distclean uninstall
* Example: ./waf build -j4""", version = 'waf %s' % Params.g_version)

	p=parser.add_option

	p('-j', '--jobs',
		type    = 'int',
		default = 1,
		help    = 'specify the number of parallel jobs [Default: 1]',
		dest    = 'jobs')

	p('-e', '--evil',
		action  = 'store_true',
		default = False,
		help    = 'run as a daemon     [Default: False]',
		dest    = 'daemon')

	p('-f', '--force',
		action  = 'store_true',
		default = False,
		help    = 'force the files installation',
		dest    = 'force')

	p('-k', '--keep',
		action  = 'store_true',
		default = False,
		help    = 'keep running happily on independant task groups',
		dest    = 'keep')

	p('-p', '--progress',
		action  = 'count',
		default = 0,
		help    = '-p: progress bar; -pp: ide output',
		dest    = 'progress_bar')

	p('-v', '--verbose',
		action  = 'count',
		default = 0,
		help    = 'show verbose output [Default: False]',
		dest    = 'verbose')

	p('--prefix',
		help    = "installation prefix [Default: '%s']" % default_prefix,
		default = default_prefix,
		dest    = 'prefix')

	p('--destdir',
		help    = "installation root   [Default: '%s']" % default_destdir,
		default = default_destdir,
		dest    = 'destdir')

	p('--nocache',
		action  = 'store_true',
		default = False,
		help    = 're-run all compilation tests',
		dest    = 'nocache')

	if 'configure' in sys.argv:
		p('-b', '--blddir',
			action  = 'store',
			default = '',
			help    = 'build dir for the project (configuration)',
			dest    = 'blddir')

		p('-s', '--srcdir',
			action  = 'store',
			default = '',
			help    = 'src dir for the project (configuration)',
			dest    = 'srcdir')

	p('--zones',
		action  = 'store',
		default = '',
		help    = 'debugging zones',
		dest    = 'zones')

	p('--targets',
		action  = 'store',
		default = '',
		help    = 'compile the targets given only',
		dest    = 'compile_targets')

	return parser

def parse_args_impl(parser):
	(Params.g_options, args) = parser.parse_args()
	#print Params.g_options, " ", args

	# By default, 'waf' is equivalent to 'waf build'
	lst='dist configure clean distclean build install uninstall'.split()
	for var in lst:    Params.g_commands[var]    = 0
	if len(args) == 0: Params.g_commands['build'] = 1

	# Parse the command arguments
	for arg in args:
		arg = arg.strip()
		if arg in lst:
			Params.g_commands[arg]=True
		else:
			print 'Error: Invalid command specified ',arg
			print parser.print_help()
			sys.exit(1)

	# TODO -k => -j0
	if Params.g_options.keep: Params.g_options.jobs = 1

	Params.g_verbose = Params.g_options.verbose
	Params.g_zones = Params.g_options.zones.split(',')
	if Params.g_verbose>1: Params.set_trace(1,1,1)
	else: Params.set_trace(0,0,1)

class Handler:
	"loads wscript modules in folders for adding options"
	def __init__(self):
		self.parser = create_parser()
		self.cwd = os.getcwd()
	def add_option(self, *kw, **kwargs):
		self.parser.add_option(*kw, **kwargs)
	def sub_options(self, dir):
		current = self.cwd

		self.cwd = os.path.join(self.cwd, dir)
		cur = os.path.join(self.cwd, 'wscript')

		debug("cur is "+str(cur), 'options')

		try:
			mod = Utils.load_module(cur)
		except AttributeError:
			msg = "no module was found for wscript (sub_options)\n[%s]:\n * make sure such a function is defined \n * run configure from the root of the project"
			fatal(msg % self.cwd)
		try:
			mod.set_options(self)
		except AttributeError:
			msg = "no set_options function was found in wscript\n[%s]:\n * make sure such a function is defined \n * run configure from the root of the project"
			fatal(msg % self.cwd)

		self.cwd = current

	def tool_options(self, tool, tooldir=None):
		if type(tool) is types.ListType:
			for i in tool: self.tool_options(i, tooldir)
			return

		if not tooldir: tooldir = Params.g_tooldir
		tooldir = Utils.to_list(tooldir)
		try:
			file,name,desc = imp.find_module(tool, tooldir)
		except:
			error("no tool named '%s' found" % tool)
			return
		module = imp.load_module(tool,file,name,desc)
		try:
			module.set_options(self)
		except:
			warning("tool %s has no function set_options or set_options failed" % tool)
			pass

	def parse_args(self):
		parse_args_impl(self.parser)

