import Task
from TaskGen import extension
import sys

Task.simple_task_type('genpy', '${PYTHON} ${SRC} > ${TGT}', color='BLUE', before='cc')

@extension(".genpy")
def genpy_file(self, node):
    gentask = self.create_task('genpy')
    gentask.set_inputs(node)
    nd = node.change_ext('.c')
    gentask.set_outputs(nd)
    self.allnodes.append(nd)

def detect(conf):
    conf.env['PYTHON'] = sys.executable
    conf.env['GENPY_EXT'] = ['.genpy']
    return 1
