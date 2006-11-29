# Stock plugin configuration and build methods. These factor out the
# common tasks carried out by plugins in order to configure and build
# themselves.

import sys

def plugin(name, source=None, configure=False, build=False,
           build_replace=False, needs_lib=False, extra_libs=[],
           tool='cc', broken=False):
  def stock_configure(conf):
    if broken:
      conf.check_message_custom('%s plugin' % name, '',
                                'currently broken, disabled')
      return
    if configure and not configure(conf):
      return
    conf.env['XMMS_PLUGINS_ENABLED'].append(name)

  def stock_build(bld):
    obj = bld.create_obj(tool, 'plugin')
    obj.target = 'xmms_%s' % name
    obj.includes = '../../include'
    if source:
      obj.source = source
    else:
      obj.source = '%s.c' % name

    libs = ['glib-2.0']
    if needs_lib:
      libs.append(name)
    libs += extra_libs
    obj.uselib = ' '.join(libs)
    if sys.platform == 'win32':
      obj.uselib_local = 'xmms2d'

    obj.install_in = bld.env_of_name('default')["XMMS_DEFS"]["PKGLIBDIR"]

    if build:
      build(bld, obj)

  if build_replace:
    return stock_configure, build_replace
  else:
    return stock_configure, stock_build
