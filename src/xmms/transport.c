/** @file 
 * Controls transport plugins.
 *
 * This file is responsible for the transportlayer.
 */

#include "transport.h"
#include "transport_int.h"
#include "plugin.h"
#include "plugin_int.h"
#include "object.h"
#include "util.h"
#include "ringbuf.h"
#include "signal_xmms.h"
#include "playlist.h"

#include <glib.h>
#include <string.h>

#include <sys/stat.h>

/*
 * Static function prototypes
 */

static void xmms_transport_destroy (xmms_transport_t *transport);
static xmms_plugin_t *xmms_transport_plugin_find (const gchar *url);
static gpointer xmms_transport_thread (gpointer data);

/*
 * Type definitions
 */

/** This describes a directory or a file */
struct xmms_transport_entry_St {
	/** Absolute path to entry */
	gchar path[1024];
	xmms_transport_entry_type_t type;
};

struct xmms_transport_St {
	/** Object for emiting signals */
	xmms_object_t object;
	xmms_plugin_t *plugin; /**< The plugin used as media. */

	/** The entry that are transported.
	 * The url will be extracted from this
	 * upon open */
	xmms_playlist_entry_t *entry;

	GMutex *mutex;
	GCond *cond;
	GCond *seek_cond;
	GCond *mime_cond;
	GThread *thread;
	/** This is true if we are currently buffering. */
	gboolean running;

	xmms_ringbuf_t *buffer;
	/** String containing current mimetype */
	gchar *mimetype;
	/** Private plugin data */
	gpointer plugin_data;

	gchar *suburl;

	/* Seek */
	/** Set to TRUE if a seek is scheduled. */
	gboolean want_seek;
	gint seek_offset;
	gint seek_whence;
	
};

/*
 * Public functions
 */

/**
 * Recreates the Ringbuffer with new size.
 * Danger will robinsson!!!! This will kill all data in the buffer. 
 */

void
xmms_transport_ringbuf_resize (xmms_transport_t *transport, gint size)
{
	g_return_if_fail (transport);
	g_return_if_fail (size);

	g_mutex_lock (transport->mutex);
	xmms_ringbuf_destroy (transport->buffer);
	transport->buffer = xmms_ringbuf_new (size);

	g_mutex_unlock (transport->mutex);
}

/**
 * Retrives a list of files from the transport plugin.
 *
 * @returns a list with xmms_transport_entry_t's
 */

GList *
xmms_transport_list (const gchar *path)
{
	xmms_transport_list_method_t list;
	xmms_plugin_t *plugin;

	g_return_val_if_fail (path, NULL);

	plugin = xmms_transport_plugin_find (path);

	if (!xmms_plugin_properties_check (plugin, XMMS_PLUGIN_PROPERTY_LIST))
		return NULL;

	g_return_val_if_fail (plugin, NULL);

	list = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_LIST);
	g_return_val_if_fail (list, NULL);

	return list (path);
}

/**
 * Free all resources used by a list with xmms_transport_entry_t's
 */

void
xmms_transport_list_free (GList *list)
{
	GList *node;

	g_return_if_fail (list);

	for (node = list; node; node = g_list_next (node)) {
		xmms_transport_entry_t *e = node->data;
		xmms_transport_entry_free (e);
	}

	g_list_free (list);
}


/**
 * Allocates a xmms_transport_entry_t
 * call xmms_transport_entry_free to free resources used by entry.
 *
 * @sa xmms_transport_entry_free
 */

xmms_transport_entry_t *
xmms_transport_entry_new (gchar *path, xmms_transport_entry_type_t type)
{
	xmms_transport_entry_t *ret;

	g_return_val_if_fail (path, NULL);

	ret = g_new (xmms_transport_entry_t, 1);
	g_strlcpy (ret->path, path, 1024);
	ret->type = type;

	return ret;
}

/**
 * Returns the plugin for this transport.
 */

xmms_plugin_t *
xmms_transport_plugin_get (const xmms_transport_t *transport)
{
	g_return_val_if_fail (transport, NULL);

	return transport->plugin;
}

