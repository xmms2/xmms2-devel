# encoding: utf-8
#
# WAF build scripts for XMMS2
# Copyright (C) 2006-2001 XMMS2 Team
#

import sys
if sys.version_info < (2,4):
    raise RuntimeError("Python 2.4 or newer is required")

import os

# Waf removes the current dir from the python path. We readd it to import waf
# tools stuff.
sys.path.insert(0, os.getcwd())

from waflib import Options, Utils, Errors, Logs

from waftools.compiler_flags import compiler_flags
from waftools import gittools

BASEVERSION="0.7 DrNo"
APPNAME='xmms2'

top = '.'
out = '_build_'

_waf_hexversion = 0x01060700
_waf_mismatch_msg = """
You are building xmms2 with a waf version that is different from the one
distributed with xmms2. This is not supported by the XMMS2 Team. Before
reporting any errors with this setup, please rebuild XMMS2 using './waf
configure build install'
"""

####
## Initialization
####
def init(ctx):
    import gc
    gc.disable()

    from waflib import Context
    if _waf_hexversion != Context.HEXVERSION:
        Logs.warn(_waf_mismatch_msg)

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

optional_subdirs = """
src/clients/nycli
src/clients/launcher
src/clients/et
src/clients/mdns/dns_sd
src/clients/mdns/avahi
src/clients/medialib-updater
src/clients/vistest
src/clients/lib/xmmsclient-ecore
src/clients/lib/xmmsclient++
src/clients/lib/xmmsclient++-glib
src/clients/lib/xmmsclient-cf
src/clients/lib/python
src/clients/lib/perl
src/clients/lib/ruby
tests
pixmaps
""".split()

def is_plugin(x):
    return os.path.exists(os.path.join("src/plugins", x, "wscript"))

all_optionals = set(os.path.basename(o) for o in optional_subdirs)
all_plugins = set(p for p in os.listdir("src/plugins") if is_plugin(p))

def get_newest(*dirs):
    m = 0
    for d in dirs:
        m = max([m]+[os.stat(os.path.join(sd, "wscript")).st_mtime for sd in d])
    return m

####
## Build
####
def build(bld):
    if bld.env.BUILD_XMMS2D:
        subdirs.append("src/xmms")

    plugins = bld.env.XMMS_PLUGINS_ENABLED
    plugindirs = ["src/plugins/%s" % plugin for plugin in plugins]
    optionaldirs = bld.env.XMMS_OPTIONAL_BUILD

    newest = get_newest(subdirs, plugindirs, optionaldirs)
    if bld.env.NEWEST_WSCRIPT_SUBDIR and newest > bld.env.NEWEST_WSCRIPT_SUBDIR:
        bld.fatal("You need to run waf configure")
        raise SystemExit()

    bld.add_subdirs(subdirs)
    bld.add_subdirs(plugindirs)
    bld.add_subdirs(optionaldirs)

    for name, lib in bld.env.XMMS_PKGCONF_FILES:
        bld(features = 'subst',
            source = 'xmms2.pc.in',
            target = name + '.pc',
            install_path = '${PKGCONFIGDIR}',
            NAME = name,
            LIB = lib+' ', # Avoid error in waf 1.6 if lib == ''
            PREFIX = bld.env.PREFIX,
            BINDIR = bld.env.BINDIR,
            LIBDIR = bld.env.LIBDIR,
            INCLUDEDIR = os.path.join(bld.env.INCLUDEDIR, "xmms2"),
            VERSION = bld.env.VERSION
            )

    bld.install_files('${SHAREDDIR}', "mind.in.a.box-lament_snipplet.ogg")


