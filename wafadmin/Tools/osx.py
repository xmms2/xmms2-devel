#! /usr/bin/env python
# encoding: utf-8

"MacOSX related tools"

import ccroot
import Action
import os
import shutil

from Params import error, debug, fatal, warning

def apply_core_osx(self):
	ccroot.ccroot.apply_core_o(self)
	if self.m_type == 'program':
		apptask = self.create_task('macapp', self.env, 300)
		apptask.set_inputs(self.m_linktask.m_outputs)
		apptask.set_outputs(self.m_linktask.m_outputs[0].change_ext('.app'))
		self.m_apptask = apptask

app_dirs = ['Contents', os.path.join('Contents','MacOS'), os.path.join('Contents','Resources')]

app_info = '''
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist SYSTEM "file://localhost/System/Library/DTDs/PropertyList.dtd">
<plist version="0.9">
<dict>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleGetInfoString</key>
	<string>Created by waf</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>NOTE</key>
	<string>Do not change this file, it will be overwritten by waf.</string>
'''
app_info_foot = '''
</dict>
</plist>
'''

def app_build(task):
	global app_dirs
	env = task.m_env

	i = 0;
	for p in task.m_outputs:
		srcfile = p.srcpath(env)

		debug("creating directories")
		try:
			os.mkdir(srcfile)
			[os.makedirs(os.path.join(srcfile, d)) for d in app_dirs]
		except:
			pass

		# copy the program to the contents dir
		srcprg = task.m_inputs[i].srcpath(env)
		dst = os.path.join(srcfile, 'Contents', 'MacOS')
		debug("copy %s to %s" % (srcprg, dst))
		shutil.copy(srcprg, dst)

		# create info.plist
		debug("generate Info.plist")
		f = file(os.path.join(srcfile, "Contents", "Info.plist"), "w")
		f.write(app_info)
		f.write("\t<key>CFBundleExecutable</key>\n\t<string>%s</string>\n" % os.path.basename(srcprg))
		f.write(app_info_foot)
		f.close()

		i += 1

	return 0

def setup(env):
	ccroot.ccroot.apply_core_o = ccroot.ccroot.apply_core
	ccroot.ccroot.apply_core = apply_core_osx
	Action.Action('macapp', vars=[], func=app_build)

def detect(conf):
	return 1

