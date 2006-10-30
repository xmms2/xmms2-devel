#!/usr/bin/env python
# encoding: utf-8
#
# Copyright 2006 David Anderson <dave at natulte.net>

import os
import os.path
import sys

# Waf removes the current dir from the python path. We readd it to
# import gittools.
sys.path = [os.getcwd()]+sys.path

import gittools

from Params import fatal, pprint

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

####
## Build
####
def build(bld):
  # Build the XMMS2 defs file
  defs = bld.create_obj('subst')
  defs.source = 'src/include/xmms/xmms_defs.h.in'
  defs.target = 'src/include/xmms/xmms_defs.h'
  defs.dict = bld.env_of_name('default')['XMMS_DEFS']

  # Process subfolders
  bld.add_subdirs('src/lib/xmmssocket src/lib/xmmsipc src/xmms')

  # Build configured plugins
  plugins = bld.env_of_name('default')['XMMS_PLUGINS_ENABLED']
  bld.add_subdirs(["src/plugins/%s" % plugin for plugin in plugins])

  # Build the client lib
  bld.add_subdirs('src/clients/lib/xmmsclient')

####
## Configuration
####
def _set_defs(conf):
  """Set the values needed by xmms_defs.h.in in the environment."""

  defs = {}

  platform_names = ['linux', 'freebsd', 'openbsd',
                    'netbsd', 'dragonfly']
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

  # Glib is required by all plugins, so check for it here and let them
  # assume its presence.
  conf.check_tool('checks')
  conf.check_pkg2('glib-2.0', version='2.6.0', uselib='glib-2.0')
  conf.env['XMMS_PLUGINS_ENABLED'] = []
  disabled = []
  for plugin in os.listdir('src/plugins'):
    conf.sub_config("src/plugins/%s" % plugin)
    if conf.env['XMMS_PLUGINS_ENABLED'][-1] != plugin:
      disabled.append(plugin)

  print "\nPlugins configuration:\n======================"
  print " Enabled: ",
  pprint('BLUE', " ".join(conf.env['XMMS_PLUGINS_ENABLED']))
  print " Disabled: ",
  pprint('BLUE', " ".join(disabled))

def configure(conf):
  conf.check_tool('gcc')

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

  _configure_plugins(conf)
  _set_defs(conf)

####
## Options
####
def set_options(opt):
  opt.tool_options('gcc')