/**
 * Free all resources used by entry
 */

void
xmms_transport_entry_free (xmms_transport_entry_t *entry)
{
	g_return_if_fail (entry);

	g_free (entry);
}

/**
 * Returns type of entry
 */

xmms_transport_entry_type_t
xmms_transport_entry_type_get (xmms_transport_entry_t *entry)
{
	g_return_val_if_fail (entry, 0);

	return entry->type;
}

/**
 * Get path of entry.
 */

const gchar *
xmms_transport_entry_path_get (xmms_transport_entry_t *entry)
{
	g_return_val_if_fail (entry, NULL);

	return entry->path;
}

/**
  * Get a transport's private data.
  *
  * @returns Pointer to private data.
  */

gpointer
xmms_transport_plugin_data_get (xmms_transport_t *transport)
{
	gpointer ret;
	g_return_val_if_fail (transport, NULL);

	xmms_transport_lock (transport);
	ret = transport->plugin_data;
	xmms_transport_unlock (transport);

	return ret;
}

/**
 * Set a transport's private data
 *
 * @param transport the transport to store the pointer in.
 * @param data pointer to private data.
 */

void
xmms_transport_plugin_data_set (xmms_transport_t *transport, gpointer data)
{
	xmms_transport_lock (transport);
	transport->plugin_data = data;
	xmms_transport_unlock (transport);
}

/**
 * Wether this transport is a local one.
 * 
 * A local transport is file / socket / fd
 * Remote transports are http / ftp.
 * This is decided by the plugin with the property 
 * XMMS_PLUGIN_PROPERTY_LOCAL
 *
 * @param transport the transport structure.
 * @returns TRUE if this transport plugin has XMMS_PLUGIN_PROPERTY_LOCAL
 */
gboolean
xmms_transport_islocal (xmms_transport_t *transport)
{
	g_return_val_if_fail (transport, FALSE);

	return xmms_plugin_properties_check (transport->plugin, XMMS_PLUGIN_PROPERTY_LOCAL);
}


/**
 * Sets this transports mimetype.
 *
 * This should be called from the plugin to propagate the mimetype
 */
void
xmms_transport_mimetype_set (xmms_transport_t *transport, const gchar *mimetype)
{
	g_return_if_fail (transport);
	g_return_if_fail (mimetype);

	xmms_transport_lock (transport);
	
	if (transport->mimetype)
		g_free (transport->mimetype);
	transport->mimetype = g_strdup (mimetype);

	xmms_transport_unlock (transport);
	
	if (transport->running)
		xmms_object_emit (XMMS_OBJECT (transport), XMMS_SIGNAL_TRANSPORT_MIMETYPE, mimetype);

	g_cond_signal (transport->mime_cond);
}

/**
 * Initialize a transport_t from a URI
 *
 * @returns a allocated xmms_transport_t that needs to be freed by a
 * xmms_transport_close ()
 */
xmms_transport_t *
xmms_transport_new ()
{
	xmms_transport_t *transport;

	transport = g_new0 (xmms_transport_t, 1);
	xmms_object_init (XMMS_OBJECT (transport));
	transport->mutex = g_mutex_new ();
	transport->cond = g_cond_new ();
	transport->seek_cond = g_cond_new ();
	transport->mime_cond = g_cond_new ();
	transport->buffer = xmms_ringbuf_new (XMMS_TRANSPORT_RINGBUF_SIZE);
	transport->want_seek = FALSE;

	return transport;
}

/**
 * Opens the transport.
 *
 * This will call the plugins open function. 
 * Could be a socket opening or a FD or something
 * like that.
 *
 * @param transport the transport structure.
 * @param entry A entry containing a URL that should be
 * 	  used by this transport.
 */

