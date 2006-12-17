#! /usr/bin/env python

# buildtargets in order

import Runner
pexec = Runner.exec_command

pexec('rm -rf runtest/ && mkdir -p runtest/')

# cleanup
info("deps test end")
pexec('rm -rf runtest/')

