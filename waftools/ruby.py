#!/usr/bin/env python
# encoding: utf-8
# daniel.svensson at purplescout.se 2008

import Task, Options, Utils
from TaskGen import taskgen, before, feature
from Configure import conf

@taskgen
@before('apply_incpaths', 'apply_type_vars', 'apply_lib_vars')
@feature('rubyext')
@before('apply_bundle')
def init_rubyext(self):
    self.default_install_path = '${ARCHDIR_RUBY}'
    self.uselib = self.to_list(getattr(self, 'uselib', ''))
    if not 'RUBY' in self.uselib:
        self.uselib.append('RUBY')

@before('apply_link')
@feature('rubyext')
def apply_ruby_so_name(self):
    self.env['shlib_PATTERN'] = self.env['rubyext_PATTERN']

@conf
def check_ruby_version(conf, minver=()):
    """
    Checks if ruby is installed.
    If installed the variable RUBY will be set in environment.
    Ruby binary can be overridden by --with-ruby-binary config variable
    """
    if Options.options.rubybinary:
        ruby = Options.options.rubybinary
    else:
        ruby = conf.find_program("ruby", var="RUBY")
        if not ruby:
            return False

    conf.env['RUBY'] = ruby

    res = True

    version = Utils.cmd_output(ruby + " -e 'puts defined?(VERSION) ? VERSION : RUBY_VERSION'").strip()
    if version and minver:
        conf.env['RUBY_VERSION'] = tuple(int(x) for x in version.split("."))
        res = not (conf.env['RUBY_VERSION'] < minver)

    if not version:
        version = "Unknown"

    cver = ".".join(str(x) for x in minver)

    conf.check_message('ruby', cver, res, version)

    return res

@conf
def check_ruby_ext_devel(conf):
    if not (conf.env['RUBY'] and conf.env['RUBY_VERSION']):
        return False


    ruby = conf.env['RUBY']
    version = conf.env['RUBY_VERSION']

    def ruby_get_config(key):
        return Utils.cmd_output(ruby + " -rrbconfig -e 'print Config::CONFIG[\"" + key + "\"]'").strip()

    if version >= (1, 9, 0):
        ruby_h = Utils.cmd_output(ruby + " -rrbconfig -e 'puts File.exist?(Config::CONFIG[\"rubyhdrdir\"] + \"/ruby.h\")'").strip()
    elif version >= (1, 8, 0):
        ruby_h = Utils.cmd_output(ruby + " -rrbconfig -e 'puts File.exist?(Config::CONFIG[\"archdir\"] + \"/ruby.h\")'").strip()

    if ruby_h != 'true':
        conf.check_message('ruby', 'header file', False)
        return False

    conf.check_message('ruby', 'header file', True)

    archdir = Utils.cmd_output(ruby + " -rrbconfig -e 'puts \"%s\" % [].fill(Config::CONFIG[\"archdir\"], 0..1)'").strip()
    conf.env["CPPPATH_RUBY"] = [archdir]
    conf.env["LINKFLAGS_RUBY"] = '-L%s' % archdir

    if version >= (1, 9, 0):
        incpaths = Utils.cmd_output(ruby + " -rrbconfig -e 'puts Config::CONFIG[\"rubyhdrdir\"]'").strip()
        conf.env["CPPPATH_RUBY"] += [incpaths]

        incpaths = Utils.cmd_output(ruby + " -rrbconfig -e 'puts File.join(Config::CONFIG[\"rubyhdrdir\"], Config::CONFIG[\"arch\"])'").strip()
        conf.env["CPPPATH_RUBY"] += [incpaths]

    ldflags = Utils.cmd_output(ruby + " -rrbconfig -e 'print Config::CONFIG[\"LDSHARED\"]'").strip()

    # ok this is really stupid, but the command and flags are combined.
    # so we try to find the first argument...
    flags = ldflags.split()
    while flags and flags[0][0] != '-':
        flags = flags[1:]

    # we also want to strip out the deprecated ppc flags
    if len(flags) > 1 and flags[1] == "ppc":
        flags = flags[2:]

    conf.env["LINKFLAGS_RUBY"] += " " + " ".join(flags)

    ldflags = ruby_get_config("LIBS")
    conf.env["LINKFLAGS_RUBY"] += " " + ldflags
    
    ldflags = ruby_get_config("LIBRUBYARG_SHARED")
    conf.env["LINKFLAGS_RUBY"] += " " + ldflags

    cflags = ruby_get_config("CCDLFLAGS")
    conf.env["CCFLAGS_RUBY"] = cflags

    if Options.options.rubyarchdir:
        conf.env["ARCHDIR_RUBY"] = Options.options.rubyarchdir
    else:
        conf.env["ARCHDIR_RUBY"] = Utils.cmd_output(ruby + " -rrbconfig -e 'print Config::CONFIG[\"sitearchdir\"]'").strip()

    if Options.options.rubylibdir:
        conf.env["LIBDIR_RUBY"] = Options.options.rubylibdir
    else:
        conf.env["LIBDIR_RUBY"] = Utils.cmd_output(ruby + " -rrbconfig -e 'print Config::CONFIG[\"sitelibdir\"]'").strip()

    conf.env['rubyext_PATTERN'] = '%s.' + Utils.cmd_output(ruby + " -rrbconfig -e 'print Config::CONFIG[\"DLEXT\"]'").strip()

    # Change some strings to a list
    conf.env["LINKFLAGS_RUBY"] = Utils.to_list(conf.env["LINKFLAGS_RUBY"])
    conf.env["CPPPATH_RUBY"] = Utils.to_list(conf.env["CPPPATH_RUBY"])
    conf.env["CCFLAGS_RUBY"] = Utils.to_list(conf.env["CCFLAGS_RUBY"])

    return True

def set_options(opt):
    opt.add_option('--with-ruby-archdir', type="string", dest="rubyarchdir", help="Specify directory where to install arch specific files")
    opt.add_option('--with-ruby-libdir', type="string", dest="rubylibdir", help="Specify alternate ruby library path")
    opt.add_option('--with-ruby-binary', type='string', dest='rubybinary', help="Specify alternate ruby binary")
