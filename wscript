# encoding: utf-8
#
# WAF build scripts for XMMS2
# Copyright (C) 2006-2008 XMMS2 Team
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
import Params
import Object
import Utils
import Common

BASEVERSION="0.4 DrKosmos+WIP"
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
                    "src/clients/lib/xmmsclient-cf",
                    "src/clients/lib/python",
                    "src/clients/lib/perl",
                    "src/clients/lib/ruby",
                    "pixmaps"]

all_optionals = sets.Set([os.path.basename(o) for o in optional_subdirs])
all_plugins = sets.Set([p for p in os.listdir("src/plugins")
                        if os.path.exists(os.path.join("src/plugins",p,"wscript"))])

####
## Build
####
def build(bld):
    env = bld.env()

    if env["BUILD_XMMS2D"]:
        subdirs.append("src/xmms")

    newest = max([os.stat(os.path.join(sd, "wscript")).st_mtime for sd in subdirs])
    if env['NEWEST_WSCRIPT_SUBDIR'] and newest > env['NEWEST_WSCRIPT_SUBDIR']:
        Params.fatal("You need to run waf configure")

    # Process subfolders
    bld.add_subdirs(subdirs)

    # Build configured plugins
    plugins = env['XMMS_PLUGINS_ENABLED']
    bld.add_subdirs(["src/plugins/%s" % plugin for plugin in plugins])

    # Build the clients
    bld.add_subdirs(env['XMMS_OPTIONAL_BUILD'])

    # pkg-config
    o = bld.create_obj('pkgc')
    o.libs = env['XMMS_PKGCONF_FILES']

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
    if os.environ.has_key('PKG_CONFIG_PREFIX'):
        prefix = os.environ['PKG_CONFIG_PREFIX']
        if not os.path.isabs(prefix):
            prefix = os.path.abspath(prefix)
        conf.env['PKG_CONFIG_DEFINES'] = {'prefix':prefix}

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

    if conf.check_tool('winres'):
        conf.env['WINRCFLAGS'] = '-I' + os.path.abspath('pixmaps')
        conf.env['xmms_icon'] = True
    else:
        conf.env['xmms_icon'] = False

    if Params.g_options.target_platform:
        Params.g_platform = Params.g_options.target_platform

    nam,changed = gittools.get_info()
    conf.check_message("git commit id", "", True, nam)
    if Params.g_options.customversion:
        conf.env["VERSION"] = BASEVERSION + " (%s + %s)" % (nam, Params.g_options.customversion)
    else:
        dirty=""
        if changed:
            dirty="-dirty"
        conf.check_message("uncommitted changes", "", bool(changed))
        conf.env["VERSION"] = BASEVERSION + " (git commit: %s%s)" % (nam, dirty)

    conf.env["CCFLAGS"] = Utils.to_list(conf.env["CCFLAGS"]) + ['-g', '-O0']
    conf.env["CXXFLAGS"] = Utils.to_list(conf.env["CXXFLAGS"]) + ['-g', '-O0']
    conf.env['XMMS_PKGCONF_FILES'] = []
    conf.env['XMMS_OUTPUT_PLUGINS'] = [(-1, "NONE")]

    if Params.g_options.bindir:
        conf.env["BINDIR"] = Params.g_options.bindir
        conf.env["program_INST_VAR"] = "BINDIR"
        conf.env["program_INST_DIR"] = ""
    else:
        conf.env["BINDIR"] = os.path.join(conf.env["PREFIX"], "bin")

    if Params.g_options.libdir:
        conf.env["LIBDIR"] = Params.g_options.libdir
        conf.env["shlib_INST_VAR"]     = "LIBDIR"
        conf.env["shlib_INST_DIR"]     = ""
        conf.env["staticlib_INST_VAR"] = "LIBDIR"
        conf.env["staticlib_INST_DIR"] = ""

    if Params.g_options.pkgconfigdir:
        conf.env['PKGCONFIGDIR'] = Params.g_options.pkgconfigdir
        print conf.env['PKGCONFIGDIR']
    else:
        conf.env['PKGCONFIGDIR'] = os.path.join(conf.env["PREFIX"], "lib", "pkgconfig")

    if Params.g_options.config_prefix:
        for dir in Params.g_options.config_prefix:
            if not os.path.isabs(dir):
                dir = os.path.abspath(dir)
            conf.env.prepend_value("LIBPATH", os.path.join(dir, "lib"))
            conf.env.prepend_value("CPPPATH", os.path.join(dir, "include"))

    # Our static libraries may link to dynamic libraries
    if Params.g_platform != 'win32':
        conf.env["staticlib_CCFLAGS"] += ['-fPIC', '-DPIC']
    else:
        # As we have to change target platform after the tools
        # have been loaded there are a few variables that needs
        # to be initiated if building for win32.

        # Make sure we don't have -fPIC and/or -DPIC in our CCFLAGS
        conf.env["shlib_CCFLAGS"] = []
        conf.env['plugin_CCFLAGS'] = []

        # Setup various prefixes
        conf.env["shlib_SUFFIX"] = '.dll'
        conf.env['plugin_SUFFIX'] = '.dll'
        conf.env['program_SUFFIX'] = '.exe'

    # Add some specific OSX things
    if Params.g_platform == 'darwin':
        conf.env["LINKFLAGS"] += ['-multiply_defined suppress']
        conf.env["explicit_install_name"] = True
    else:
        conf.env["explicit_install_name"] = False

    # Check for support for the generic platform
    has_platform_support = os.name in ('nt', 'posix')
    conf.check_message("platform code for", os.name, has_platform_support)
    if not has_platform_support:
        Params.fatal("xmms2 only has platform support for Windows "
                     "and POSIX operating systems.")

    conf.check_tool('checks')

    # Check sunOS socket support
    if Params.g_platform == 'sunos5':
        if not conf.check_library2("socket", uselib='socket'):
            Params.fatal("xmms2 requires libsocket on Solaris.")
        conf.env.append_unique('CCFLAGS', '-D_POSIX_PTHREAD_SEMANTICS')
        conf.env.append_unique('CCFLAGS', '-D_REENTRANT')
        conf.env['socket_impl'] = 'socket'
    # Check win32 (winsock2) socket support
    elif Params.g_platform == 'win32':
        if Params.g_options.winver:
            major, minor = [int(x) for x in Params.g_options.winver.split('.')]
        else:
            try:
                major, minor = sys.getwindowsversion()[:2]
            except AttributeError, e:
                Params.warning('No Windows version found and no version set. ' +
                               'Defaulting to 5.1 (XP). You will not be able ' +                               'to use this build of XMMS2 on older Windows ' +
                               'versions.')
                major, minor = (5, 1)
        need_wspiapi = True
        if (major >= 5 and minor >= 1):
            need_wspiapi = False
        conf.check_header2('windows.h')
        conf.check_header2('ws2tcpip.h')
        conf.check_header2('winsock2.h')
        conf.env['CCDEFINES'] += ['HAVE_WINSOCK2']
        conf.env['CXXDEFINES'] += ['HAVE_WINSOCK2']
        h = conf.create_header_configurator()
        h.name = 'wspiapi.h'
        h.header_code = "#include <windows.h>\n#include <ws2tcpip.h>"
        if h.run():
            conf.env['CCDEFINES'] += ['HAVE_WSPIAPI']
        elif need_wspiapi:
            Params.fatal('XMMS2 requires WSPiApi.h on Windows versions prior ' +
                         'to XP. WSPiApi.h is provided by the Platform SDK.')
        else:
            print('This XMMS2 will only run on Windows XP and newer ' +
                  'machines. If you are planning to use XMMS2 on older ' +
                  'versions of Windows or are packaging a binary, please ' +
                  'consider installing the WSPiApi.h header for ' +
                  'compatibility. It is provided by the Platform SDK.')
        conf.env['CCDEFINES'] += ['_WIN32_WINNT=0x%02x%02x' % (major, minor)]
        if not conf.check_library2("wsock32", uselib='socket'):
            Params.fatal("xmms2 requires wsock32 on windows.")
        conf.env.append_unique('LIB_socket', 'ws2_32')
        conf.env['socket_impl'] = 'wsock32'
    # Default POSIX sockets
    else:
        conf.env['socket_impl'] = 'posix'

    # Glib is required by everyone, so check for it here and let them
    # assume its presence.
    conf.check_pkg2('glib-2.0', version='2.8.0', uselib='glib2')

    enabled_plugins, disabled_plugins = _configure_plugins(conf)
    enabled_optionals, disabled_optionals = _configure_optionals(conf)

    newest = max([os.stat(os.path.join(sd, "wscript")).st_mtime for sd in subdirs])
    conf.env['NEWEST_WSCRIPT_SUBDIR'] = newest

    [conf.sub_config(s) for s in subdirs]

    _output_summary(enabled_plugins, disabled_plugins, enabled_optionals, disabled_optionals)

    return True


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

    opt.add_option('--with-custom-version', type='string',
                   dest='customversion')
    opt.add_option('--with-plugins', action="callback", callback=_list_cb,
                   type="string", dest="enable_plugins")
    opt.add_option('--without-plugins', action="callback", callback=_list_cb,
                   type="string", dest="disable_plugins")
    opt.add_option('--with-optionals', action="callback", callback=_list_cb,
                   type="string", dest="enable_optionals")
    opt.add_option('--without-optionals', action="callback", callback=_list_cb,
                   type="string", dest="disable_optionals")
    opt.add_option('--conf-prefix', action="callback", callback=_list_cb,
                   type='string', dest='config_prefix')
    opt.add_option('--without-xmms2d', type='int', dest='without_xmms2d')
    opt.add_option('--with-mandir', type='string', dest='manualdir')
    opt.add_option('--with-bindir', type='string', dest='bindir')
    opt.add_option('--with-libdir', type='string', dest='libdir')
    opt.add_option('--with-pkgconfigdir', type='string', dest='pkgconfigdir')
    opt.add_option('--with-target-platform', type='string',
                   dest='target_platform')
    opt.add_option('--with-windows-version', type='string', dest='winver')

    for o in optional_subdirs + subdirs:
        opt.sub_options(o)

def shutdown():
    if Params.g_commands['install'] and os.geteuid() == 0:
        ldconfig = '/sbin/ldconfig'
        if os.path.isfile(ldconfig):
            try: os.popen(ldconfig)
            except: pass
