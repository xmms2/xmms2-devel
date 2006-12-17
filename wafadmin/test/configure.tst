#! /usr/bin/env python

# loading an environment

import Runner
pexec = Runner.exec_command

pexec('rm -rf waf/ && mkdir -p waf/')

config={'modules':'conftest'}

dest = open('./waf/conftest.py', 'w')
dest.write('import sys\n')
dest.write('def exists(env):\n')
dest.write('   return True\n')
dest.write('def generate(env):\n')
dest.write('   if sys.platform == "linux1" or sys.platform == "linux2":\n')
dest.write('      env.setValue("OS","linux")\n')
dest.write('   elif sys.platform == "win32":\n')
dest.write('		env.setValue("OS","win32")\n')
dest.write('   elif sys.platform == "mac":\n')
dest.write('		env.setValue("OS","darwin")\n')
dest.write('   else:\n')
dest.write('      print "Error: Platform %s is not supported"\n')
dest.write('      sys.exit(1)\n')
dest.write('   return env\n')
dest.close()

import Configure
conf=Configure.Configure(config)
import time
t1=time.clock()
conf.execute()
t2=time.clock()

# cleanup
info("* reading and writing the configure test took %.2f seconds" % (t2-t1))
info("configure test end")
pexec('rm -rf waf/')

