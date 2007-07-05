# encoding: utf-8
#
# WAF build scripts for XMMS2
# Copyright (C) 2006-2007 XMMS2 Team
#

import sys
if sys.version_info < (2,3):
    raise RuntimeError("Python 2.3 or newer is required")

import os
import optparse

# Waf removes the current dir from the python path. We readd it to
# import waftools stuff.
sys.path.insert(0,os.getcwd())

from waftools import gittools

import sets
from Params import g_platform
import Params
import Object
import Utils
import Common

VERSION="0.2 DrJekyll+WIP (git commit: %s)" % gittools.get_info_str()
APPNAME='xmms2'

srcdir='.'
blddir = '_build_'

####
## Initialization
####
def init():
    import gc
    gc.disable()

subdirs = """
          src/lib/xmmstypes
          src/lib/xmmssocket
          src/lib/xmmsipc
          src/lib/xmmsutils
          src/clients/lib/xmmsclient
          src/clients/lib/xmmsclient-glib
          src/include
          src/includepriv
          """.split()

optional_subdirs = ["src/clients/cli",
                    "src/clients/launcher",
                    "src/clients/et",
                    "src/clients/mdns/dns_sd",
                    "src/clients/mdns/avahi",
                    "src/clients/medialib-updater",
                    "src/clients/lib/xmmsclient-ecore",
                    "src/clients/lib/xmmsclient++",
                    "src/clients/lib/xmmsclient++-glib",
                    "src/clients/lib/python",
                    "src/clients/lib/perl",
                    "src/clients/lib/ruby"]

all_optionals = sets.Set([os.path.basename(o) for o in optional_subdirs])
all_plugins = sets.Set([p for p in os.listdir("src/plugins")
                        if os.path.exists(os.path.join("src/plugins",p,"wscript"))])

####
## Build
####
def build(bld):
    if bld.env_of_name("default")["BUILD_XMMS2D"]:
        subdirs.append("src/xmms")

    newest = max([os.stat(os.path.join(sd, "wscript")).st_mtime for sd in subdirs])
    if bld.env_of_name('default')['NEWEST_WSCRIPT_SUBDIR'] and newest > bld.env_of_name('default')['NEWEST_WSCRIPT_SUBDIR']:
        Params.fatal("You need to run waf configure")

    # Process subfolders
    bld.add_subdirs(subdirs)

    # Build configured plugins
    plugins = bld.env_of_name('default')['XMMS_PLUGINS_ENABLED']
    bld.add_subdirs(["src/plugins/%s" % plugin for plugin in plugins])

    # Build the clients
    bld.add_subdirs(bld.env_of_name('default')['XMMS_OPTIONAL_BUILD'])

    # pkg-config
    o = bld.create_obj('pkgc')
    o.version = VERSION
    o.libs = bld.env_of_name('default')['XMMS_PKGCONF_FILES']

    Common.install_files('SHAREDDIR', '', 'mind.in.a.box-lament_snipplet.ogg')


####
## Configuration
####
def _configure_optionals(conf):
    """Process the optional xmms2 subprojects"""
    def _check_exist(optionals, msg):
        unknown_optionals = optionals.difference(all_optionals)
        if unknown_optionals:
            Params.fatal(msg % {'unknown_optionals': ', '.join(unknown_optionals)})
        return optionals

    conf.env['XMMS_OPTIONAL_BUILD'] = []

    if Params.g_options.enable_optionals:
        selected_optionals = _check_exist(sets.Set(Params.g_options.enable_optionals),
                                           "The following optional(s) were requested, "
                                           "but don't exist: %(unknown_optionals)s")
        optionals_must_work = True
    elif Params.g_options.disable_optionals:
        disabled_optionals = _check_exist(sets.Set(Params.g_options.disable_optionals),
                                          "The following optional(s) were disabled, "
                                          "but don't exist: %(unknown_optionals)s")
        selected_optionals = all_optionals.difference(disabled_optionals)
        optionals_must_work = False
    else:
        selected_optionals = all_optionals
        optionals_must_work = False

    failed_optionals = sets.Set()
    succeeded_optionals = sets.Set()

    for o in selected_optionals:
        x = [x for x in optional_subdirs if os.path.basename(x) == o][0]
        if conf.sub_config(x):
            conf.env.append_value('XMMS_OPTIONAL_BUILD', x)
            succeeded_optionals.add(o)
        else:
            failed_optionals.add(o)

    if optionals_must_work and failed_optionals:
        Params.fatal("The following required optional(s) failed to configure: "
                     "%s" % ', '.join(failed_optionals))

    disabled_optionals = sets.Set(all_optionals)
    disabled_optionals.difference_update(succeeded_optionals)

    return succeeded_optionals, disabled_optionals

