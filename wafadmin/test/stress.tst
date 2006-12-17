#! /usr/bin/env python

# loading an environment

import Params
import Runner
pexec = Runner.exec_command

Params.set_trace(0, 0, 0)

# constants
sconstruct_x = """
bld.set_srcdir('.')
bld.set_bdir('_build_')
bld.scandirs('src ')
from Common import dummy
add_subdir('src')
"""

sconscript_0="""
obj=dummy()
obj.source='dummy.h'
obj.target='dummy.i'
"""

sconscript_1="""
i=0
while i<1000:
	i+=1
	obj=dummy()
	obj.source='dummy.h'
	obj.target='dummy.i'
"""

sconscript_2="""
i=0
while i<10000:
	i+=1
	obj=dummy()
	obj.source='dummy.h'
	obj.target='dummy.i'
"""

def runscript(scriptname):
	# clean before building
	pexec('rm -rf runtest/ && mkdir -p runtest/src/')

	# write our files
	dest = open('./runtest/sconstruct', 'w')
	dest.write(sconstruct_x)
	dest.close()

	dest = open('./runtest/src/sconscript', 'w')
	dest.write(scriptname)
	dest.close()

	dest = open('./runtest/src/dummy.h', 'w')
	dest.write('content')
	dest.close()

	# now that the files are there, run the app
	Params.set_trace(0,0,0)

	os.chdir('runtest')
	sys.path.append('..')
	import Scripting

	import time
	t1=time.clock()
	Scripting.Main()
	t2=time.clock()

	os.chdir('..')
	Params.set_trace(1,1,1)

	return (t2-t1)

#t=runscript(sconscript_1)
#print "* posting 1000  objects ",t," seconds (1000  times the same)"
#t=runscript(sconscript_2)
#print "* posting 10000 objects ",t," seconds (10000 times the same)"


def runscript2(scriptname, howmany):
	# clean before building
	pexec('rm -rf runtest/ && mkdir -p runtest/src/')

	sc = open('./runtest/sconstruct', 'w')
	sc.write("""
bld.set_srcdir('.')
bld.set_bdir('_build_')
bld.scandirs('""")

	i=0
	while i<howmany:
		if i<1: sc.write('src'+str(i))
		else: sc.write(' src'+str(i))
		i+=1

	sc.write("""')
from Common import dummy
""")

	i=0
	while i<howmany:
		sc.write('add_subdir("src'+str(i)+'")\n')
		i+=1

	sc.write("""
import Params
#Params.set_trace(0,0,0)""")
	sc.close()

	# write our files
	i=0
	while i<howmany:
		pexec('mkdir -p runtest/src'+str(i))

		dest = open('./runtest/src'+str(i)+'/sconscript', 'w')
		dest.write(scriptname)
		dest.close()

		dest = open('./runtest/src'+str(i)+'/dummy.h', 'w')
		dest.write('content')
		dest.close()
		i+=1

	# now that the files are there, run the app
	
	#Params.set_trace(0,0,0)

def measure():
	os.chdir('runtest')
	sys.path.append('..')
	import Scripting

	import time
	t1=time.clock()
	Scripting.Main()
	t2=time.clock()

	os.chdir('..')
	
	return (t2-t1)

val=750
runscript2(sconscript_0, val)
t=measure()
print "* posted %d objects in %.2f seconds (%d different ones)" % (val, t, val)

import Utils

Utils.reset()
t=measure()
print "* posted %d objects in %.2f seconds (%d different ones, second run)" % (val, t, val)

Utils.reset()
t=measure()
print "* posted %d objects in %.2f seconds (%d different ones, third run)" % (val, t, val)


# cleanup
info("stress test end")
#pexec('rm -rf runtest/')

