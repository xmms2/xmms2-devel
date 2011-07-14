import Task
import TaskGen
from TaskGen import extension
import sys

TaskGen.declare_chain(
		name='genpy',
		rule='${PYTHON} ${SRC} > ${TGT}',
		reentrant=True,
		color='BLUE',
		ext_in='.genpy',
		ext_out='.c',
		before='c')

def configure(conf):
    conf.env['PYTHON'] = sys.executable
    conf.env['GENPY_EXT'] = ['.genpy']
    return True