####
## Configuration
####
def _configure_optionals(conf):
    """Process the optional xmms2 subprojects"""
    def _check_exist(optionals, msg):
        unknown_optionals = optionals.difference(all_optionals)
        if unknown_optionals:
            conf.fatal(msg%dict(unknown_optionals=', '.join(unknown_optionals)))
            raise SystemExit()
        return optionals

    conf.env.XMMS_OPTIONAL_BUILD = []

    if conf.options.enable_optionals is not None:
        selected_optionals = _check_exist(set(conf.options.enable_optionals),
                "The following optional(s) were requested, "
                "but don't exist: %(unknown_optionals)s")
        optionals_must_work = True
    elif conf.options.disable_optionals is not None:
        disabled_optionals = _check_exist(set(conf.options.disable_optionals),
                "The following optional(s) were disabled, "
                "but don't exist: %(unknown_optionals)s")
        selected_optionals = all_optionals.difference(disabled_optionals)
        optionals_must_work = False
    else:
        selected_optionals = all_optionals
        optionals_must_work = False

    failed_optionals = set()
    succeeded_optionals = set()

    for o in selected_optionals:
        x = [x for x in optional_subdirs if os.path.basename(x) == o][0]
        try:
            conf.sub_config(x)
            conf.env.append_value('XMMS_OPTIONAL_BUILD', x)
            succeeded_optionals.add(o)
        except Errors.ConfigurationError:
            failed_optionals.add(o)

    if optionals_must_work and failed_optionals:
        conf.fatal("The following required optional(s) failed to configure: %s"
                % ', '.join(failed_optionals))
        raise SystemExit(1)

    disabled_optionals = set(all_optionals)
    disabled_optionals.difference_update(succeeded_optionals)

    return succeeded_optionals, disabled_optionals

def _configure_plugins(conf):
    """Process ll xmms2d plugins"""
    def _check_exist(plugins, msg):
        unknown_plugins = plugins.difference(all_plugins)
        if unknown_plugins:
            conf.fatal(msg%dict(unknown_plugins=', '.join(unknown_plugins)))
            raise SystemExit(1)
        return plugins

    conf.env.XMMS_PLUGINS_ENABLED = []

    if conf.options.enable_plugins is not None:
        selected_plugins = _check_exist(set(conf.options.enable_plugins),
                "The following plugin(s) were requested, "
                "but don't exist: %(unknown_plugins)s")
        disabled_plugins = all_plugins.difference(selected_plugins)
        plugins_must_work = True
    elif conf.options.disable_plugins is not None:
        disabled_plugins = _check_exist(set(conf.options.disable_plugins),
                "The following plugin(s) were disabled, "
                "but don't exist: %(unknown_plugins)s")
        selected_plugins = all_plugins.difference(disabled_plugins)
        plugins_must_work = False
    elif conf.options.without_xmms2d:
        disabled_plugins = all_plugins
        selected_plugins = set()
        plugins_must_work = False
    else:
        selected_plugins = all_plugins
        disabled_plugins = set()
        plugins_must_work = False

    for plugin in selected_plugins:
        try:
            conf.sub_config("src/plugins/%s" % plugin)
            if (not conf.env.XMMS_PLUGINS_ENABLED or
                    (len(conf.env.XMMS_PLUGINS_ENABLED) > 0 and
                        conf.env.XMMS_PLUGINS_ENABLED[-1] != plugin)):
                disabled_plugins.add(plugin)
        except Errors.ConfigurationError:
            disabled_plugins.add(plugin)

    if plugins_must_work:
        broken_plugins = selected_plugins.intersection(disabled_plugins)
        if broken_plugins:
            conf.fatal("The following required plugin(s) failed to configure: "
                    "%s" % ', '.join(broken_plugins))
            raise SystemExit(1)

    return conf.env.XMMS_PLUGINS_ENABLED, disabled_plugins

def _output_summary(enabled_plugins, disabled_plugins,
                    enabled_optionals, disabled_optionals,
                    output_plugins):
    enabled_plugins = [x for x in enabled_plugins if x not in output_plugins]
    Logs.pprint('Normal', "\n")

    Logs.pprint('Normal', "Optional configuration:\n"
                          "=======================")
    Logs.pprint('Normal', "Enabled: ", sep='')
    Logs.pprint('BLUE', ', '.join(sorted(enabled_optionals)))
    Logs.pprint('Normal', "Disabled: ", sep='')
    Logs.pprint('BLUE', ', '.join(sorted(disabled_optionals)))

    Logs.pprint('Normal', "Plugins configuration:\n"
                          "======================")
    Logs.pprint('Normal', 'Output: ', sep='')
    Logs.pprint('BLUE', ', '.join(sorted(output_plugins)))
    Logs.pprint('Normal', 'XForm/Other: ', sep='')
    Logs.pprint('BLUE', ', '.join(sorted(enabled_plugins)))
    Logs.pprint('Normal', 'Disabled: ', sep='')
    Logs.pprint('BLUE', ', '.join(sorted(disabled_plugins)))

