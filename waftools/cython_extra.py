# encoding: utf-8
# Extend waf cython tool features.

import re
from waflib import Configure
from subprocess import Popen, STDOUT, PIPE

# Borrowed from Python 2.7, subprocess.py
class CalledProcessError(Exception):
    """This exception is raised when a process run by check_call() or
    check_output() returns a non-zero exit status.
    The exit status will be stored in the returncode attribute;
    check_output() will also store the output in the output attribute.
    """
    def __init__(self, returncode, cmd, output=None):
        self.returncode = returncode
        self.cmd = cmd
        self.output = output
    def __str__(self):
        return "Command '%s' returned non-zero exit status %d" % (self.cmd, self.returncode)

# Borrowed from Python 2.7, subprocess.py
def check_output(*popenargs, **kwargs):
    if 'stdout' in kwargs:
        raise ValueError('stdout argument not allowed, it will be overridden.')
    process = Popen(stdout=PIPE, *popenargs, **kwargs)
    output, unused_err = process.communicate()
    retcode = process.poll()
    if retcode:
        cmd = kwargs.get("args")
        if cmd is None:
            cmd = popenargs[0]
        raise CalledProcessError(retcode, cmd, output=output)
    return output

cython_ver_re = re.compile('Cython version ([0-9.]+)')
def check_cython_version(self, version=None, minver=None, maxver=None):
	log_s = []
	if version:
		log_s.append("version=%r"%version)
		minver=version
		maxver=version
	else:
		if minver:
			log_s.append("minver=%r"%minver)
		if maxver:
			log_s.append("maxver=%r"%maxver)
	if isinstance(minver, str):
		minver = minver.split('.')
	if isinstance(maxver, str):
		maxver = maxver.split('.')
	if minver:
		minver = tuple(map(int, minver))
	if maxver:
		maxver = tuple(map(int, maxver))
	
	# Trick to be compatible python 2.x and 3.x
	try:
		u = unicode
	except NameError:
		u = str

	cmd = [self.env.CYTHON[0], '--version']
	o = u(check_output(cmd, stderr=STDOUT), 'UTF-8').strip()
	m = cython_ver_re.match(o)
	self.start_msg('Checking for cython version')
	if not m:
		self.end_msg('Not found', 'YELLOW')
		self.fatal("No version found")
	else:
		v = m.group(1)
		ver = tuple(map(int, v.split('.')))
		check = (not minver or minver <= ver) and (not maxver or maxver >= ver)
		self.to_log('  cython %s\n  -> %r\n' % (" ".join(log_s), v))
		if check:
			self.end_msg(v)
			self.env.CYTHON_VERSION = ver
		else:
			self.end_msg('wrong version %s' % v, 'YELLOW')
			self.fatal("Bad cython version (%s)"%v)
	return True


def configure(conf):
	if not conf.env.CYTHON:
		conf.fatal("cython_extra requires the tool 'cython' to be loaded first.")
	Configure.conf(check_cython_version)
	
