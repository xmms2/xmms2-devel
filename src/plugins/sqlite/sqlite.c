#include "xmms/plugin.h"
#include "xmms/medialib.h"
#include "xmms/util.h"
#include "xmms/magic.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

/*
 * Type definitions
 */

typedef struct {
} xmms_sqlite_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_sqlite_new (xmms_medialib_t *medialib);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_MEDIALIB, "sqlite",
			"SQLite medialib " VERSION,
		 	"SQLite backend to medialib");

	xmms_plugin_info_add (plugin, "URL", "http://www.xmms.org/");
	xmms_plugin_info_add (plugin, "URL", "http://www.sqlite.org/");
	xmms_plugin_info_add (plugin, "Author", "XMMS Team");
	
	xmms_plugin_method_add (plugin, XMMS_METHOD_NEW, xmms_sqlite_new);

	return plugin;
}

/*
 * Member functions
 */

gboolean
xmms_sqlite_new (xmms_medialib_t *medialib)
{
	gchar *dbpath;
	g_return_val_if_fail (medialib, FALSE);

	g_snprintf (dbpath, XMMS_PATH_MAX, "%s/.xmms2/medialib.db", g_get_home_dir ());

	return TRUE;

}
