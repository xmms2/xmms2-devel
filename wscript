#!/usr/bin/env python
# encoding: utf-8
#
# Copyright 2006 David Anderson <dave at natulte.net>

import os
import os.path
import sys
import optparse

# Waf removes the current dir from the python path. We readd it to
# import xmmsenv stuff.
sys.path = [os.getcwd()]+sys.path

from xmmsenv import sets # We have our own sets, to not depend on py2.4
from xmmsenv import gittools

from Params import fatal, pprint
import Params

VERSION="0.2 DrGonzo+WIP (git commit: %s)" % gittools.get_info_str()
APPNAME='xmms2'

srcdir='.'
blddir = '_build_'

####
## Initialization
####
def init():
  import gc
  gc.disable()

optional_subdirs = ["src/clients/cli",
                    "src/clients/et",
                    "src/clients/mdns/dns_sd",
                    "src/clients/mdns/avahi",
                    "src/clients/lib/xmmsclient++",
                    "src/clients/lib/python",
                    "src/clients/lib/ruby"]

####
## Build
####
def build(bld):
#  bld.set_variants('default debug')

  # Build the XMMS2 defs file
  defs = bld.create_obj('subst', 'uh')
  defs.source = 'src/include/xmms/xmms_defs.h.in'
  defs.target = 'src/include/xmms/xmms_defs.h'
  defs.dict = bld.env_of_name('default')['XMMS_DEFS']


  # Process subfolders
  bld.add_subdirs('src/lib/xmmssocket src/lib/xmmsipc src/lib/xmmsutils src/xmms')

  # Build configured plugins
  plugins = bld.env_of_name('default')['XMMS_PLUGINS_ENABLED']
  bld.add_subdirs(["src/plugins/%s" % plugin for plugin in plugins])

  # Build the client libs
  bld.add_subdirs('src/clients/lib/xmmsclient')
  bld.add_subdirs('src/clients/lib/xmmsclient-glib')

  # Build the clients
  bld.add_subdirs(bld.env_of_name('default')['XMMS_OPTIONAL_BUILD'])

  # Headers
  bld.add_subdirs('src/include')

####
## Configuration
####
def _set_defs(conf):
  """Set the values needed by xmms_defs.h.in in the environment."""

  defs = {}

  platform_names = ['linux', 'freebsd', 'openbsd',
                    'netbsd', 'dragonfly', 'darwin']
  for platform in platform_names:
    if sys.platform.startswith(platform):
      defs['PLATFORM'] = "XMMS_OS_%s" % platform.upper()
      break
  defs['VERSION'] = VERSION
  defs['PKGLIBDIR'] = os.path.join(conf.env['PREFIX'],
                                   'lib', 'xmms2')
  defs['BINDIR'] = os.path.join(conf.env['PREFIX'],
                                'bin')
  defs['SHAREDDIR'] = os.path.join(conf.env['PREFIX'],
                                  'share', 'xmms2')
  defs['DEFAULT_OUTPUT'] = 'alsa' # TODO(dave): fix
  defs['USERCONFDIR'] = '.config/xmms2'
  defs['SYSCONFDIR'] = '/etc/xmms2'

  conf.env['XMMS_DEFS'] = defs

def _configure_plugins(conf):
  """Process all xmms2d plugins"""
  import Params

  def _check_exist(plugins, msg):
    unknown_plugins = plugins.difference(all_plugins)
    if unknown_plugins:
      fatal(msg % {'unknown_plugins': ', '.join(unknown_plugins)})
    return plugins

  # Glib is required by all plugins, so check for it here and let them
  # assume its presence.
  conf.check_tool('checks')
  conf.check_pkg2('glib-2.0', version='2.6.0', uselib='glib-2.0')
  conf.env['XMMS_PLUGINS_ENABLED'] = []

  all_plugins = sets.Set([p for p in os.listdir("src/plugins") if os.path.exists(os.path.join("src/plugins",p,"wscript"))])

  # If an explicit list was provided, only try to process that
  if Params.g_options.plugins:
    selected_plugins = _check_exist(sets.Set(Params.g_options.plugins),
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

  print "\nOptional configuration:\n======================"
  print " Enabled:",
  pprint('BLUE', ', '.join(conf.env['XMMS_OPTIONAL_BUILD']))
  print " Disabled:",
  pprint('BLUE', ", ".join([i for i in optional_subdirs if i not in conf.env['XMMS_OPTIONAL_BUILD']]))
  print "\nPlugins configuration:\n======================"
  print " Enabled:",
  pprint('BLUE', ", ".join(conf.env['XMMS_PLUGINS_ENABLED']))
  print " Disabled:",
  pprint('BLUE', ", ".join(disabled_plugins))

def configure(conf):
  if (conf.check_tool('g++')):
    conf.env["HAVE_CXX"] = True
  else:
  	conf.env["HAVE_CXX"] = False
  conf.check_tool('gcc')

  conf.env["CCFLAGS"] += ['-g', '-O0']
  conf.env["CXXFLAGS"] += ['-g', '-O0']

  if Params.g_options.config_prefix:
    conf.env["LIBPATH"] += [os.path.join(Params.g_options.config_prefix, "lib")]
    conf.env["CCFLAGS"] += ["-I%s" % os.path.join(Params.g_options.config_prefix, "include")]
    conf.env["CXXFLAGS"] += ["-I%s" % os.path.join(Params.g_options.config_prefix, "include")]

  # Check for support for the generic platform
  has_platform_support = os.name in ('nt', 'posix')
  conf.check_message("platform code for", os.name,
                     has_platform_support)
  if not has_platform_support:
    fatal("xmms2 only has platform support for Windows "
          "and POSIX operating systems.")

  conf.sub_config('src/lib/xmmssocket')
  conf.sub_config('src/lib/xmmsipc')
  conf.sub_config('src/xmms')
  conf.sub_config('src/clients/lib/xmmsclient-glib')

  conf.env['XMMS_OPTIONAL_BUILD'] = []
  for o in optional_subdirs:
    if conf.sub_config(o):
      conf.env['XMMS_OPTIONAL_BUILD'].append(o)

  _configure_plugins(conf)
  _set_defs(conf)

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
  opt.add_option('--with-plugins', action="callback", callback=_list_cb,
                 type="string", dest="plugins")
  opt.add_option('--without-plugins', action="callback", callback=_list_cb,
                 type="string", dest="disable_plugins")
  opt.tool_options('gcc')
  for o in optional_subdirs:
    opt.sub_options(o)
  opt.add_option('--with-conf-prefix', type='string', dest='config_prefix')