def configure(conf):
    has_platform_support = os.name in ('nt', 'posix')
    conf.msg('Platform code for %s' % os.name, has_platform_support)
    if not has_platform_support:
        conf.fatal('xmms2 only has platform support for Windows and POSIX operating systems.')
        raise SystemExit(1)

    if 'PKG_CONFIG_PREFIX' in os.environ:
        prefix = os.environ['PKG_CONFIG_PREFIX']
        if not os.path.isabs(prefix):
            pretix = os.path.abspath(prefix)
        conf.env.PKG_CONFIG_DEFINES = dict(prefix=prefix)

    conf.env.BUILD_XMMS2D = False
    if not conf.options.without_xmms2d:
        conf.env.BUILD_XMMS2D = True
        subdirs.insert(0, "src/xmms")

    conf.check_tool('gnu_dirs')
    conf.check_tool('man', tooldir=os.path.abspath('waftools'))
    conf.check_tool('misc')
    conf.check_tool('gcc')
    conf.check_tool('g++')

    if conf.options.target_platform:
        Options.platform = conf.options.target_platform

    nam,changed = gittools.get_info()
    conf.msg("git commit id", nam)
    if conf.options.customversion:
        conf.env.VERSION = "%s (%s + %s)" % (BASEVERSION, nam, conf.options.customversion)
    else:
        dirty = changed and "-dirty" or ""
        conf.msg("uncommited changed", changed and "yes" or "no")
        conf.env.VERSION = "%s (git commit: %s%s)" % (BASEVERSION, nam, dirty)

    conf.env.append_unique('CFLAGS', ['-g', '-O0'])
    conf.env.append_unique('CXXFLAGS', ['-g', '-O0'])

    if conf.options.with_profiling:
        conf.env.append_unique('CFLAGS', ['--coverage'])
        conf.env.append_unique('LINKFLAGS', ['--coverage'])

    flags = compiler_flags(conf)

    flags.enable_c_warning('all')
    flags.enable_c_warning('empty-body')
    flags.enable_c_warning('format=2')
    flags.enable_c_warning('format-nonliteral')
    flags.enable_c_warning('format-security')
    flags.enable_c_warning('ignored-qualifiers')
    flags.enable_c_warning('missing-prototypes')
    flags.enable_c_warning('strict-prototypes')
    flags.enable_c_warning('type-limits')
    flags.enable_c_warning('write-strings')

    flags.disable_c_warning('format-extra-args')
    flags.disable_c_warning('format-zero-length')

    flags.enable_feature('diagnostics-show-option')

    flags.enable_c_error('implicit-function-declaration')

    conf.env.XMMS_PKGCONF_FILES = []
    conf.env.XMMS_OUTPUT_PLUGINS = [(-1, "NONE")]

    if conf.options.pkgconfigdir:
        conf.env.PKGCONFIGDIR = conf.options.pkgconfigdir
        Logs.pprint('Normal', conf.env.PKGCONFIGDIR) #XXX What is it ?
    else:
        conf.env.PKGCONFIGDIR = os.path.join(conf.env.LIBDIR, 'pkgconfig')

    if conf.options.config_prefix:
        for d in conf.options.config_prefix:
            if not os.path.isabs(d):
                d = os.path.abspath(d)
            conf.env.prepend_value('LIBPATH', os.path.join(d, 'lib'))
            conf.env.prepend_value('CPPPATH', os.path.join(d, 'include'))

    if Options.platform != 'win32':
        conf.env.append_unique('CFLAGS_cstlib', ['-fPIC', '-DPIC'])
        conf.env.append_unique('CPPFLAGS_cxxshlib', ['-fPIC', '-DPIC'])
    else:
        # As we have to change target platform after the tools
        # have been loaded there are a few variables that needs
        # to be initiated if building for win32.

        # Make sure we don't have -fPIC and/or -DPIC in our CFLAGS
        conf.env.CFLAGS_cshlib = []
        conf.env.CXXFLAGS_cxxshlib = []

        # Setup various prefixes
        conf.env.cshlib_PATTERN = 'lib%s.dll'
        conf.env.cprogram_PATTERN = '%s.exe'

    if Options.platform == 'darwin':
        conf.env.append_value('LINKFLAGS', '-multiply_defined_suppress')
        conf.env.explicit_install_name = True
    else:
        conf.env.explicit_install_name = False

    if Options.platform == 'sunos':
        conf.check_cc(function_name='socket', lib='socket', header_name='sys/socket.h', uselib_store='socket')
        if not conf.env.HAVE_SOCKET:
            conf.fatal("xmms2 requires libsocket on Solaris.")
            raise SystemExit(1)
        conf.env.append_unique('CFLAGS', '-D_POSIX_PTHREAD_SEMANTICS')
        conf.env.append_unique('CFLAGS', '-D_REENTRANT')
        conf.env.append_unique('CFLAGS', '-std=gnu99')
        conf.env.socket_impl = 'socket'
    elif Options.platform == 'win32':
        if conf.options.winver:
            major, minor = [int(x) for x in Options.options.winver.split('.')]
        else:
            try:
                majot, minor = sys.getwindowsversion()[:2]
            except AttributeError:
                Logs.warn("No Windows version found and no version set. "
                          "Defaulting to 5.1 (XP). You will not be able to use "
                          "this build of XMMS2 on older Windows versions.")
                major, minor = (5, 1)
        need_wspiapi = True
        if ((major, minor) >= (5, 1)):
            need_wspiapi = False

        # check_cc is mandatory by default.
        for lib in 'wsock32 ws2_32'.split():
            conf.check_cc(lib=lib, uselib_store="socket")
        for hdr in 'windows.h ws2tcpip.h winsock2.h'.split():
            conf.check_cc(header_name=hdr, uselib="socket")
        code = """
#include <ws2tcpip.h>
#include <wspiapi.h>

int main() { return 0; }
"""
        if not conf.check_cc(header_name="wspiapi.h", fragment=code,
                uselib="socket", mandatory=need_wspiapi):
            Logs.warn('This XMMS2 will only run on Windows XP and newer '
                    'machines. If you are planning to use XMMS2 on older '
                    'versions of Windows or are packaging a binary, please '
                    'consider installing the WSPiApi.h header for '
                    'compatibility. It is provided by the Platform SDK.')
        conf.env.DEFINES += [
                '_WIN32_WINNT=0x%02x%02x' % (major, minor),
                'HAVE_WINSOCK2=1'
                ]
        conf.env.socket_impl = 'wsock32'
    else:
        conf.env.socket_impl = 'posix'

    conf.env.xmms_icon = False
    if Options.platform == 'win32':
        try:
            conf.check_tool('winres')
        except Errors.ConfigurationError:
            pass
        else:
            conf.env.WINRCFLAGS = '-I%s' % os.path.abspath('pixmaps')
            conf.env.xmms_icon = True

    # TaskGen.mac_bundle option seems to be no longer silently ignored
    # if gcc -bundle option is not available.
    # TODO: Add --no-mac-bundle in options ?
    conf.env.mac_bundle_enabled = Options.platform == 'darwin'

    conf.check_cfg(package='glib-2.0', atleat_version='2.8.0',
            uselib_store='glib2', args='--cflags --libs')

    enabled_plugins, disabled_plugins = _configure_plugins(conf)
    enabled_optionals, disabled_optionals = _configure_optionals(conf)

    plugins = conf.env.XMMS_PLUGINS_ENABLED
    plugindirs = ["src/plugins/%s" % plugin for plugin in plugins]
    optionaldirs = conf.env.XMMS_OPTIONAL_BUILD
    newest = get_newest(subdirs, plugindirs, optionaldirs)
    conf.env.NEWEST_WSCRIPT_SUBDIR = newest

    [conf.sub_config(s) for s in subdirs]
    conf.write_config_header('xmms_configuration.h')

    output_plugins = [name for x, name in conf.env.XMMS_OUTPUT_PLUGINS if x > 0]

    _output_summary(
            enabled_plugins, disabled_plugins,
            enabled_optionals, disabled_optionals,
            output_plugins)

    return True

