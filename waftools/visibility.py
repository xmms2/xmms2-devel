# encoding: utf-8

from waflib.TaskGen import after_method,feature

UNKNOWN_OPTIONS_AS_ERRORS = '-Werror=unknown-warning-option'
VISIBILITY_FEATURE = '-fvisibility=hidden'

@feature('visibilityhidden')
@after_method('propagate_uselib_vars')
def add_visibility_hidden_flag(task):
    if task.env.have_visibility_cflag:
        task.env.append_unique('CFLAGS', VISIBILITY_FEATURE)
    if task.env.have_visibility_cxxflag:
        task.env.append_unique('CXXFLAGS', VISIBILITY_FEATURE)

def configure(ctx):
    have_cflag = False
    have_cxxflag = False

    ctx.env.stash()

    if ctx.check_cc(cflags=UNKNOWN_OPTIONS_AS_ERRORS, mandatory=False):
        ctx.env.append_unique('CFLAGS', UNKNOWN_OPTIONS_AS_ERRORS)

    if ctx.check_cxx(cxxflags=UNKNOWN_OPTIONS_AS_ERRORS, mandatory=False):
        ctx.env.append_unique('CXXFLAGS', UNKNOWN_OPTIONS_AS_ERRORS)

    if ctx.check_cc(cflags=VISIBILITY_FEATURE, mandatory=False):
        have_cflag = True
    if ctx.check_cxx(cflags=VISIBILITY_FEATURE, mandatory=False):
        have_cxxflag = True

    ctx.env.revert()

    ctx.env.have_visibility_cflag = have_cflag
    ctx.env.have_visibility_cxxflag = have_cxxflag
