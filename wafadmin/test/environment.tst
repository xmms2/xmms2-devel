#! /usr/bin/env python

# loading an environment

import Runner
pexec = Runner.exec_command

pexec('rm -rf runtest/ && mkdir -p runtest/')

dest = open('./runtest/cache.txt', 'w')
i=0
while i<100:
	dest.write('QT4_CACHED%d = 1\n'%i)
	dest.write('KDE4_CACHED%d = True\n'%i)
	dest.write('EXTRAINCLUDES%d = None\n'%i)
	dest.write("LIB_QT%d = ['QtGui', 'pthread', 'Xext', 'z', 'png', 'm', 'z', 'X11', 'SM', 'ICE']\n"%i)
	dest.write('LIB_QT%d = ["QtGui", "pthread", "Xext", "z", "png", "m", "z", "X11", "SM", "ICE"]\n'%i)
	dest.write('CXXFLAGS_OPENSSL%d=[]\n'%i)
	dest.write('DCOPIDL2CPP%d = "/home1/kde4svn/KDE/kdelibs/build/dcop/dcopidl2cpp/dcopidl2cpp"\n'%i)
	dest.write('HASHTABLE%d = {"A":1, "B":2, "C":3}\n'%i)
	dest.write('HASHTABLE2%d = {"STRING1":"STRING2", "STRING3":"STRING4"}\n'%i)
	dest.write('BLANK%d = ""\n'%i)
	i+=1
dest.close()
 
env=Environment.Environment()
import time
t1=time.clock()
env.load('./runtest/cache.txt')
env.store('./runtest/cache.txt')
t2=time.clock()
 
# cleanup
info("* reading and writing %d environment variables took %.2f seconds" % (1000, t2-t1))
info("environment test end")
pexec('rm -rf runtest/')

