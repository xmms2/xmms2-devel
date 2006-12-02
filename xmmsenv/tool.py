import sys

def add_install_flag(obj):
	if not sys.platform == 'darwin':
		return
	obj.env["LINKFLAGS_xlibs"] = obj.env["LINKFLAGS_xlibs"][0] % obj.target
	if obj.uselib:
		obj.uselib += ' xlibs'

