from waflib import TaskGen
import sys

TaskGen.declare_chain(
		name='genpy',
		rule='${PYTHON} ${SRC} ${GENPY_ARGS} > ${TGT}',
		reentrant=True,
		color='BLUE',
		ext_in='.genpy',
		ext_out='.c',
		before='c')

def configure(conf):
    conf.env['PYTHON'] = sys.executable
    conf.env['GENPY_EXT'] = ['.genpy']
    conf.env['GENPY_ARGS'] = ''
    return True
