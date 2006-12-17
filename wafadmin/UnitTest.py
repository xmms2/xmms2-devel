#! /usr/bin/env python
# encoding: utf-8
# Carlos Rafael Giani, 2006

"""
Unit tests run in the shutdown() method, and for c/c++ programs

One should NOT have to give parameters to programs to execute

In the shutdown method, add the following code:

	>>> def shutdown():
	...	ut = UnitTest.unit_test()
	...	ut.run()
	...	ut.print_results()


Each object to use as a unit test must be a program and must have X{obj.unit_test=1}
"""

import Params, Object, pproc

class unit_test:
	"Unit test representation"
	def __init__(self):
		self.returncode_ok = 0		# Unit test returncode considered OK. All returncodes differing from this one
						# will cause the unit test to be marked as "FAILED".

		# The following variables are filled with data by run().

		# print_results() uses these for printing the unit test summary,
		# but if there is need for direct access to the results,
		# they can be retrieved here, after calling run().

		self.num_tests_ok = 0		# Number of successful unit tests
		self.num_tests_failed = 0	# Number of failed unit tests
		self.num_tests_err = 0		# Tests that have not even run
		self.total_num_tests = 0	# Total amount of unit tests
		self.max_label_length = 0	# Maximum label length (pretty-print the output)

		self.unit_tests = {}		# Unit test dictionary. Key: the label (unit test filename relative
						# to the build dir), value: unit test filename with absolute path
		self.unit_test_results = {}	# Dictionary containing the unit test results.
						# Key: the label, value: result (true = success false = failure)
		self.unit_test_erroneous = {}	# Dictionary indicating erroneous unit tests.
						# Key: the label, value: true = unit test has an error  false = unit test is ok

	def run(self):
		"Run the unit tests and gather results (note: no output here)"

		self.num_tests_ok = 0
		self.num_tests_failed = 0
		self.num_tests_err = 0
		self.total_num_tests = 0
		self.max_label_length = 0

		self.unit_tests = {}
		self.unit_test_results = {}
		self.unit_test_erroneous = {}

		# If waf is not building, don't run anything
		try:
			if not Params.g_commands['build']: return
		except:
			pass

		# Gather unit tests to call
		for obj in Object.g_allobjs:
			if not hasattr(obj,'unit_test'): continue
			unit_test = getattr(obj,'unit_test')
			if not unit_test: continue
			try:
				if obj.m_type == 'program':
					filename = obj.m_linktask.m_outputs[0].abspath(obj.env)
					label = obj.m_linktask.m_outputs[0].bldpath(obj.env)
					self.max_label_length = max(self.max_label_length, len(label))
					self.unit_tests[label] = filename
			except:
				pass
		self.total_num_tests = len(self.unit_tests)
		# Now run the unit tests
		for label, filename in self.unit_tests.iteritems():
			try:
				pp = pproc.Popen(filename, stdout = pproc.PIPE, stderr = pproc.PIPE) # PIPE for ignoring output
				pp.wait()

				result = int(pp.returncode == self.returncode_ok)

				if result: self.num_tests_ok += 1
				else: self.num_tests_failed += 1

				self.unit_test_results[label] = result
				self.unit_test_erroneous[label] = 0
			except:
				self.unit_test_erroneous[label] = 1
				self.num_tests_err += 1

	def print_results(self):
		"Pretty-prints a summary of all unit tests, along with some statistics"

		# If waf is not building, don't output anything
		try:
			if not Params.g_commands['build']: return
		except:
			pass

		p = Params.pprint
		# Early quit if no tests were performed
		if self.total_num_tests == 0:
			p('YELLOW', 'No unit tests present')
			return
		p('GREEN', 'Running unit tests')
		print

		for label, filename in self.unit_tests.iteritems():
			err = 0
			result = 0

			try: err = self.unit_test_erroneous[label]
			except: pass

			try: result = self.unit_test_results[label]
			except: pass

			n = self.max_label_length - len(label)
			if err: n += 4
			elif result: n += 7
			else: n += 3

			line = '%s %s' % (label, '.' * n)

			print line,
			if err: p('RED', 'ERROR')
			elif result: p('GREEN', 'OK')
			else: p('YELLOW', 'FAILED')

		percentage_ok = float(self.num_tests_ok) / float(self.total_num_tests) * 100.0
		percentage_failed = float(self.num_tests_failed) / float(self.total_num_tests) * 100.0
		percentage_erroneous = float(self.num_tests_err) / float(self.total_num_tests) * 100.0

		print
		print "Successful tests:      %i (%.1f%%)" % (self.num_tests_ok, percentage_ok)
		print "Failed tests:          %i (%.1f%%)" % (self.num_tests_failed, percentage_failed)
		print "Erroneous tests:       %i (%.1f%%)" % (self.num_tests_err, percentage_erroneous)
		print
		print "Total number of tests: %i" % self.total_num_tests
		print
		p('GREEN', 'Unit tests finished')