def _configure_plugins(conf):
    """Process all xmms2d plugins"""
    def _check_exist(plugins, msg):
        unknown_plugins = plugins.difference(all_plugins)
        if unknown_plugins:
            Params.fatal(msg % {'unknown_plugins': ', '.join(unknown_plugins)})
        return plugins

    conf.env['XMMS_PLUGINS_ENABLED'] = []

    # If an explicit list was provided, only try to process that
    if Params.g_options.enable_plugins:
        selected_plugins = _check_exist(sets.Set(Params.g_options.enable_plugins),
                                        "The following plugin(s) were requested, "
                                        "but don't exist: %(unknown_plugins)s")
        disabled_plugins = all_plugins.difference(selected_plugins)
        plugins_must_work = True
    # If a disable list was provided, we try all plugins except for those.
    elif Params.g_options.disable_plugins:
        disabled_plugins = _check_exist(sets.Set(Params.g_options.disable_plugins),
                                        "The following plugins(s) were disabled, "
                                        "but don't exist: %(unknown_plugins)s")
        selected_plugins = all_plugins.difference(disabled_plugins)
        plugins_must_work = False
    # If we don't have the daemon we don't build plugins.
    elif Params.g_options.without_xmms2d:
        disabled_plugins = all_plugins
        selected_plugins = sets.Set()
        plugins_must_work = False
    # Else, we try all plugins.
    else:
        selected_plugins = all_plugins
        disabled_plugins = sets.Set()
        plugins_must_work = False


    for plugin in selected_plugins:
        conf.sub_config("src/plugins/%s" % plugin)
        if (not conf.env["XMMS_PLUGINS_ENABLED"] or
                (len(conf.env["XMMS_PLUGINS_ENABLED"]) > 0
                 and conf.env['XMMS_PLUGINS_ENABLED'][-1] != plugin)):
            disabled_plugins.add(plugin)

    # If something failed and we don't tolerate failure...
    if plugins_must_work:
        broken_plugins = selected_plugins.intersection(disabled_plugins)
        if broken_plugins:
            Params.fatal("The following required plugin(s) failed to configure: "
                                     "%s" % ', '.join(broken_plugins))

    return conf.env['XMMS_PLUGINS_ENABLED'], disabled_plugins

def _output_summary(enabled_plugins, disabled_plugins,
                                        enabled_optionals, disabled_optionals):
    print "\nOptional configuration:\n======================"
    print " Enabled:",
    Params.pprint('BLUE', ', '.join(enabled_optionals))
    print " Disabled:",
    Params.pprint('BLUE', ", ".join(disabled_optionals))
    print "\nPlugins configuration:\n======================"
    print " Enabled:",
    Params.pprint('BLUE', ", ".join(enabled_plugins))
    print " Disabled:",
    Params.pprint('BLUE', ", ".join(disabled_plugins))

