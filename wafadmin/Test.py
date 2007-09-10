#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Some waf tests - most are obsolete"

import Params, os

def info(msg):
	Params.pprint('CYAN', msg)

def testname(file, tests_dir='test'):
	test_file=os.path.join(tests_dir, file)
	return open(test_file, 'r')

def run_tests():
	from test import build_dir
	info("******** build dir tests ********")
	build_dir.run_tests()
	for i in ['dist','configure','clean','distclean','make','install','doc']:
		Params.g_commands[i]=0
	info("******** node path tests ********")
	exec testname('paths.tst', os.path.join('wafadmin', 'test'))
	info("******** scheduler and task tests ********")
	exec testname('scheduler.tst', os.path.join('wafadmin', 'test'))

if __name__ == "__main__":

	for i in ['dist','configure','clean','distclean','make','install','doc']:
		Params.g_commands[i]=0

	#exec testname('paths.tst')
	#exec testname('environment.tst')

	exec testname('scheduler.tst')

	#exec testname('configure.tst')
	#exec testname('stress.tst')
	#exec testname('stress2.tst')
	#exec testname('scanner.tst')
	#exec testname('cpptest.tst') # redundant, the demo does it better

