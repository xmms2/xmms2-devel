import Action
import Node
from Params import error

pyrex_str = '${PYREX} ${SRC} -o ${TGT}'

def pyrex_file(self, node):
    basefile = node.m_name[:-4]

    gen_script = node.m_parent.get_file(node.m_name)

    gen_name = "%s.c" % basefile
    gen_src = node.m_parent.get_file(gen_name)
    if not gen_src:
        gen_src = node.m_parent.get_build(gen_name)
    if not gen_src:
        gen_src = Node.Node(gen_name, node.m_parent)
        node.m_parent.append_build(gen_src)

    gentask = self.create_task('pyrexc', nice=1)
    gentask.set_inputs(gen_script)
    gentask.set_outputs(gen_src)

    cctask = self.create_task('cc')
    cctask.set_inputs(gen_src)
    cctask.set_outputs(gen_src.change_ext('.o'))

def setup(env):
    Action.simple_action('pyrexc', pyrex_str, color='BLUE')
    env.hook('cc', 'PYREX_EXT', pyrex_file)

def detect(conf):
        python = conf.find_program('pyrexc', var='PYREX')
        if not python:
            error("Could not find pyrexc. You loose?")
            return 0
        conf.env['PYREX_EXT'] = ['.pyx']
        return 1


