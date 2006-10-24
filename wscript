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
  return
  # Color disable code.
  import Params
  Params.g_col_scheme = [0 for _ in Params.g_col_scheme]
  Params.g_colors = dict([(k, "") for k,v
                          in zip(Params.g_col_names,
                                 Params.g_col_scheme)])

def build(bld):
  # Process subfolders
  bld.add_subdirs('src/lib/xmmssocket src/lib/xmmsipc src/xmms')

def configure(conf):
  conf.check_tool('gcc')

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
