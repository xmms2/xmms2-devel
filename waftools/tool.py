import os
from Params import g_platform

def add_install_flag(bld, obj):
    env = bld.env_of_name("default")

    if env['explicit_install_name']:
        libname = obj.env["shlib_PREFIX"]+obj.target+obj.env["shlib_SUFFIX"]
        insname = os.path.join(obj.env["PREFIX"], 'lib', libname)
        obj.env.append_unique("LINKFLAGS", '-install_name ' + insname)
