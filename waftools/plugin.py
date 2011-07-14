# Stock plugin configuration and build methods. These factor out the
# common tasks carried out by plugins in order to configure and build
# themselves.

from waflib.Errors import ConfigurationError

def plugin(name, source=None, configure=False, build=False,
           build_replace=False, libs=[],
           tool='c', broken=False, output_prio=None):
    def stock_configure(conf):
        if broken:
            conf.msg('%s plugin' % name, 'disabled (broken)', color='RED')
            return

        if configure:
            configure(conf)

        conf.env.XMMS_PLUGINS_ENABLED.append(name)
        if output_prio:
            conf.env.XMMS_OUTPUT_PLUGINS.append((output_prio, name))

    def stock_build(bld):
        pat = tool=='c' and '*.c' or '*.cpp'
        obj = bld(
            features = '%(tool)s %(tool)sshlib' % dict(tool=tool),
            target = 'xmms_%s' % name,
            source = source or bld.path.ant_glob(pat),
            includes = '../../.. ../../include',
            uselib = ['glib2'] + libs,
            use = bld.env.xmms_shared_library and 'xmms2core' or '',
            install_path = '${PLUGINDIR}',
            mac_bundle = bld.env.mac_bundle_enabled,
        )

        if build:
            build(bld, obj)

    return stock_configure, stock_build
