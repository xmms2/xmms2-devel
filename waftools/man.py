from waflib import Task, Errors
import os
import gzip

from waflib.TaskGen import feature,before_method

def gzip_func(task):
    infile = task.inputs[0].abspath()
    outfile = task.outputs[0].abspath()

    input = None
    outf = None
    output = None
    try:
        input = open(infile, 'rb')
        outf = open(outfile, 'wb')
        output = gzip.GzipFile(os.path.basename(infile), fileobj=outf)
        output.write(input.read())
    finally:
        if input:
            input.close()
        if output: # Must close before outf to flush compressed data.
            output.close()
        if outf:
            outf.close()

Task.task_factory('man', gzip_func, color='BLUE')

@feature('man')
@before_method('process_source')
def process_man(self):
    source = self.to_nodes(getattr(self, 'source', []))
    self.source = []

    section = getattr(self, 'section', None)

    for node in source:
        if not node:
            raise Errors.BuildError('cannot find input file %s for processing' % x)

        # s = section or node.name.rpartition('.')[2]
        s = section or "." in node.name and node.name.rsplit(".", 1)[1]
        if not s:
            raise Errors.BuildError('cannot determine man section from filename')

        out = self.path.find_or_declare(node.name + '.gz')

        tsk = self.create_task('man')
        tsk.set_inputs(node)
        tsk.set_outputs(out)

        self.bld.install_files('${MANDIR}/man%s' % s, out)


def configure(conf):
    return True
