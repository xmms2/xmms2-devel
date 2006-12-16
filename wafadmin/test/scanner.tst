#! /usr/bin/env python

# testing scanners

import Runner
pexec = Runner.exec_command

# constants
wscript_top = """
VERSION='0.0.1'
APPNAME='cpp_test'

# these variables are mandatory ('/' are converted automatically)
srcdir = '.'
blddir = '_build_'

def build(bld):
	bld.add_subdirs('src src2')

def configure(conf):
	conf.checkTool(['g++'])

	conf.env['CXXFLAGS_MYPROG']='-O3'
	conf.env['LIB_MYPROG']='m'
	conf.env['SOME_INSTALL_DIR']='/tmp/ahoy/lib/'

def set_options(opt):
	opt.add_option('--prefix', type='string', help='installation prefix', dest='prefix')
	opt.sub_options('src')

"""

sconscript_0="""
def build(bld):
	import Params
	print "command-line parameter meow is ", Params.g_options.meow

	# 1. A simple program
	obj = bld.createObj('cpp', 'program')
	obj.source='''
	main.cpp
	'''
	obj.includes='. src'
	obj.target='testprogram'

"""

dummy_hfile="""
#include <iostream>
#include <limits.h>

int c = 3;
"""

#
# clean before building anything
#
pexec('rm -rf runtest/ && mkdir -p runtest/src/sub/')

#
# write our files
#
dest = open('./runtest/wscript', 'w')
dest.write(wscript_top)
dest.close()

dest = open('./runtest/src/sconscript', 'w')
dest.write(sconscript_0)
dest.close()

dest = open('./runtest/src/dummy.h', 'w')
dest.write(dummy_hfile)
dest.close()


dest = open('./runtest/src/file1.moc', 'w')
dest.write("file1.moc")
dest.close()

dest = open('./runtest/src/sub/file2.moc', 'w')
dest.write("file2.moc")
dest.close()

dest = open('./runtest/src/sub/file3.moc', 'w')
dest.write("file3.moc")
dest.close()




# now that the files are there, run the app
#Params.set_trace(0,0,0)

#
# change the current directory to 'runtest'
#
os.chdir('runtest')
sys.path.append('..')
import Scripting

#
# load the build application
#
bld = Build.Build()
bld.load()
bld.set_srcdir('.')
bld.set_bdir('_build_')

bld.scandirs('src src/sub/')

src_node = bld.m_tree.m_srcnode
path='src/dummy.h'.split('/')
hfile = src_node.find_node( path )

runtest_src     = src_node.find_node(['src'])
runtest_src_sub = runtest_src.find_node(['sub'])

import Scan
lst = Scan.c_scanner(hfile, [runtest_src, runtest_src_sub, src_node])
print "* list of nodes found by scanning ", lst

#bld.m_tree.dump()

#
# close the build application
#
bld.cleanup()
bld.store()

#Params.set_trace(1,1,1)

#
# cleanup
#
info("scanner test end")
os.chdir('..')
#pexec('rm -rf runtest/')

