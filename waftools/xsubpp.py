import Action
import Node
from Params import error

xsubpp_str = '${PERL} ${XSUBPP} -noprototypes -typemap ${EXTUTILS_TYPEMAP} ${SRC} > ${TGT}'

def xsubpp_file(self, node):
    basefile = node.m_name[:-3]

    gen_script = node.m_parent.get_file(node.m_name)

    gen_name = "%s.c" % basefile
    gen_src = node.m_parent.get_file(gen_name)
    if not gen_src:
        gen_src = node.m_parent.get_build(gen_name)
    if not gen_src:
        gen_src = Node.Node(gen_name, node.m_parent)
        node.m_parent.append_build(gen_src)

    gentask = self.create_task('xsubpp', nice=1)
    gentask.set_inputs(gen_script)
    gentask.set_outputs(gen_src)

    cctask = self.create_task('cc')
    cctask.set_inputs(gen_src)
    cctask.set_outputs(gen_src.change_ext('.o'))

def setup(env):
    Action.simple_action('xsubpp', xsubpp_str, color='BLUE')
    env.hook('cc', 'PERLXS_EXT', xsubpp_file)

def detect(conf):
        conf.env['PERLXS_EXT'] = ['.xs']
        return 1
