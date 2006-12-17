#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"Additional configuration checks hooked on the configuration class"

import Utils, Configure
from Params import error, fatal

endian_str = """
#include <stdio.h>
int am_big_endian()
{
        long one = 1;
        return !(*((char *)(&one)));
}

int main()
{
  if (am_big_endian())
     printf("bigendian=1\\n");
  else
     printf("bigendian=0\\n");

  return 0;
}
"""

class compile_configurator(Configure.configurator_base):
	"inheritance demo"
	def __init__(self, conf):
		Configure.configurator_base.__init__(self, conf)
		self.name = ''
		self.code = ''
		self.flags = ''
		self.define = ''
		self.uselib = ''
		self.want_message = 0
		self.msg = ''

	def error(self):
		fatal('test program would not run')

	def run_cache(self, retval):
		if self.want_message:
			self.conf.check_message('compile code (cached)', '', 1, option=self.msg)

	def validate(self):
		if not self.code:
			fatal('test configurator needs code to compile and run!')

	def run_test(self):
		obj = Configure.check_data()
		obj.code = self.code
		obj.env  = self.env
		obj.uselib = self.uselib
		obj.flags = self.flags
		ret = self.conf.run_check(obj)

		if self.want_message:
			self.conf.check_message('compile code', '', ret, option=self.msg)

		return ret

def create_compile_configurator(self):
	return compile_configurator(self)

def checkEndian(self, define='', pathlst=[]):
	if define == '': define = 'IS_BIGENDIAN'

	if self.is_defined(define): return self.get_define(define)

	global endian

	test = self.create_test_configurator()
	test.code = endian_str
	code = test.run()['result']

	#code = self.TryRun(endian_str, pathlst=pathlst)

	try:
		t = Utils.to_hashtable(code)
		is_big = int(t['bigendian'])
	except:
		error('endian test failed '+code)
		is_big = 0
		raise

	if is_big: strbig = 'big endian'
	else:      strbig = 'little endian'

	self.check_message_custom('endianness', '', strbig)
	self.add_define(define, is_big)
	return is_big

features_str = """
#include <stdio.h>
int am_big_endian()
{
        long one = 1;
        return !(*((char *)(&one)));
}

int main()
{
  if (am_big_endian())
     printf("bigendian=1\\n");
  else
     printf("bigendian=0\\n");

  printf("int_size=%d\\n", sizeof(int));
  printf("long_int_size=%d\\n", sizeof(long int));
  printf("long_long_int_size=%d\\n", sizeof(long long int));
  printf("double_size=%d\\n", sizeof(double));

  return 0;
}
"""

def checkFeatures(self, lst=[], pathlst=[]):

	global endian

	test = self.create_test_configurator()
	test.code = features_str
	code = test.run()['result']
	#code = self.TryRun(features_str, pathlst=pathlst)

	try:
		t = Utils.to_hashtable(code)
		is_big = int(t['bigendian'])
	except:
		error('endian test failed '+code)
		is_big = 0
		raise

	if is_big: strbig = 'big endian'
	else:      strbig = 'little endian'


	self.check_message_custom('endianness', '', strbig)

	self.check_message_custom('int size', '', t['int_size'])
	self.check_message_custom('long int size', '', t['long_int_size'])
	self.check_message_custom('long long int size', '', t['long_long_int_size'])
	self.check_message_custom('double size', '', t['double_size'])

	self.add_define('IS_BIGENDIAN', is_big)
	self.add_define('INT_SIZE', int(t['int_size']))
	self.add_define('LONG_INT_SIZE', int(t['long_int_size']))
	self.add_define('LONG_LONG_INT_SIZE', int(t['long_long_int_size']))
	self.add_define('DOUBLE_SIZE', int(t['double_size']))

	return is_big

def check_header(self, header, define=''):

	if not define:
		upstr = header.upper().replace('/', '_').replace('.', '_')
		define = 'HAVE_' + upstr

	test = self.create_header_configurator()
	test.name = header
	test.define = define
	return test.run()

def try_build_and_exec(self, code, uselib=''):
	test = self.create_test_configurator()
	test.uselib = uselib
	test.code = code
	ret = test.run()
	if ret: return ret['result']
	return None

def try_build(self, code, uselib='', msg=''):
	test = self.create_compile_configurator()
	test.uselib = uselib
	test.code = code
	if msg:
		test.want_message = 1
		test.msg = msg
	ret = test.run()
	return ret

def check_flags(self, flags, uselib='', options='', msg=1):
	test = self.create_test_configurator()
	test.uselib = uselib
	test.code = 'int main() {return 0;}\n'
	test.flags = flags
	ret = test.run()

	if msg: self.check_message('flags', flags, not (ret is None))

	if ret: return 1
	return None

def setup(env):
	pass


# function wrappers for convenience
def check_header2(self, name, mandatory=1, define=''):
	import os
	ck_hdr = self.create_header_configurator()
	if define: ck_hdr.define = define
	# header provides no fallback for define:
	else: ck_hdr.define = 'HAVE_' + os.path.basename(name).replace('.','_').upper()
	ck_hdr.mandatory = mandatory
	ck_hdr.name = name
	return ck_hdr.run()

def check_library2(self, name, mandatory=1, uselib=''):
	ck_lib = self.create_library_configurator()
	if uselib: ck_lib.uselib = uselib
	ck_lib.mandatory = mandatory
	ck_lib.name = name
	return ck_lib.run()

def check_pkg2(self, name, version, mandatory=1, uselib=''):
	ck_pkg = self.create_pkgconfig_configurator()
	if uselib: ck_pkg.uselib = uselib
	ck_pkg.mandatory = mandatory
	ck_pkg.version = version
	ck_pkg.name = name
	return ck_pkg.run()

def check_cfg2(self, name, mandatory=1, define='', uselib=''):
	ck_cfg = self.create_cfgtool_configurator()
	if uselib: ck_cfg.uselib = uselib
	# cfgtool provides no fallback for uselib:
	else: ck_cfg.uselib = name.upper()
 	ck_cfg.mandatory = mandatory
	ck_cfg.binary = name + '-config'
	return ck_cfg.run()

def detect(conf):
	"attach the checks to the conf object"

	conf.hook(check_header)
	conf.hook(create_compile_configurator)
	conf.hook(try_build)
	conf.hook(try_build_and_exec)
	conf.hook(check_flags)

	# additional methods
	conf.hook(check_header2)
	conf.hook(check_library2)
	conf.hook(check_pkg2)
	conf.hook(check_cfg2)

	# the point of checkEndian is to make an example, the following is better
	# if sys.byteorder == "little":
	conf.hook(checkEndian)
	conf.hook(checkFeatures)

	return 1