gboolean
xmms_transport_open (xmms_transport_t *transport, xmms_playlist_entry_t *entry)
{
	g_return_val_if_fail (entry, FALSE);
	g_return_val_if_fail (transport, FALSE);

	XMMS_DBG ("Trying to open stream: %s", xmms_playlist_entry_url_get (entry));
	
	transport->plugin = xmms_transport_plugin_find 
		(xmms_playlist_entry_url_get (entry));
	if (!transport->plugin)
		return FALSE;
	transport->entry = entry;
	
	XMMS_DBG ("Found plugin: %s", xmms_plugin_name_get (transport->plugin));

	return xmms_transport_plugin_open (transport, entry, NULL);
}

/** 
 * Gets the current URL from the transport.
 */
const gchar *
xmms_transport_url_get (const xmms_transport_t *const transport)
{
	const gchar *ret;
	g_return_val_if_fail (transport, NULL);

	xmms_transport_lock (transport);
	ret =  xmms_playlist_entry_url_get (transport->entry);
	xmms_transport_unlock (transport);

	return ret;
}

/**
 * Gets the current entry
 */
xmms_playlist_entry_t *
xmms_transport_entry_get (xmms_transport_t *transport)
{
	xmms_playlist_entry_t *entry;

	g_return_val_if_fail (transport, NULL);

	xmms_transport_lock (transport);
	entry = transport->entry;
	xmms_transport_unlock (transport);

	return entry;
}

/**
 * Gets the suburl from the transport.
 */
const gchar *
xmms_transport_suburl_get (const xmms_transport_t *const transport)
{
	const gchar *ret;
	g_return_val_if_fail (transport, NULL);

	xmms_transport_lock (transport);
	ret =  transport->suburl;
	xmms_transport_unlock (transport);

	return ret;
}

/**
 * Gets the current mimetype
 * This can return NULL if plugin has not
 * called xmms_transport_mimetype_set()
 */
const gchar *
xmms_transport_mimetype_get (xmms_transport_t *transport)
{
	const gchar *ret;
	g_return_val_if_fail (transport, NULL);

	xmms_transport_lock (transport);
	ret =  transport->mimetype;
	xmms_transport_unlock (transport);

	return ret;
}

/**
 * Like xmms_transport_mimetype_get but blocks if
 * transport plugin has not yet called mimetype_set
 */
const gchar *
xmms_transport_mimetype_get_wait (xmms_transport_t *transport)
{
	const gchar *ret;
	g_return_val_if_fail (transport, NULL);

	xmms_transport_lock (transport);
	if (!transport->mimetype) {
		XMMS_DBG ("Wating for mime_cond");
		g_cond_wait (transport->mime_cond, transport->mutex);
	}
	ret = transport->mimetype;
	xmms_transport_unlock (transport);

	return ret;
}


/**
 * Reads len bytes into buffer.
 *
 * This function reads from the transport thread buffer, if you want to
 * read more then currently are buffered, it will wait for you. Does not
 * guarantee that all bytes are read, may return less bytes.
 *
 * @param transport transport to read from.
 * @param buffer where to store read data.
 * @param len number of bytes to read.
 * @returns number of bytes actually read, or -1 on error.
 *
 */
gint
xmms_transport_read (xmms_transport_t *transport, gchar *buffer, guint len)
{
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (len > 0, -1);


	if (!transport->running) {
		xmms_transport_read_method_t read_method;
		XMMS_DBG ("Doing unbuffered read...");
		read_method = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_READ);
		if (read_method) {
			gint ret = read_method (transport, buffer, len);
			return ret;
		}
		return -1;
	}
	
	xmms_transport_lock (transport);
	
	if (transport->want_seek) {
		g_cond_signal (transport->cond);
		g_cond_wait (transport->seek_cond, transport->mutex);
	}

	if (len > XMMS_TRANSPORT_RINGBUF_SIZE) {
		len = XMMS_TRANSPORT_RINGBUF_SIZE;
	}

	xmms_ringbuf_wait_used (transport->buffer, len, transport->mutex);
	ret = xmms_ringbuf_read (transport->buffer, buffer, len);
	
	xmms_transport_unlock (transport);

	return ret;
}

