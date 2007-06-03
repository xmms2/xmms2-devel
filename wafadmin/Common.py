#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Important functions: install_files, install_as, symlink_as (destdir is taken into account)"

import os, types, shutil, glob
import Params, Utils
from Params import error, fatal

class InstallError:
	pass

def check_dir(dir):
	#print "check dir ", dir
	try:
		os.stat(dir)
	except OSError:
		try:
			os.makedirs(dir)
		except OSError:
			fatal("Cannot create folder " + dir)

def do_install(src, tgt, chmod=0644):
	if Params.g_commands['install']:
		# check if the file is already there to avoid a copy
		_do_install = 1
		if not Params.g_options.force:
			try:
				t1 = os.stat(tgt).st_mtime
				t2 = os.stat(src).st_mtime
				if t1 >= t2: _do_install = 0
			except OSError:
				_do_install = 1

		if _do_install:
			srclbl = src
			try:
				srclbl = src.replace(Params.g_build.m_bldnode.abspath(None)+os.sep, '')
				srclbl = src.replace(Params.g_build.m_srcnode.abspath(None)+os.sep, '')
			except:
				pass
			print "* installing %s as %s" % (srclbl, tgt)
			try: os.remove(tgt) # <- stuff for shared libs and stale inodes
			except OSError: pass
			try:
				shutil.copy2(src, tgt)
				os.chmod(tgt, chmod)
			except IOError:
				try:
					os.stat(src)
				except IOError:
					error('file %s does not exist' % str(src))
				fatal('Could not install the file %s' % str(tgt))
	elif Params.g_commands['uninstall']:
		print "* uninstalling %s" % tgt

		Params.g_build.m_uninstall.append(tgt)

		try: os.remove(tgt)
		except OSError: pass

def path_install(var, subdir, env=None):
	bld = Params.g_build
	if not env: env=Params.g_build.m_allenvs['default']
	destpath = env[var]
	if not destpath:
		error("Installing: to set a destination folder use env['%s']" % (var))
		destpath = var
	destdir = env.get_destdir()
	if destdir: destpath = os.path.join(destdir, destpath.lstrip(os.sep))
	if subdir: destpath = os.path.join(destpath, subdir.lstrip(os.sep))

	return destpath

def install_files(var, subdir, files, env=None, chmod=0644):
	if (not Params.g_commands['install']) and (not Params.g_commands['uninstall']): return

	bld = Params.g_build
	if not env: env=Params.g_build.m_allenvs['default']
	node = bld.m_curdirnode

	if type(files) is types.StringType:
		if '*' in files:
			gl = node.abspath()+os.sep+files
			lst = glob.glob(gl)
		else:
			lst = files.split()
	else: lst=files

	destpath = env[var]
	if not destpath:
		error("Installing: to set a destination folder use env['%s']" % (var))
		destpath = var

	destdir = env.get_destdir()
	if destdir: destpath = os.path.join(destdir, destpath.lstrip(os.sep))
	if subdir: destpath = os.path.join(destpath, subdir.lstrip(os.sep))

	check_dir(destpath)

	# copy the files to the final destination
	for filename in lst:
		if not os.path.isabs(filename):
			alst = Utils.split_path(filename)
			filenode = node.find_build_lst(alst, create=1)

			file     = filenode.abspath(env)
			destfile = os.path.join(destpath, filenode.m_name)
		else:
			file     = filename
			alst     = Utils.split_path(filename)
			destfile = os.path.join(destpath, alst[-1])

		do_install(file, destfile, chmod=chmod)

def install_as(var, destfile, srcfile, env=None, chmod=0644):
	if (not Params.g_commands['install']) and (not Params.g_commands['uninstall']): return

	bld = Params.g_build
	if not env: env=Params.g_build.m_allenvs['default']
	node = bld.m_curdirnode

	tgt = env[var]
	destdir = env.get_destdir()
	if destdir: tgt = os.path.join(destdir, tgt.lstrip(os.sep))
	tgt = os.path.join(tgt, destfile.lstrip(os.sep))

	dir, name = os.path.split(tgt)
	check_dir(dir)

	# the source path
	if not os.path.isabs(srcfile):
		alst = Utils.split_path(srcfile)
		filenode = node.find_build_lst(alst, create=1)
		src = filenode.abspath(env)
	else:
		src = srcfile

	do_install(src, tgt, chmod=chmod)

def symlink_as(var, src, dest, env=None):
	if (not Params.g_commands['install']) and (not Params.g_commands['uninstall']): return

	bld = Params.g_build
	if not env: env=Params.g_build.m_allenvs['default']
	node = bld.m_curdirnode

	tgt = env[var]
	destdir = env.get_destdir()
	if destdir: tgt = os.path.join(destdir, tgt.lstrip(os.sep))
	tgt = os.path.join(tgt, dest.lstrip(os.sep))

	dir, name = os.path.split(tgt)
	check_dir(dir)

	if Params.g_commands['install']:
		try:
			print "* symlink %s (-> %s)" % (tgt, src)
			os.symlink(src, tgt)
			return 0
		except OSError:
			return 1
	elif Params.g_commands['uninstall']:
		try:
			print "* removing %s" % (tgt)
			os.remove(tgt)
			return 0
		except OSError:
			return 1

