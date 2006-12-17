#! /usr/bin/env python

# loading an environment

import time
import Params
import Runner
import Utils
pexec = Runner.exec_command

Params.set_trace(1, 1, 1)
#Params.set_trace(0, 0, 0)

# constants
cache_x = """
AR = '/usr/bin/ar'
ARFLAGS = 'r'
CXX = '/home/ita/bin/g++'
CXXFLAGS = '-O2'
CXX_ST = '%s -c -o %s'
DESTDIR = '/tmp/blah/'
LIB = []
LIBSUFFIX = '.so'
LINK = '/home/ita/bin/g++'
LINKFLAGS = []
LINK_ST = '%s -o %s'
PREFIX = '/usr'
RANLIB = '/usr/bin/ranlib'
RANLIBFLAGS = ''
_CPPDEFFLAGS = ''
_CXXINCFLAGS = ''
_LIBDIRFLAGS = ''
_LIBFLAGS = ''
program_obj_ext = ['.o']
shlib_CXXFLAGS = ['-fPIC', '-DPIC']
shlib_LINKFLAGS = ['-shared']
shlib_PREFIX = 'lib'
shlib_SUFFIX = '.so'
shlib_obj_ext = ['.os']
staticlib_LINKFLAGS = ['-Wl,-Bstatic']
staticlib_PREFIX = 'lib'
staticlib_SUFFIX = '.a'
staticlib_obj_ext = ['.o']
"""

sconstruct_x = """
bld.set_srcdir('.')
bld.set_bdir('_build_')
bld.set_default_env('main.cache.py')

bld.scandirs('src src/subdir')

from Common import cppobj

add_subdir('src')
"""

sconscript_0="""
#print 'test script for creating testprogram is read'
obj=cppobj('program')
obj.source='''
a.cpp b.cpp main.cpp subdir/doh.cpp
'''
obj.includes='. src'
obj.target='testprogram'
"""

# clean before building
pexec('rm -rf runtest/ && mkdir -p runtest/src/subdir/')

dest = open('./runtest/sconstruct', 'w')
dest.write(sconstruct_x)
dest.close()

dest = open('./runtest/src/sconscript', 'w')
dest.write(sconscript_0)
dest.close()

dest = open('./runtest/main.cache.py', 'w')
dest.write(cache_x)
dest.close()

# cpp files
dest = open('./runtest/src/a.h', 'w')
dest.write('#ifndef A_H\n#define A_H\n')
dest.write('#include "a2.h"\nclass a { public: a(); int a_1; int a_2; a2* tst; };\n')
dest.write('#endif')
dest.close()

dest = open('./runtest/src/a.cpp', 'w')
dest.write('#include "a.h"\n a::a() { a_1=3; a_2=4; }\n')
dest.close()

dest = open('./runtest/src/a2.h', 'w')
dest.write('#ifndef A2_H\n#define A2_H\n')
dest.write('class a2 { public: int truc; };\n')
dest.write('#endif\n')
dest.close()

dest = open('./runtest/src/b.h', 'w')
dest.write('#include "a.h"\n')
dest.write('class b : public a { public: b(); };\n')
dest.close()

dest = open('./runtest/src/b.cpp', 'w')
dest.write('#include "b.h"\n')
dest.write('b::b() { }\n')
dest.close()

dest = open('./runtest/src/subdir/doh.cpp', 'w')
dest.write('static int k=7;\n')
dest.close()

dest = open('./runtest/src/main.cpp', 'w')
dest.write('#include "a.h"\n')
dest.write('#include "b.h"\n')
dest.write('#include <iostream>\nusing namespace std;')
dest.write('int main() { cout<<"hello, world"<<endl; return 0; }\n')
dest.close()

# now build a program with all that

Params.g_trace_exclude = "Action Common Deptree Node Option Object Scripting Build Configure Environment KDE Scan Utils Task".split()
#Params.g_trace_exclude = "Action Deptree Node Option Build Configure Environment Task".split()
#Params.g_trace_exclude = "Action Deptree Node Option Build Configure Environment".split()
#Params.g_trace_exclude = "Configure Environment".split()

def measure():
	# now that the files are there, run the app
	#Params.set_trace(0,0,1)

	os.chdir('runtest')
	sys.path.append('..')
	import Scripting

	t1=time.clock()
	Scripting.Main()
	t2=time.clock()

	os.chdir('..')
	#Params.set_trace(1,1,1)

	return (t2-t1)

def check_tasks_done(lst):
	done = map( lambda a: a.m_idx, Params.g_done )
	ok=1
	for i in lst:
		if i not in done:
			error("found a task that has not run" + str(i))
			ok=0
	for i in done:
		if i not in lst:
			error("found a task that has run when it should not have" + str(i))
			ok=0
	if not ok:
		error(" -> test failed, correct the errors !")
	if ok:
		Params.pprint("GREEN", " -> test successful\n")

def modify_file(file):
	dest = open(file, 'a')
	dest.write('/* file is modified */ \n')
	dest.close()

Params.g_fake=0

info("try to build a c++ program")
t=measure()

#print os.listdir('.')
res = os.popen('./runtest/_build_/src/testprogram').read().strip()
if res=='hello, world':
	Params.pprint('GREEN', '-> Program successfully executed')
else:
	Params.pprint('RED', 'An error occured, this test failed')

# cleanup
info("cpp test end")
#pexec('rm -rf runtest/')

