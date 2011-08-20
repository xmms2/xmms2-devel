import TaskGen, Build, Utils, Task
import sys, os
import gzip

#cls = Task.simple_task_type('man', 'foo', color='YELLOW')
#cls.maxjobs = 1

from TaskGen import feature

def gzip_func(task):
    infile = task.inputs[0].abspath()
    outfile = task.outputs[0].abspath()

    input = open(infile, 'rb')
    outf = open(outfile, 'wb')
    output = gzip.GzipFile(os.path.basename(infile), fileobj=outf)
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

def configure(conf):
    return True
