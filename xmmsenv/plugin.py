# Stock plugin configuration and build methods. These factor out the
# common tasks carried out by plugins in order to configure and build
# themselves.

def plugin(name, sources=None, configure=False, build=False, build_replace=False):
  def stock_configure(conf):
    if configure and not configure(conf):
      return
    conf.env['XMMS_PLUGINS_ENABLED'].append(name)

  def stock_build(bld):
    obj = bld.create_obj('cc', 'plugin')
    obj.target = 'xmms_%s' % name
    obj.includes = '../../include'
    obj.source = '%s.c' % name
    obj.uselib = 'glib-2.0'
    if build:
      build(bld, obj)

  if build_replace:
    return stock_configure, build_replace
  else:
    return stock_configure, stock_build
