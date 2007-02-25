import Action
import Node
from Params import error

xsubpp_str = '${PERL} ${XSUBPP} -noprototypes -typemap ${EXTUTILS_TYPEMAP} ${SRC} > ${TGT}'

def xsubpp_file(self, node):
    gentask = self.create_task('xsubpp', nice=1)
    gentask.set_inputs(node)
    gentask.set_outputs(node.change_ext('.c'))

    cctask = self.create_task('cc')
    cctask.set_inputs(gentask.m_outputs)
    cctask.set_outputs(node.change_ext('.o'))

def setup(env):
    Action.simple_action('xsubpp', xsubpp_str, color='BLUE')
    env.hook('cc', 'PERLXS_EXT', xsubpp_file)

def detect(conf):
        conf.env['PERLXS_EXT'] = ['.xs']
        return 1
