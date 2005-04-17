/** @file gnomevfs.c
 *  Transport plugin for the GNOME VFS 2.0.
 * 
 *  Copyright (C) 2005 Daniel Svensson, <daniel@nittionio.nu>
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */


#include "xmms/log.h"
#include "xmms/magic.h"
#include "xmms/plugin.h"
#include "xmms/transport.h"
#include "xmms/util.h"
#include "xmms/xmms.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>

#include <libgnomevfs/gnome-vfs.h>



/*
 * Type definitions
 */

typedef struct {
	GnomeVFSHandle *handle;
	gchar *urlptr;
	const gchar *mime;
} xmms_gnomevfs_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_gnomevfs_can_handle (const gchar *url);
static gboolean xmms_gnomevfs_init (xmms_transport_t *transport, 
									const gchar *url);
static void xmms_gnomevfs_close (xmms_transport_t *transport);
static gint xmms_gnomevfs_read (xmms_transport_t *transport, gchar *buffer, 
								guint len, xmms_error_t *error);
static gint xmms_gnomevfs_size (xmms_transport_t *transport);
static gint xmms_gnomevfs_seek (xmms_transport_t *transport, gint offset, 
								gint whence);
static guint xmms_gnomevfs_lmod (xmms_transport_t *transport);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_TRANSPORT, "gnomevfs",
			"GNOME VFS transport plugin" XMMS_VERSION,
		 	"A transport that itself supports a wide range of transports,"
			"http, webdav, tar, ssh and more.");

	xmms_plugin_info_add (plugin, "URL", "http://www.gnome.org/");
	xmms_plugin_info_add (plugin, "Author", "Daniel Svensson");
	xmms_plugin_info_add (plugin, "E-Mail", "daniel@nittionio.nu");
	
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE, 
							xmms_gnomevfs_can_handle);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_INIT, 
							xmms_gnomevfs_init);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, 
							xmms_gnomevfs_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_READ, 
							xmms_gnomevfs_read);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SIZE, 
							xmms_gnomevfs_size);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SEEK, 
							xmms_gnomevfs_seek);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_LMOD, 
							xmms_gnomevfs_lmod);

	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_SEEK);
	xmms_plugin_properties_add (plugin, XMMS_PLUGIN_PROPERTY_LIST);
	
	return plugin;
}

/*
 * Member functions
 */
static gboolean
xmms_gnomevfs_can_handle (const gchar *url)
{
	gboolean retval = FALSE;
	
	g_return_val_if_fail (url, FALSE);

	XMMS_DBG ("xmms_gnomevfs_can_handle (%s)", url);
	
	if ((g_strncasecmp (url, "ssh:", 4) == 0))
		retval = TRUE;
	
	return retval;
}


static gboolean
xmms_gnomevfs_init (xmms_transport_t *transport, const gchar *url)
{
	xmms_gnomevfs_data_t *data;
	GnomeVFSFileInfo *info;
	GnomeVFSHandle *handle;
	GnomeVFSResult result;

	g_return_val_if_fail (transport, FALSE);
	g_return_val_if_fail (url, FALSE);

	if (!gnome_vfs_init ())
		return FALSE;
	
	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (url, info, 
					  GNOME_VFS_FILE_INFO_DEFAULT);
	gnome_vfs_file_info_unref (info);

	if (result != GNOME_VFS_OK) {
		return FALSE;
	}

	result = gnome_vfs_open (&handle, url, 
				 GNOME_VFS_OPEN_READ | 
				 GNOME_VFS_OPEN_RANDOM);
	
	if (result != GNOME_VFS_OK) {
		return FALSE;
	}

	data = g_new0 (xmms_gnomevfs_data_t, 1);
	data->mime = NULL;
	data->urlptr = g_strdup (url);
	data->handle = handle;
	xmms_transport_private_data_set (transport, data);

	data->mime = xmms_magic_mime_from_file ((const gchar*)data->urlptr);
	if (!data->mime) {
		return FALSE;
	}
	xmms_transport_mimetype_set (transport, (const gchar*)data->mime);
	
	return TRUE;
}


