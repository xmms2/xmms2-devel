import Action
import Node
from Params import error
import sys

genpy_str = '${PYTHON} ${SRC} -> ${TGT}'

def genpy_file(self, node):
	basefile = node.m_name[:-6]

	gen_script = node.m_parent.get_file("generate-%s.py" % basefile)
	static_src = node.m_parent.get_file("%s.c" % basefile)

	gen_name = "%s-gen.c" % basefile
	gen_src = node.m_parent.get_file(gen_name)
	if not gen_src:
		gen_src = node.m_parent.get_build(gen_name)
	if not gen_src:
		gen_src = Node.Node(gen_name, node.m_parent)
		node.m_parent.append_build(gen_src)

	gentask = self.create_task('genpy', nice=1)
	gentask.set_inputs([gen_script, static_src])
	gentask.set_outputs(gen_src)

	cctask = self.create_task('cc')
	cctask.set_inputs(gen_src)
	cctask.set_outputs(gen_src.change_ext('.o'))

def setup(env):
	Action.simple_action('genpy', genpy_str, color='BLUE')

	env.hook('cc', 'GENPY_EXT', genpy_file)

def detect(conf):
    conf.env['PYTHON'] = sys.executable
    conf.env['GENPY_EXT'] = ['.genpy']
    return 1
