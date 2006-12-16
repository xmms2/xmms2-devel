#! /usr/bin/env python

# loading an environment

import Params
import Runner
pexec = Runner.exec_command

Params.set_trace(0, 0, 0)

### to be more realistic, kdelibs has
# * about 20 main source folders
# * about 14 cpp files per folder and 15 headers too
# * about 1400 cpp files (1400 headers too)
# -> about 100 folders containing sources
# -> 20 main folders containing 4 each
# -> only root folders contain scripts


# constants
sconstruct_x = """
bld.set_srcdir('.')
bld.set_bdir('_build_')
from Common import dummy

"""

sconscript_0="""
#print 'script read'
obj=dummy()
obj.source='dummy1-1.cpp'
obj.target='dummy1.i'
"""

# clean before building
pexec('rm -rf runtest/ && mkdir -p runtest/')

# important, parameters for the simulation
want_h_files=1

# write our files
scandirs=['bld.scandirs("""\n']

i=0
while i<20:
	i+=1

	dir = "./runtest/src%d"%i
	pexec('mkdir -p %s' % dir )

	dest = open(dir+'/sconscript', 'w')
	dest.write(sconscript_0)
	dest.close()

	j=0
	while j<14:
		j+=1

		srcname='/dummy%d-%d.cpp'%(i,j)
		dest = open(dir+srcname, 'w')
		dest.write('level 0')
		dest.close()

		if want_h_files:
			srcname='/dummy%d-%d.h'%(i,j)
			dest = open(dir+srcname, 'w')
			dest.write('level 0')
			dest.close()

		scandirs.append( 'src%d\n' % (i) )

	j=0
	while j<4:
		j+=1

		subdir = dir+'/subdir%d'%j
		pexec('mkdir -p %s' % subdir)

		scandirs.append( 'src%d/subdir%d\n' % (i, j) )

		prefix = subdir

		k=0
		while k<14:
			k+=1

			file = prefix+('/dummy%d-%d-%d.cpp'%(i,j,k))
			dest = open(file, 'w')
			dest.write('level 1')
			dest.close()

			if want_h_files:
				file = prefix+('/dummy%d-%d-%d.h'%(i,j,k))
				dest = open(file, 'w')
				dest.write('level 1')
				dest.close()

scandirs.append('""")\n')

dest = open('./runtest/sconstruct', 'w')
dest.write(sconstruct_x)
dest.write( "".join(scandirs) )
dest.write("add_subdir('src1')\n")
dest.close()

def measure():
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

t=measure()
print "measure for simulation %.2f seconds" % t

import Utils
Utils.reset()
t=measure()
print "measure for simulation %.2f seconds (second attempt)" % t

Utils.reset()
t=measure()
print "measure for simulation %.2f seconds (third attempt)" % t


# cleanup
info("stress2 test end")
#pexec('rm -rf runtest/')

