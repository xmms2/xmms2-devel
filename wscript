# encoding: utf-8
#
# WAF build scripts for XMMS2
# Copyright (C) 2006-2008 XMMS2 Team
#

import sys
if sys.version_info < (2,4):
    raise RuntimeError("Python 2.4 or newer is required")

import os
import optparse

# Waf removes the current dir from the python path. We readd it to
# import waftools stuff.
sys.path.insert(0,os.getcwd())

from waftools import gittools

import sets
import Options
import Utils
import Build
import Configure
from logging import fatal, warning

BASEVERSION="0.5 DrLecter"
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
          src/lib/xmmsvisualization
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
                    "src/clients/vistest",
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
    if bld.env["BUILD_XMMS2D"]:
        subdirs.append("src/xmms")

    newest = max([os.stat(os.path.join(sd, "wscript")).st_mtime for sd in subdirs])
    if bld.env['NEWEST_WSCRIPT_SUBDIR'] and newest > bld.env['NEWEST_WSCRIPT_SUBDIR']:
        fatal("You need to run waf configure")
        raise SystemExit

    # Process subfolders
    bld.add_subdirs(subdirs)

    # Build configured plugins
    plugins = bld.env['XMMS_PLUGINS_ENABLED']
    bld.add_subdirs(["src/plugins/%s" % plugin for plugin in plugins])

    # Build the clients
    bld.add_subdirs(bld.env['XMMS_OPTIONAL_BUILD'])

    for name, lib in bld.env['XMMS_PKGCONF_FILES']:
        obj = bld.new_task_gen('subst')
        obj.source = 'xmms2.pc.in'
        obj.target = name + '.pc'
        obj.dict = {
               'NAME': name,
                'LIB': lib,
             'PREFIX': bld.env['PREFIX'],
             'BINDIR': os.path.join("${prefix}", "bin"),
             'LIBDIR': os.path.join("${prefix}", "lib"),
         'INCLUDEDIR': os.path.join("${prefix}", "include", "xmms2"),
            'VERSION': bld.env["VERSION"],
        }
        obj.install_path = '${PKGCONFIGDIR}'

    bld.install_files('${SHAREDDIR}', 'mind.in.a.box-lament_snipplet.ogg')


