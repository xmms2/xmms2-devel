import TaskGen, Build, Utils, Task
#import Utils, Node
import sys, os
import gzip
from misc import copy_taskgen

cls = Task.simple_task_type('man', 'foo', color='YELLOW')
cls.maxjobs = 1

import sys, os, gzip
import Utils, misc
from TaskGen import feature

def gzip_func(task):
    env = task.env
    infile = task.inputs[0].abspath(env)
    outfile = task.outputs[0].abspath(env)

    input = open(infile, 'r')
    output = gzip.GzipFile(outfile, mode='w')
    output.write(input.read())

@feature('man')
def process_man(self):
    if not getattr(self, 'files', None):
        return

    for x in self.to_list(self.files):
        node = self.path.find_resource(x)
        if not node:
            raise Build.BuildError('cannot find input file %s for processing' % x)

        target = self.target
        if not target:
            target = node.name

        out = self.path.find_or_declare(x + '.gz')

        tsk = self.create_task('copy')
        tsk.set_inputs(node)
        tsk.set_outputs(out)
        tsk.fun = gzip_func
        tsk.install_path = '${MANDIR}/man' + getattr(self, 'section', '1')
        tsk.color = 'BLUE'

#def setup(env):
#    Object.register('man', manobj)

def detect(conf):
    return 1
