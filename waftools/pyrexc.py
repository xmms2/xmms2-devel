import Task
from TaskGen import extension

Task.simple_task_type('pyrexc', '${PYREX} ${SRC} -o ${TGT}', color='BLUE', before='cc')

@extension('.pyx')
def pyrex_file(self, node):
    gentask = self.create_task('pyrexc')
    gentask.set_inputs(node)
    nd = node.change_ext('.c')
    gentask.set_outputs(nd)
    self.allnodes.append(nd)

def detect(conf):
    python = conf.find_program('pyrexc', var='PYREX')
    if not python:
        return 0
    conf.env['PYREX_EXT'] = ['.pyx']
    return 1

