#! /usr/bin/env python

import Runner

pexec = Runner.exec_command

# scan folders, print relative paths between nodes
pexec('rm -rf runtest')
pexec('mkdir -p runtest/src/blah')
pexec('mkdir -p runtest/src/blah2')
pexec('mkdir -p runtest/tst/bleh')
pexec('mkdir -p runtest/tst/bleh2')

info("> 1")
bld = Build.Build()
bld.load_dirs('runtest', 'runtest/_build_')

info("> 2, check a path under srcnode")
srcnode = Params.g_build.m_srcnode
tstnode = Params.g_build.m_srcnode.find_node(['src','blah'])
print tstnode.relpath_gen(srcnode)
	
info("> 3, check two different paths")
tstnode2 = Params.g_build.m_srcnode.find_node(['tst','bleh2'])
print tstnode.relpath_gen(tstnode2)

info("> 4, check srcnode against itself")
print srcnode.relpath_gen(srcnode)

# cleanup
info("paths test end")
pexec('rm -rf runtest .dblite _build_')


