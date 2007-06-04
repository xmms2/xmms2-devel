import Action
import Node
from Params import error
import sys

genpy_str = '${PYTHON} ${SRC} -> ${TGT}'

def genpy_file(self, node):
    gentask = self.create_task('genpy', nice=1)
    gentask.set_inputs([node, node.change_ext('.head.c')])
    gentask.set_outputs(node.change_ext('.c'))

    cctask = self.create_task('cc')
    cctask.set_inputs(gentask.m_outputs)
    cctask.set_outputs(node.change_ext('.o'))

def setup(env):
    Action.simple_action('genpy', genpy_str, color='BLUE')

    env.hook('cc', 'GENPY_EXT', genpy_file)

def detect(conf):
    conf.env['PYTHON'] = sys.executable
    conf.env['GENPY_EXT'] = ['.genpy']
    return 1
