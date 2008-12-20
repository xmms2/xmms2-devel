# Stock plugin configuration and build methods. These factor out the
# common tasks carried out by plugins in order to configure and build
# themselves.

def plugin(name, source=None, configure=False, build=False,
           build_replace=False, libs=[],
           tool='cc', broken=False, output_prio=None):
    def stock_configure(conf):
        if broken:
            conf.check_message_custom('%s plugin' % name, '',
                                      'disabled (broken)')
            return
        if configure and not configure(conf):
            return
        conf.env.append_value('XMMS_PLUGINS_ENABLED', name)
        if output_prio:
            conf.env.append_value('XMMS_OUTPUT_PLUGINS', (output_prio, name))

    def stock_build(bld):
        obj = bld.new_task_gen(tool, 'shlib')
        obj.target = 'xmms_%s' % name
        obj.includes = '../../.. ../../include'
        obj.mac_bundle = True

        if source:
            obj.source = source
        else:
            obj.source = ['%s.c' % name]

        obj.uselib = ['glib2'] + libs

        if bld.env['xmms_shared_library']:
            obj.uselib_local = 'xmms2core'

        obj.install_path = '${PLUGINDIR}'

        if build:
            build(bld, obj)

    return stock_configure, stock_build
