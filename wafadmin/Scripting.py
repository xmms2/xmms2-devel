#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Module called for configuring, compiling and installing targets"

import os, sys, cPickle
import Params, Utils, Configure, Environment, Build, Runner, Options
from Params import error, fatal, warning, g_lockfile

g_dirwatch   = None
g_daemonlock = 0

def add_subdir(dir, bld):
	"each wscript calls bld.add_subdir"
	node = bld.m_curdirnode.ensure_node_from_lst(Utils.split_path(dir))
	if node is None:
		fatal("subdir not found (%s), restore is %s" % (dir, bld.m_curdirnode))
	bld.m_subdirs = [[node, bld.m_curdirnode]] + bld.m_subdirs

def call_back(idxName, pathName, event):
	#print "idxName=%s, Path=%s, Event=%s "%(idxName, pathName, event)
	# check the daemon lock state
	global g_daemonlock
	if g_daemonlock: return
	g_daemonlock = 1

	# clean up existing variables, and start a new instance
	Utils.reset()
	Main()
	g_daemonlock = 0

def start_daemon():
	"if it does not exist already:start a new directory watcher; else: return immediately"
	global g_dirwatch
	if not g_dirwatch:
		import DirWatch
		g_dirwatch = DirWatch.DirectoryWatcher()
		m_dirs=[]
		for nodeDir in Params.g_build.m_srcnode.dirs():
			tmpstr = "%s" %nodeDir
			tmpstr = "%s" %(tmpstr[3:])[:-1]
			m_dirs.append(tmpstr)
		g_dirwatch.add_watch("tmp Test", call_back, m_dirs)
		# infinite loop, no need to exit except on ctrl+c
		g_dirwatch.loop()
		g_dirwatch = None
	else:
		g_dirwatch.suspend_all_watch()
		m_dirs=[]
		for nodeDir in Params.g_build.m_srcnode.dirs():
			tmpstr = "%s" %nodeDir
			tmpstr = "%s" %(tmpstr[3:])[:-1]
			m_dirs.append(tmpstr)
		g_dirwatch.add_watch("tmp Test", call_back, m_dirs)

def configure():
	Runner.set_exec('normal')
	bld = Build.Build()
	try:
		srcdir = ""
		try: srcdir = Params.g_options.srcdir
		except AttributeError: pass
		if not srcdir: srcdir = Utils.g_module.srcdir

		blddir = ""
		try: blddir = Params.g_options.blddir
		except AttributeError: pass
		if not blddir: blddir = Utils.g_module.blddir

		Params.g_cachedir = Utils.join_path(blddir,'_cache_')

	except AttributeError:
		msg = "The attributes srcdir or blddir are missing from wscript\n[%s]\n * make sure such a function is defined\n * run configure from the root of the project\n * use waf configure --srcdir=xxx --blddir=yyy"
		fatal(msg % os.path.abspath('.'))
	except OSError:
		pass
	except KeyError:
		pass

	bld.load_dirs(srcdir, blddir)

	conf = Configure.Configure(srcdir=srcdir, blddir=blddir)
	conf.sub_config('')
	conf.store(bld)
	conf.cleanup()

	# this will write a configure lock so that subsequent run will
	# consider the current path as the root directory, to remove: use 'waf distclean'
	file = open(g_lockfile, 'w')
	w = file.write

	proj={}
	proj['blddir']=blddir
	proj['srcdir']=srcdir
	proj['argv']=sys.argv[1:]
	proj['hash']=conf.hash
	proj['files']=conf.files
	cPickle.dump(proj, file)
	file.close()

def read_cache_file(filename):
	file = open(g_lockfile, 'r')
	proj = cPickle.load(file)
	file.close()
	return proj