static void
xmms_gnomevfs_close (xmms_transport_t *transport)
{
	XMMS_DBG("XMMS_GNOMEVFS_CLOSE");
	xmms_gnomevfs_data_t *data;
	
	g_return_if_fail (transport);

	data = xmms_transport_private_data_get (transport);
	g_return_if_fail (data);
	
	gnome_vfs_close (data->handle);
	gnome_vfs_shutdown ();
	
	g_free (data->urlptr);

	g_free (data);
}


static gint
xmms_gnomevfs_read (xmms_transport_t *transport, gchar *buffer, guint len, 
					xmms_error_t *error)
{
	xmms_gnomevfs_data_t *data;
	GnomeVFSFileSize bytes_read;
	GnomeVFSResult result;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (error, -1);
	
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	if (data->mime) {
		data->mime = xmms_magic_mime_from_file ((const gchar*)data->urlptr);
		xmms_transport_mimetype_set (transport, (const gchar*)data->mime);
		data->mime = NULL;
	}

	result = gnome_vfs_read (data->handle, buffer, len, &bytes_read);

	if (result == GNOME_VFS_ERROR_EOF) {
		xmms_error_set (error, XMMS_ERROR_EOS, "End of file reached.");
	}

	if (result != GNOME_VFS_OK) {
		xmms_log_error ("GnomeVFS failed to read.");
		xmms_error_set (error, XMMS_ERROR_GENERIC, "GnomeVFS hickup, failure" 
						"to read the file");
	}

	return bytes_read;
}


static gint
xmms_gnomevfs_seek (xmms_transport_t *transport, gint offset, gint whence)
{
	xmms_gnomevfs_data_t *data;
	GnomeVFSSeekPosition w = GNOME_VFS_SEEK_START;
	GnomeVFSResult result;
	GnomeVFSFileSize offset_return = -1;

	g_return_val_if_fail (transport, -1);
	
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	switch (whence) {
		case XMMS_TRANSPORT_SEEK_SET:
			w = GNOME_VFS_SEEK_START;
			break;
		case XMMS_TRANSPORT_SEEK_END:
			w = GNOME_VFS_SEEK_END;
			break;
		case XMMS_TRANSPORT_SEEK_CUR:
			w = GNOME_VFS_SEEK_CURRENT;
			break;
	}

	result = gnome_vfs_seek (data->handle, w, offset);
	if (result != GNOME_VFS_OK) {
		xmms_log_error ("GnomeVFS failed to seek.");
		return -1;
	}

	result = gnome_vfs_tell (data->handle, &offset_return);
	if (result != GNOME_VFS_OK) {
		xmms_log_error ("GnomeVFS failed to tell.");
		return -1;
	}

	return offset_return;
}


static guint
xmms_gnomevfs_lmod (xmms_transport_t *transport)
{
	xmms_gnomevfs_data_t *data;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;

	guint lmod = 0;

	g_return_val_if_fail (transport, 0);
	
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, 0);

	info = gnome_vfs_file_info_new ();

	result = gnome_vfs_get_file_info_from_handle (data->handle, info, 
												  GNOME_VFS_FILE_INFO_DEFAULT);
	
	if (result == GNOME_VFS_OK && info != NULL)
		lmod = info->mtime;

	gnome_vfs_file_info_unref (info);

	return lmod;
}


static gint
xmms_gnomevfs_size (xmms_transport_t *transport)
{
	xmms_gnomevfs_data_t *data;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;

	guint size = -1;

	g_return_val_if_fail (transport, -1);
	
	data = xmms_transport_private_data_get (transport);
	g_return_val_if_fail (data, -1);

	info = gnome_vfs_file_info_new ();

	result = gnome_vfs_get_file_info_from_handle (data->handle, info, 
												  GNOME_VFS_FILE_INFO_DEFAULT);
	
	if (result == GNOME_VFS_OK && info != NULL)
		size = info->size;

	gnome_vfs_file_info_unref (info);

	return size;
}
