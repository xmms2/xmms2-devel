import Action
import Node
from Params import error

pyrex_str = '${PYREX} ${SRC} -o ${TGT}'

def pyrex_file(self, node):
    gentask = self.create_task('pyrexc', nice=1)
    gentask.set_inputs(node)
    gentask.set_outputs(node.change_ext('.c'))

    cctask = self.create_task('cc')
    cctask.set_inputs(gentask.m_outputs)
    cctask.set_outputs(node.change_ext('.o'))

def setup(env):
    Action.simple_action('pyrexc', pyrex_str, color='BLUE')
    env.hook('cc', 'PYREX_EXT', pyrex_file)

def detect(conf):
        python = conf.find_program('pyrexc', var='PYREX')
        if not python:
            return 0
        conf.env['PYREX_EXT'] = ['.pyx']
        return 1

