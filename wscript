#!/usr/bin/env python
# encoding: utf-8
#
# Copyright 2006 David Anderson <dave at natulte.net>

import os
import sys

# Waf removes the current dir from the python path. We readd it to
# import gittools.
sys.path = [os.getcwd()]+sys.path

import gittools

from Params import fatal

VERSION="0.2 DrGonzo+WIP (git commit: %s)" % gittools.get_info_str()
APPNAME='xmms2'

srcdir='.'
blddir = '_build_'

def init():
  import gc
  gc.disable()

def build(bld):
  # Build the XMMS2 defs file
  defs = bld.create_obj('subst')
  defs.source = 'src/include/xmms/xmms_defs.h.in'
  defs.target = 'src/include/xmms/xmms_defs.h'
  defs.dict = bld.env_of_name('default')

  # Process subfolders
  bld.add_subdirs('src/lib/xmmssocket src/lib/xmmsipc src/xmms')

def configure(conf):
  conf.check_tool('gcc')

  # Set the platform in the configure env
  platform_names = ['linux', 'freebsd', 'openbsd',
                    'netbsd', 'dragonfly']
  for platform in platform_names:
    if sys.platform.startswith(platform):
      conf.env['PLATFORM'] = platform
      break

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

def set_options(opt):
  pass