/**
  * Tells the transport thread to schedule a seek.
  *
  * This function will seek to a specific offset, it emulates the behaviour
  * of lseek. It will also clear the buffer.
  *
  * The whence parameter should be one of:
  * @li @c XMMS_TRANSPORT_SEEK_SET Sets position to offset from start of file
  * @li @c XMMS_TRANSPORT_SEEK_END Sets position to offset from end of file
  * @li @c XMMS_TRANSPORT_SEEK_CUR Sets position to offset from current position
  *
  * @param transport the transport to modify
  * @param offset offset in bytes
  * @param whence se above
  * 
  */

void
xmms_transport_seek (xmms_transport_t *transport, gint offset, gint whence)
{
	g_return_if_fail (transport);
	g_return_if_fail (!transport->want_seek);

	xmms_transport_lock (transport);

	if (!transport->running) {
		xmms_transport_seek_method_t seek_method;

		seek_method = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_SEEK);
		g_return_if_fail (seek_method);

		xmms_transport_unlock (transport);
		seek_method (transport, offset, whence);
		return;
	}

	transport->seek_offset = offset;
	transport->seek_whence = whence;
	transport->want_seek = TRUE;

	xmms_ringbuf_set_eos (transport->buffer, TRUE);

	xmms_transport_unlock (transport);

	g_cond_signal (transport->cond);
}

/**
  * Gets the total size of the transports media.
  *
  * @returns size of the media, or -1 if it can't be determined.
  */
gint
xmms_transport_size (xmms_transport_t *transport)
{
	xmms_transport_size_method_t size;
	g_return_val_if_fail (transport, -1);

	size = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_SIZE);
	g_return_val_if_fail (size, -1);

	return size (transport);
}

/**
 * Gets the plugin that was used to instantiate this transport
 *
 */
xmms_plugin_t *
xmms_transport_get_plugin (const xmms_transport_t *transport)
{
	g_return_val_if_fail (transport, NULL);

	return transport->plugin;

}

/**
 * Instantiate a transport plugin.
 *
 */

gboolean
xmms_transport_plugin_open (xmms_transport_t *transport, xmms_playlist_entry_t *entry, 
		gpointer data)
{
	xmms_transport_open_method_t init_method;
	xmms_plugin_t *plugin;
	gchar *url;
	
	plugin = transport->plugin;
	
	init_method = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_INIT);

	if (!init_method) {
		XMMS_DBG ("Transport has no init method!");
		return FALSE;
	}

	xmms_transport_plugin_data_set (transport, data);

	url = xmms_playlist_entry_url_get (transport->entry);

	transport->suburl = url + strlen(url); /* empty string */

	while (!init_method (transport, url)) {

		while (*--transport->suburl != '/' ){
			if (*transport->suburl == 0){ /* restore */
				*transport->suburl = '/';
			}
			if (transport->suburl <= url) {
				xmms_ringbuf_destroy (transport->buffer);
				g_mutex_free (transport->mutex);
				g_free (transport);
				transport = NULL;
				return FALSE;
			}
		}
		*transport->suburl = 0;
		transport->suburl++;

		if (!g_file_test (url, G_FILE_TEST_IS_DIR))
			return FALSE;

		XMMS_DBG ("Trying %s  (suburl: %s)",url,transport->suburl);
	}

	transport->suburl = g_strdup (transport->suburl);
	return TRUE;
}


/*
 * Private functions
 */

/**
  * Start the transport thread.
  * This should be called to make the transport start buffer.
  *
  * @internal
  */

void
xmms_transport_start (xmms_transport_t *transport)
{
	g_return_if_fail (transport);

	transport->running = TRUE;
	transport->thread = g_thread_create (xmms_transport_thread, transport, TRUE, NULL); 
}


/**
  * Tells the transport thread to quit and call xmms_transport_destroy
  *
  * @internal
  */

