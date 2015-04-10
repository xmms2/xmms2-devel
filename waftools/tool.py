import os

def add_install_flag(bld, obj):
    env = bld.env

    if env['explicit_install_name']:
        libname = obj.env["cshlib_PATTERN"] % obj.target
        insname = os.path.join(obj.env["LIBDIR"], libname)
        obj.env.append_unique("LINKFLAGS", ["-install_name", insname])
