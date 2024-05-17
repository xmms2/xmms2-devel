#! /usr/bin/env python
# encoding: utf-8

import os
import re
from waflib import Task,Errors,Utils
from waflib.Configure import conf
from waflib.TaskGen import extension,feature,before_method


@feature('perldoc')
@before_method('process_source')
def perldoc_file(self):
	source = self.to_nodes(getattr(self, 'source', []))
	self.source = []

	for node in source:
		if not node:
			raise Errors.BuildError('cannot find input file for processing')

		match = re.search(r"MODULE\s*=\s*(\S+)", node.read())
		if match is None:
			raise Errors.BuildError('.xs file does not contain a MODULE declaration')
		module = match.group(1)

		path = module.replace("::", os.sep) + ".pod"
		outnode = node.parent.find_or_declare(path)
		self.create_task('perldoc',node,outnode)

		base_path = getattr(self, 'install_path', "${INSTALLDIR_PERL_LIB}")
		self.bld.install_files(os.path.join(base_path, os.path.dirname(path)), outnode)

class perldoc(Task.Task):
	run_str='${PODSELECT} ${SRC} > ${TGT}'
	color='BLUE'
	ext_in=['.xs']
	ext_out=['.pod']

def configure(ctx):
	ctx.find_program('podselect', var='PODSELECT')