void
xmms_transport_close (xmms_transport_t *transport)
{
	g_return_if_fail (transport);

	if (transport->thread) {
		xmms_transport_lock (transport);
		transport->running = FALSE;
		xmms_ringbuf_set_eos (transport->buffer, TRUE);
		XMMS_DBG("Waking transport");
		g_cond_signal (transport->cond);
		xmms_transport_unlock (transport);
		g_thread_join (transport->thread);
	} else {
		xmms_transport_destroy (transport);
	}
}


/*
 * Static functions
 */

static void
xmms_transport_seek_real (xmms_transport_t *transport)
{
	xmms_transport_seek_method_t seek_method;
	g_return_if_fail (transport);

	seek_method = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_SEEK);
	g_return_if_fail (seek_method);

	XMMS_DBG ("Seeking to %d", transport->seek_offset);

	xmms_ringbuf_clear (transport->buffer);
	xmms_transport_unlock (transport);
	seek_method (transport, transport->seek_offset, transport->seek_whence);
	xmms_transport_lock (transport);

	transport->want_seek = FALSE;
	xmms_ringbuf_set_eos (transport->buffer, FALSE);
	g_cond_signal (transport->seek_cond);
}

static void
xmms_transport_destroy (xmms_transport_t *transport)
{
	xmms_transport_close_method_t close_method;

	close_method = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_CLOSE);
	
	if (close_method)
		close_method (transport);
	xmms_ringbuf_destroy (transport->buffer);
	g_cond_free (transport->seek_cond);
	g_cond_free (transport->mime_cond);
	g_cond_free (transport->cond);
	g_mutex_free (transport->mutex);
	xmms_object_cleanup (XMMS_OBJECT (transport));

	if (transport->mimetype)
		g_free (transport->mimetype);

	g_free (transport);
}

/**
 * Finds a transportplugin for this URL.
 *
 * @internal
 */

static xmms_plugin_t *
xmms_transport_plugin_find (const gchar *url)
{
	GList *list, *node;
	xmms_plugin_t *plugin = NULL;
	xmms_transport_can_handle_method_t can_handle;

	g_return_val_if_fail (url, NULL);

	list = xmms_plugin_list_get (XMMS_PLUGIN_TYPE_TRANSPORT);
	
	for (node = list; node; node = g_list_next (node)) {
		plugin = node->data;
		XMMS_DBG ("Trying plugin: %s", xmms_plugin_name_get (plugin));
		can_handle = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE);
		
		if (!can_handle)
			continue;

		if (can_handle (url)) {
			xmms_plugin_ref (plugin);
			break;
		}
	}
	if (!node)
		plugin = NULL;

	if (list)
		xmms_plugin_list_destroy (list);

	return plugin;
}

static gpointer
xmms_transport_thread (gpointer data)
{
	xmms_transport_t *transport = data;
	gchar buffer[4096];
	xmms_transport_read_method_t read_method;
	gint ret;

	g_return_val_if_fail (transport, NULL);
	
	read_method = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_READ);
	if (!read_method)
		return NULL;

	xmms_playlist_entry_ref (transport->entry);

	xmms_transport_lock (transport);
	while (transport->running) {
		if (transport->want_seek) {
			xmms_transport_seek_real (transport);
		}

		xmms_transport_unlock (transport);
		ret = read_method (transport, buffer, sizeof(buffer));
		xmms_transport_lock (transport);

		if (transport->want_seek) {
			continue;
		}

		if (ret > 0) {
			xmms_ringbuf_wait_free (transport->buffer, ret, transport->mutex);
			xmms_ringbuf_write (transport->buffer, buffer, ret);
		} else {
			if (ret == -1) {
				xmms_ringbuf_set_eos (transport->buffer, TRUE);
				XMMS_DBG("Sleeping on transport cond");
				g_cond_wait (transport->cond, transport->mutex);
			} else { /* ret == 0 */
				continue;
			}
		}
	}
	xmms_transport_unlock (transport);

	xmms_playlist_entry_unref (transport->entry);

	XMMS_DBG ("xmms_transport_thread: cleaning up");
	
	xmms_transport_destroy (transport);
	
	return NULL;
}