####
## Options
####
def _list_cb(option, opt, value, parser):
    """Callback that lets you specify lists of targets."""
    vals = value.replace(' ','').split(',')
    if vals == ['']:
        vals = []
    if getattr(parser.values, option.dest):
        vals += getattr(parser.values, option.dest)
    setattr(parser.values, option.dest, vals)

def options(opt):
    opt.tool_options('gnu_dirs')
    opt.tool_options('gcc')

    opt.add_option('--with-custom-version', type='string',
                   dest='customversion', help="Override git commit hash version")
    opt.add_option('--with-plugins', action="callback", callback=_list_cb,
                   type="string", dest="enable_plugins", default=None,
                   help="Comma separated list of plugins to build")
    opt.add_option('--without-plugins', action="callback", callback=_list_cb,
                   type="string", dest="disable_plugins", default=None,
                   help="Comma separated list of plugins to skip")
    opt.add_option('--with-default-output-plugin', type='string',
                   dest='default_output_plugin',
                   help="Force a default output plugin")
    opt.add_option('--with-optionals', action="callback", callback=_list_cb,
                   type="string", dest="enable_optionals", default=None,
                   help="Comma separated list of optionals to build")
    opt.add_option('--without-optionals', action="callback", callback=_list_cb,
                   type="string", dest="disable_optionals", default=None,
                   help="Comma separated list of optionals to skip")
    opt.add_option('--conf-prefix', action="callback", callback=_list_cb,
                   type='string', dest='config_prefix',
                   help="Specify a directory to prepend to configuration prefix")
    opt.add_option('--without-xmms2d', action='store_true', default=False,
                   dest='without_xmms2d', help="Skip build of xmms2d")
    opt.add_option('--with-pkgconfigdir', type='string', dest='pkgconfigdir',
                   help="Specify directory where to install pkg-config files")
    opt.add_option('--with-target-platform', type='string',
                   dest='target_platform',
                   help="Force a target platform (cross-compilation)")
    opt.add_option('--with-windows-version', type='string', dest='winver',
                   help="Force a specific Windows version (cross-compilation)")
    opt.add_option('--with-profiling', action="store_true", default=False,
                   dest='with_profiling', help="Enable profiling")
    opt.add_option('--run-tests', action='store_true', default=False,
                   dest='run_tests', help="Run test suite")
    opt.add_option('--with-ldconfig', action='store_true', default=None,
                   dest='ldconfig', help="Run ldconfig after install even if not root")
    opt.add_option('--without-ldconfig', action='store_false',
                   dest='ldconfig', help="Don't run ldconfig after install")

    opt.sub_options("src/xmms")
    for o in optional_subdirs + subdirs:
        opt.sub_options(o)

def shutdown(ctx):
    if 'install' in Options.commands and (
            ctx.options.ldconfig or
            (ctx.options.ldconfig is None and os.geteuid() == 0)
            ):
        ldconfig = '/sbin/ldconfig'
        if os.path.isfile(ldconfig):
            libprefix = Utils.subst_vars('${PREFIX]/lib', ctx.env)
            try:
                import subprocess
                subprocess.check_output(['ldconfig', 'libprefix'])
            except:
                pass

    if ctx.options.run_tests:
        os.system(os.path.join(blddir, "default/tests/test_xmmstypes"))
        os.system(os.path.join(blddir, "default/tests/test_server"))