####
## Configuration
####
def _configure_optionals(conf):
    """Process the optional xmms2 subprojects"""
    def _check_exist(optionals, msg):
        unknown_optionals = optionals.difference(all_optionals)
        if unknown_optionals:
            fatal(msg % {'unknown_optionals': ', '.join(unknown_optionals)})
            raise SystemExit
        return optionals

    conf.env['XMMS_OPTIONAL_BUILD'] = []

    if Options.options.enable_optionals:
        selected_optionals = _check_exist(sets.Set(Options.options.enable_optionals),
                                           "The following optional(s) were requested, "
                                           "but don't exist: %(unknown_optionals)s")
        optionals_must_work = True
    elif Options.options.disable_optionals:
        disabled_optionals = _check_exist(sets.Set(Options.options.disable_optionals),
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
        fatal("The following required optional(s) failed to configure: "
                     "%s" % ', '.join(failed_optionals))
        raise SystemExit

    disabled_optionals = sets.Set(all_optionals)
    disabled_optionals.difference_update(succeeded_optionals)

    return succeeded_optionals, disabled_optionals

def _configure_plugins(conf):
    """Process all xmms2d plugins"""
    def _check_exist(plugins, msg):
        unknown_plugins = plugins.difference(all_plugins)
        if unknown_plugins:
            fatal(msg % {'unknown_plugins': ', '.join(unknown_plugins)})
            raise SystemExit
        return plugins

    conf.env['XMMS_PLUGINS_ENABLED'] = []

    # If an explicit list was provided, only try to process that
    if Options.options.enable_plugins:
        selected_plugins = _check_exist(sets.Set(Options.options.enable_plugins),
                                        "The following plugin(s) were requested, "
                                        "but don't exist: %(unknown_plugins)s")
        disabled_plugins = all_plugins.difference(selected_plugins)
        plugins_must_work = True
    # If a disable list was provided, we try all plugins except for those.
    elif Options.options.disable_plugins:
        disabled_plugins = _check_exist(sets.Set(Options.options.disable_plugins),
                                        "The following plugins(s) were disabled, "
                                        "but don't exist: %(unknown_plugins)s")
        selected_plugins = all_plugins.difference(disabled_plugins)
        plugins_must_work = False
    # If we don't have the daemon we don't build plugins.
    elif Options.options.without_xmms2d:
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
            fatal("The following required plugin(s) failed to configure: "
                                     "%s" % ', '.join(broken_plugins))
            raise SystemExit

    return conf.env['XMMS_PLUGINS_ENABLED'], disabled_plugins

def _output_summary(enabled_plugins, disabled_plugins,
                                        enabled_optionals, disabled_optionals):
    print "\nOptional configuration:\n======================"
    print " Enabled: ",
    Utils.pprint('BLUE', ', '.join(sorted(enabled_optionals)))
    print " Disabled: ",
    Utils.pprint('BLUE', ", ".join(sorted(disabled_optionals)))
    print "\nPlugins configuration:\n======================"
    print " Enabled: ",
    Utils.pprint('BLUE', ", ".join(sorted(enabled_plugins)))
    print " Disabled: ",
    Utils.pprint('BLUE', ", ".join(sorted(disabled_plugins)))

def configure(conf):
    if os.environ.has_key('PKG_CONFIG_PREFIX'):
        prefix = os.environ['PKG_CONFIG_PREFIX']
        if not os.path.isabs(prefix):
            prefix = os.path.abspath(prefix)
        conf.env['PKG_CONFIG_DEFINES'] = {'prefix':prefix}

    conf.env["BUILD_XMMS2D"] = False
    if not Options.options.without_xmms2d == True:
        conf.env["BUILD_XMMS2D"] = True
        subdirs.insert(0, "src/xmms")

    if Options.options.manualdir:
        conf.env["MANDIR"] = Options.options.manualdir
    else:
        conf.env["MANDIR"] = os.path.join(conf.env["PREFIX"], "share", "man")

    conf.check_tool('misc')
    conf.check_tool('gcc')
    conf.check_tool('g++')

    try:
        conf.check_tool('winres')
        conf.env['WINRCFLAGS'] = '-I' + os.path.abspath('pixmaps')
        conf.env['xmms_icon'] = True
    except Configure.ConfigurationError:
        conf.env['xmms_icon'] = False

    if Options.options.target_platform:
        Options.platform = Options.options.target_platform

    nam,changed = gittools.get_info()
    conf.check_message("git commit id", "", True, nam)
    if Options.options.customversion:
        conf.env["VERSION"] = BASEVERSION + " (%s + %s)" % (nam, Options.options.customversion)
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

    if Options.options.bindir:
        conf.env["BINDIR"] = Options.options.bindir
    else:
        conf.env["BINDIR"] = os.path.join(conf.env["PREFIX"], "bin")

    if Options.options.libdir:
        conf.env["LIBDIR"] = Options.options.libdir
    else:
        conf.env["LIBDIR"] = os.path.join(conf.env["PREFIX"], "lib")

    if Options.options.pkgconfigdir:
        conf.env['PKGCONFIGDIR'] = Options.options.pkgconfigdir
        print conf.env['PKGCONFIGDIR']
    else:
        conf.env['PKGCONFIGDIR'] = os.path.join(conf.env["PREFIX"], "lib", "pkgconfig")

    if Options.options.config_prefix:
        for dir in Options.options.config_prefix:
            if not os.path.isabs(dir):
                dir = os.path.abspath(dir)
            conf.env.prepend_value("LIBPATH", os.path.join(dir, "lib"))
            conf.env.prepend_value("CPPPATH", os.path.join(dir, "include"))

    # Our static libraries may link to dynamic libraries
    if Options.platform != 'win32':
        conf.env["staticlib_CCFLAGS"] += ['-fPIC', '-DPIC']
    else:
        # As we have to change target platform after the tools
        # have been loaded there are a few variables that needs
        # to be initiated if building for win32.

        # Make sure we don't have -fPIC and/or -DPIC in our CCFLAGS
        conf.env["shlib_CCFLAGS"] = []

        # Setup various prefixes
        conf.env["shlib_PATTERN"] = 'lib%s.dll'
        conf.env['program_PATTERN'] = '%s.exe'

    # Add some specific OSX things
    if Options.platform == 'darwin':
        conf.env["LINKFLAGS"] += ['-multiply_defined suppress']
        conf.env["explicit_install_name"] = True
    else:
        conf.env["explicit_install_name"] = False

    # Check for support for the generic platform
    has_platform_support = os.name in ('nt', 'posix')
    conf.check_message("platform code for", os.name, has_platform_support)
    if not has_platform_support:
        fatal("xmms2 only has platform support for Windows "
                     "and POSIX operating systems.")
        raise SystemExit

    # Check sunOS socket support
    if Options.platform == 'sunos':
        conf.check_cc(function_name='socket', lib='socket', header_name='sys/socket.h', uselib_store='socket')
        if not conf.env["HAVE_SOCKET"]:
            fatal("xmms2 requires libsocket on Solaris.")
            raise SystemExit
        conf.env.append_unique('CCFLAGS', '-D_POSIX_PTHREAD_SEMANTICS')
        conf.env.append_unique('CCFLAGS', '-D_REENTRANT')
        conf.env.append_unique('CCFLAGS', '-std=gnu99')
        conf.env['socket_impl'] = 'socket'
    # Check win32 (winsock2) socket support
    elif Options.platform == 'win32':
        if Options.options.winver:
            major, minor = [int(x) for x in Options.options.winver.split('.')]
        else:
            try:
                major, minor = sys.getwindowsversion()[:2]
            except AttributeError, e:
                warning('No Windows version found and no version set. ' +
                        'Defaulting to 5.1 (XP). You will not be able ' +
                        'to use this build of XMMS2 on older Windows ' +
                        'versions.')
                major, minor = (5, 1)
        need_wspiapi = True
        if (major >= 5 and minor >= 1):
            need_wspiapi = False

        conf.check_cc(lib="wsock32", uselib_store="socket", mandatory=1)
        conf.check_cc(lib="ws2_32", uselib_store="socket", mandatory=1)
        conf.check_cc(header_name="windows.h", uselib="socket", mandatory=1)
        conf.check_cc(header_name="ws2tcpip.h", uselib="socket", mandatory=1)
        conf.check_cc(header_name="winsock2.h", uselib="socket", mandatory=1)
        if not conf.check_cc(header_name="wspiapi.h", fragment="#include <ws2tcpip.h>\n#include <wspiapi.h>\nint main() {return 0;}", uselib="socket", mandatory=need_wspiapi):
            warning('This XMMS2 will only run on Windows XP and newer ' +
                    'machines. If you are planning to use XMMS2 on older ' +
                    'versions of Windows or are packaging a binary, please ' +
                    'consider installing the WSPiApi.h header for ' +
                    'compatibility. It is provided by the Platform SDK.')

        conf.env['CCDEFINES_socket'] += [
            '_WIN32_WINNT=0x%02x%02x' % (major, minor),
            'HAVE_WINSOCK2', 1
        ]

        conf.env['socket_impl'] = 'wsock32'
    # Default POSIX sockets
    else:
        conf.env['socket_impl'] = 'posix'

    # Glib is required by everyone, so check for it here and let them
    # assume its presence.
    conf.check_cfg(package='glib-2.0', atleast_version='2.8.0', uselib_store='glib2', args='--cflags --libs', mandatory=1)

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

    opt.sub_options("src/xmms")
    for o in optional_subdirs + subdirs:
        opt.sub_options(o)

def shutdown():
    if Options.commands['install'] and os.geteuid() == 0:
        ldconfig = '/sbin/ldconfig'
        if os.path.isfile(ldconfig):
            libprefix = Utils.subst_vars('${PREFIX}/lib', Build.bld.env)
            try: Utils.cmd_output(ldconfig + ' ' + libprefix)
            except: pass