def configure(conf):
    conf.env["BUILD_XMMS2D"] = False
    if not Params.g_options.without_xmms2d == True:
        conf.env["BUILD_XMMS2D"] = True
        subdirs.insert(0, "src/xmms")

    if Params.g_options.manualdir:
        conf.env["MANDIR"] = Params.g_options.manualdir
    else:
        conf.env["MANDIR"] = os.path.join(conf.env["PREFIX"], "share", "man")

    if (conf.check_tool('g++')):
        conf.env["HAVE_CXX"] = True
    else:
        conf.env["HAVE_CXX"] = False
    conf.check_tool('misc checks')
    conf.check_tool('gcc')
    conf.check_tool('pkgconfig', tooldir=os.path.abspath('waftools'))
    conf.check_tool('man', tooldir=os.path.abspath('waftools'))

    conf.env["VERSION"] = VERSION
    conf.env["CCFLAGS"] = Utils.to_list(conf.env["CCFLAGS"]) + ['-g', '-O0']
    conf.env["CXXFLAGS"] = Utils.to_list(conf.env["CXXFLAGS"]) + ['-g', '-O0']
    conf.env['XMMS_PKGCONF_FILES'] = []
    conf.env['XMMS_OUTPUT_PLUGINS'] = [(-1, "NONE")]

    conf.env['CCDEFINES'] += ["XMMS_VERSION=\"\\\"%s\\\"\"" % VERSION]
    conf.env['CXXDEFINES'] += ["XMMS_VERSION=\"\\\"%s\\\"\"" % VERSION]

    if Params.g_options.config_prefix:
        conf.env["LIBPATH"] += [os.path.join(Params.g_options.config_prefix, "lib")]
        include = [os.path.join(Params.g_options.config_prefix, "include")]
        conf.env['CPPPATH'] += include

    conf.env["LINKFLAGS_xlibs"] += ['-install_name %s%s%s' % (os.path.join(conf.env["PREFIX"], 'lib', conf.env["shlib_PREFIX"]), '%s', conf.env["shlib_SUFFIX"])]

    # Our static libraries may link to dynamic libraries
    if g_platform != 'win32':
        conf.env["staticlib_CCFLAGS"] += ['-fPIC', '-DPIC']

    # Check for support for the generic platform
    has_platform_support = os.name in ('nt', 'posix')
    conf.check_message("platform code for", os.name, has_platform_support)
    if not has_platform_support:
        Params.fatal("xmms2 only has platform support for Windows "
                     "and POSIX operating systems.")

    conf.check_tool('checks')

    # Check sunOS socket support
    if g_platform == 'sunos5':
        if not conf.check_library2("socket", uselib='socket'):
            Params.fatal("xmms2 requires libsocket on Solaris.")
        conf.env.append_unique('CCFLAGS', '-D_POSIX_PTHREAD_SEMANTICS')
        conf.env.append_unique('CCFLAGS', '-D_REENTRANT')

    # Check win32 (winsock2) socket support
    if g_platform == 'win32':
        if not conf.check_library2("wsock32", uselib='socket'):
            Params.fatal("xmms2 requires wsock32 on windows.")
        conf.env.append_unique('LIB_socket', 'ws2_32')

    # Glib is required by everyone, so check for it here and let them
    # assume its presence.
    conf.check_pkg2('glib-2.0', version='2.6.0', uselib='glib2')

    enabled_plugins, disabled_plugins = _configure_plugins(conf)
    enabled_optionals, disabled_optionals = _configure_optionals(conf)

    newest = max([os.stat(os.path.join(sd, "wscript")).st_mtime for sd in subdirs])
    conf.env['NEWEST_WSCRIPT_SUBDIR'] = newest

    [conf.sub_config(s) for s in subdirs]

    _output_summary(enabled_plugins, disabled_plugins, enabled_optionals, disabled_optionals)


####
## Options
####
def _list_cb(option, opt, value, parser):
    """Callback that lets you specify lists of targets."""
    vals = value.split(',')
    if getattr(parser.values, option.dest):
        vals += getattr(parser.values, option.dest)
    setattr(parser.values, option.dest, vals)

def set_options(opt):
    opt.tool_options('gcc')
    opt.add_option('--with-plugins', action="callback", callback=_list_cb,
                   type="string", dest="enable_plugins")
    opt.add_option('--without-plugins', action="callback", callback=_list_cb,
                   type="string", dest="disable_plugins")
    opt.add_option('--with-optionals', action="callback", callback=_list_cb,
                   type="string", dest="enable_optionals")
    opt.add_option('--without-optionals', action="callback", callback=_list_cb,
                   type="string", dest="disable_optionals")
    opt.add_option('--conf-prefix', type='string', dest='config_prefix')
    opt.add_option('--without-xmms2d', type='int', dest='without_xmms2d')
    opt.add_option('--with-mandir', type='string', dest='manualdir')

    for o in optional_subdirs + subdirs:
        opt.sub_options(o)

def shutdown():
    if Params.g_commands['install'] and os.geteuid() == 0:
        ldconfig = '/sbin/ldconfig'
        if os.path.isfile(ldconfig):
            try: os.popen(ldconfig)
            except: pass