def Main():
	from Common import install_files, install_as, symlink_as # do not remove
	import inspect
	if Params.g_commands['configure']:
		configure()
		sys.exit(0)

	Runner.set_exec('noredir')

	# compile the project and/or install the files
	bld = Build.Build()
	try:
		proj = read_cache_file(g_lockfile)
	except IOError:
		if Params.g_commands['clean']:
			fatal("Nothing to clean (project not configured)", ret=0)
		else:
			warning("Run waf configure first...")
			configure()
			bld = Build.Build()
			proj = read_cache_file(g_lockfile)

	if Params.g_autoconfig:
		reconf = 0

		try:
			hash = 0
			for file in proj['files']:
				mod = Utils.load_module(file)
				hash = Params.hash_sig_weak(hash, inspect.getsource(mod.configure).__hash__())
			reconf = (hash != proj['hash'])
		except:
			warning("Reconfiguring the project as an exception occured")
			reconf=1

		if reconf:
			warning("Reconfiguring the project as the configuration changed")


			a1 = Params.g_commands
			a2 = Params.g_options
			a3 = Params.g_zones
			a4 = Params.g_verbose

			Options.g_parser.parse_args(args=proj['argv'])
			configure()

			Params.g_commands = a1
			Params.g_options = a2
			Params.g_zones = a3
			Params.g_verbose = a4


			bld = Build.Build()
			proj = read_cache_file(g_lockfile)

	Params.g_cachedir = Utils.join_path(proj['blddir'], '_cache_')

	bld.load_dirs(proj['srcdir'], proj['blddir'])
	bld.load_envs()

	#bld.dump()
	Utils.g_module.build(bld)

	# bld.m_subdirs can be modified *within* the loop, so do not touch this piece of code
	while bld.m_subdirs:
		# read scripts, saving the context everytime (bld.m_curdirnode)

		# cheap queue
		lst=bld.m_subdirs[0]
		bld.m_subdirs=bld.m_subdirs[1:]

		new=lst[0]
		old=lst[1]

		# take the new node position
		bld.m_curdirnode=new

		try: bld.rescan(bld.m_curdirnode)
		except OSError: fatal("No such directory "+bld.m_curdirnode.abspath())

		# try to open 'wscript_build' for execution
		# if unavailable, open the module wscript and call the build function from it
		try:
			file_path = os.path.join(new.abspath(), 'wscript_build')
			file = open(file_path, 'r')
			exec file
			if file: file.close()
		except IOError:
			file_path = os.path.join(new.abspath(), 'wscript')
			module = Utils.load_module(file_path)
			module.build(bld)

		# restore the old node position
		bld.m_curdirnode=old

	#bld.dump()

	pre_build()

	# compile
	if Params.g_commands['build'] or Params.g_commands['install']:
		try:
			ret = bld.compile()
			if Params.g_options.progress_bar: print ''
			if not ret: Params.pprint('GREEN', 'Compilation finished successfully')
		finally:
			bld.save()
		if ret:
			msg='Compilation failed'
			if not Params.g_options.daemon: fatal(msg)
			else: error(msg)

	# install
	if Params.g_commands['install'] or Params.g_commands['uninstall']:
		try:
			ret = bld.install()
			if not ret: Params.pprint('GREEN', 'Installation finished successfully')
		finally:
			bld.save()
		if ret: fatal('Compilation failed')

	# clean
	if Params.g_commands['clean']:
		try:
			ret = bld.clean()
			if not ret: Params.pprint('GREEN', 'Project cleaned successfully')
		finally:
			bld.save()
		if ret:
			msg='Cleanup failed for a mysterious reason'
			error(msg)

	# daemon look here
	if Params.g_options.daemon and Params.g_commands['build']:
		start_daemon()
		return

	# shutdown
	try:
		Utils.g_module.shutdown()
	except AttributeError:
		#raise
		pass

def pre_build():
	pass

def Dist(appname, version):
	"dist target - should be portable"
	import shutil, tarfile

	# Our temporary folder where to put our files
	TMPFOLDER=appname+'-'+version

	# Remove an old package directory
	if os.path.exists(TMPFOLDER): shutil.rmtree(TMPFOLDER)

	# Copy everything into the new folder
	shutil.copytree('.', TMPFOLDER)

	# Remove the Build dir
	try:
		if Utils.g_module.blddir: shutil.rmtree(os.path.join(TMPFOLDER, Utils.g_module.blddir))
	except AttributeError:
		pass

	# Enter into it and remove unnecessary files
	os.chdir(TMPFOLDER)
	for (root, dirs, filenames) in os.walk('.'):
		clean_dirs = []
		for d in dirs:
			if d in ['CVS', '.svn', '{arch}']:
				shutil.rmtree(os.path.join(root,d))
			elif d.startswith('.'):
				shutil.rmtree(os.path.join(root,d))
			else:
				clean_dirs += d
		dirs = clean_dirs

		to_remove = False
		for f in list(filenames):
			if f.startswith('.'): to_remove = True
			elif f.endswith('~'): to_remove = True
			elif f.endswith('.pyc'): to_remove = True
			elif f.endswith('.pyo'): to_remove = True
			elif f.endswith('.bak'): to_remove = True
			elif f.endswith('.orig'): to_remove = True
			elif f in ['config.log']: to_remove = True
			elif f.endswith('.tar.bz2'): to_remove = True
			elif f.endswith('.zip'): to_remove = True
			elif f.endswith('Makefile'): to_remove = True

			if to_remove:
				os.remove(os.path.join(root, f))
				to_remove = False

	try:
		dist_hook = Utils.g_module.dist_hook
	except AttributeError:
		pass
	else:
		blddir = os.path.join("..", Utils.g_module.blddir)
		srcdir = os.path.join("..", Utils.g_module.srcdir)
		dist_hook(srcdir, blddir)

	# go back to the root directory
	os.chdir('..')

	tar = tarfile.open(TMPFOLDER+'.tar.bz2','w:bz2')
	tar.add(TMPFOLDER)
	tar.close()
	print 'Your archive is ready -> '+TMPFOLDER+'.tar.bz2'

	if os.path.exists(TMPFOLDER): shutil.rmtree(TMPFOLDER)

	sys.exit(0)

def DistClean():
	"clean the project"
	import os, shutil, types
	import Build

	# execute the user-provided distclean function
	try:
		if not Utils.g_module.distclean(): sys.exit(0)
	except AttributeError:
		pass

	# remove the temporary files
	# the builddir is given by lock-wscript only
	# we do no try to remove it if there is no lock file (rmtree)
	for (root, dirs, filenames) in os.walk('.'):
		for f in list(filenames):
			to_remove = 0
			if f==g_lockfile:
				# removes a lock, and the builddir indicated
				to_remove = True
				try:
					proj = read_cache_file(os.path.join(root, f))
					shutil.rmtree(os.path.join(root, proj['blddir']))
				except OSError: pass
				except IOError: pass
			elif f.endswith('~'): to_remove = 1
			elif f.endswith('.pyc'): to_remove = 1
			elif f.startswith('.wafpickle'): to_remove = 1

			if to_remove:
				#print "removing ",os.path.join(root, f)
				os.remove(os.path.join(root, f))
	lst = os.listdir('.')
	for f in lst:
		if f.startswith('.waf-'):
			try: shutil.rmtree(f)
			except: pass
	sys.exit(0)

