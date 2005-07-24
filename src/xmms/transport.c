/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
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




/** @file 
 * Control transport plugins.
 *
 * This file is responsible for the transportlayer.
 */

#include "xmmspriv/xmms_transport.h"
#include "xmmspriv/xmms_ringbuf.h"
#include "xmmspriv/xmms_plugin.h"
#include "xmms/xmms_transportplugin.h"
#include "xmms/xmms_object.h"
#include "xmms/xmms_log.h"

#include <glib.h>
#include <string.h>

#include <sys/stat.h>

/*
 * Static function prototypes
 */

static void xmms_transport_destroy (xmms_object_t *object);
static xmms_plugin_t *xmms_transport_plugin_find (const gchar *url);
static gpointer xmms_transport_thread (gpointer data);

/*
 * Type definitions
 */

/** 
 * @defgroup Transport Transport
 * @ingroup XMMSServer
 * @brief Responsible to read encoded data from source.
 *
 * The transport is responsible for reading encoded data from 
 * a source. The data will be put in ringbuffer that the decoder 
 * reads from. Transports are also responsible to tell what kind
 * of mimetype the source data is.
 *
 * @{
 */

/** this is the main transport struct. */
struct xmms_transport_St {
	/** Object for emiting signals */
	xmms_object_t object;
	xmms_plugin_t *plugin; /**< The plugin used as media. */

	/** 
	 * The entry that are transported.
	 * The url will be extracted from this
	 * upon open 
	 */
	xmms_medialib_entry_t entry;

	GMutex *mutex;
	GCond *cond;
	/**
	 * Signal on this cond when the mimetype is extracted.
	 */
	GCond *mime_cond;
	GThread *thread;
	/** This is true if we are currently buffering. */
	gboolean running;

	/**
	 * Put the source data in this buffer
	 */
	xmms_ringbuf_t *buffer;
	/** String containing current mimetype */
	gchar *mimetype;
	/** Private plugin data */
	gpointer plugin_data;

	
	/**
	 * in order to avoid a lot of buffer kills when
	 * opening a file (many decoders need to seek a lot
	 * when opening a file). We don't start buffering until
	 * we read from the buffer twice in a row. If we seek
	 * we reset the numread to 0.
	 */
	gint numread; 	
	gboolean buffering;

	gboolean have_mimetype;

	/** Number of bytes read from the transport */
	guint64 total_bytes;

	/** Number of buffer underruns */
	guint32 buffer_underruns;

	/** Error status for when we're buffering */
	xmms_error_t status;

	guint64 current_position; 	

	/** Used for linereading */
	struct {
		gchar buf[XMMS_TRANSPORT_MAX_LINE_SIZE];
		gchar *bufend;
	} lr;
};

/** @} */

/** 
 * @defgroup TransportPlugin TransportPlugin
 * @ingroup XMMSPlugin
 * @{
 */

/**
  * Get a transport's private data.
  *
  * @returns Pointer to private data.
  */

gpointer
xmms_transport_private_data_get (xmms_transport_t *transport)
{
	gpointer ret;
	g_return_val_if_fail (transport, NULL);

	ret = transport->plugin_data;

	return ret;
}

/**
 * Set transport private data
 *
 * @param transport the transport to store the pointer in.
 * @param data pointer to private data.
 */

void
xmms_transport_private_data_set (xmms_transport_t *transport, gpointer data)
{
	g_mutex_lock (transport->mutex);
	transport->plugin_data = data;
	g_mutex_unlock (transport->mutex);
}

/**
 * Set transport mimetype.
 * This should be called from the plugin to propagate the mimetype
 *
 * @param transport The transport on which to set the mimetype.
 * @param mimetype A zero-terminated string with the mimetype of the
 * source. It will be duplicated into the transport and free'd when
 * the transport is destroyed.
 */
void
xmms_transport_mimetype_set (xmms_transport_t *transport, const gchar *mimetype)
{
	g_return_if_fail (transport);

	g_mutex_lock (transport->mutex);
	
	if (transport->mimetype)
		g_free (transport->mimetype);

	if (mimetype) {
		transport->mimetype = g_strdup (mimetype);
	} else {
		transport->mimetype = NULL;
	}

	transport->have_mimetype = TRUE;

	g_mutex_unlock (transport->mutex);
	
	if (transport->running)
		xmms_object_emit (XMMS_OBJECT (transport), XMMS_IPC_SIGNAL_TRANSPORT_MIMETYPE, mimetype);

	g_cond_signal (transport->mime_cond);
}

/** 
 * Get the current URL from the transport.
 */
const gchar *
xmms_transport_url_get (const xmms_transport_t *const transport)
{
	const gchar *ret;
	g_return_val_if_fail (transport, NULL);

	g_mutex_lock (transport->mutex);
	ret =  xmms_medialib_entry_property_get_str (transport->entry, 
						     XMMS_MEDIALIB_ENTRY_PROPERTY_URL);
	g_mutex_unlock (transport->mutex);

	return ret;
}

/** 
 * Get the current #xmms_medialib_entry_t from the transport.
 */
xmms_medialib_entry_t
xmms_transport_medialib_entry_get (const xmms_transport_t *const transport)
{
	g_return_val_if_fail (transport, 0);
	return transport->entry;
}

/** @} */

/** 
 * @addtogroup Transport Transport
 *
 * @{
 */

/**
 * Return the #xmms_plugin_t for this transport.
 */

xmms_plugin_t *
xmms_transport_plugin_get (const xmms_transport_t *transport)
{
	g_return_val_if_fail (transport, NULL);

	return transport->plugin;
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
 * This method can be called to check if the current plugin supports
 * seeking. It will check that the plugin has XMMS_PLUGIN_PROPERTY_SEEK
 * is set or not.
 *
 * @returns a gboolean wheter this plugin can do seeking or not.
 */
gboolean
xmms_transport_can_seek (xmms_transport_t *transport)
{
	g_return_val_if_fail (transport, FALSE);

	return xmms_plugin_properties_check (transport->plugin, XMMS_PLUGIN_PROPERTY_SEEK);
}


/**
 * Initialize a new #xmms_transport_t structure. This structure has to
 * be dereffed by #xmms_object_unref to be freed. 
 *
 * To be able to read form this transport you'll have to call
 * #xmms_transport_open after you created a structure with
 * this function.
 *
 * @returns A newly allocated #xmms_transport_t
 */
xmms_transport_t *
xmms_transport_new ()
{
	xmms_transport_t *transport;
	xmms_config_value_t *val;

	val = xmms_config_lookup ("transport.buffersize");

	transport = xmms_object_new (xmms_transport_t, xmms_transport_destroy);
	transport->mutex = g_mutex_new ();
	transport->cond = g_cond_new ();
	transport->mime_cond = g_cond_new ();
	transport->buffer = xmms_ringbuf_new (xmms_config_value_int_get (val));
	transport->buffering = FALSE; /* maybe should be true? */
	transport->total_bytes = 0;
	transport->buffer_underruns = 0;
	transport->current_position = 0; 
	transport->lr.bufend = &transport->lr.buf[0];
	
	return transport;
}

/**
 * Make the transport ready for buffering and reading.
 * It will take the entry URL and pass it to all transport
 * plugins and let them decide if they can handle this URL
 * or not. When the it finds a plugin that claims to handle
 * it, the plugins open method will be called.
 *
 * @returns TRUE if a suitable plugin is found and the plugins
 * open method is successfull, otherwise FALSE.
 */

gboolean
xmms_transport_open (xmms_transport_t *transport, xmms_medialib_entry_t entry)
{
	gchar *tmp;
	g_return_val_if_fail (entry, FALSE);
	g_return_val_if_fail (transport, FALSE);

	tmp = xmms_medialib_entry_property_get_str (entry, 
						    XMMS_MEDIALIB_ENTRY_PROPERTY_URL);
	
	transport->plugin = xmms_transport_plugin_find (tmp);
	g_free (tmp);
	if (!transport->plugin)
		return FALSE;

	transport->entry = entry;
	
	XMMS_DBG ("Found plugin: %s", xmms_plugin_name_get (transport->plugin));

	return xmms_transport_plugin_open (transport, entry, NULL);
}



/**
 * Query the #xmms_transport_t for the current mimetype.
 *
 * @returns a zero-terminated string with the current mimetype.
 */
const gchar *
xmms_transport_mimetype_get (xmms_transport_t *transport)
{
	const gchar *ret;
	g_return_val_if_fail (transport, NULL);

	g_mutex_lock (transport->mutex);
	ret =  transport->mimetype;
	g_mutex_unlock (transport->mutex);

	return ret;
}

/**
 * Like #xmms_transport_mimetype_get but blocks if
 * transport plugin has not yet called mimetype_set
 * This must be called on plugins that is remote. It might
 * take them a while to get the mimetype.
 *
 * @returns a zero-terminated string with the current mimetype.
 */
const gchar *
xmms_transport_mimetype_get_wait (xmms_transport_t *transport)
{
	const gchar *ret;
	g_return_val_if_fail (transport, NULL);

	g_mutex_lock (transport->mutex);
	if (!transport->have_mimetype) {
		g_cond_wait (transport->mime_cond, transport->mutex);
	}
	ret = transport->mimetype;
	g_mutex_unlock (transport->mutex);

	return ret;
}


/**
 * Tell the transport to start buffer, this is normaly done
 * after you read twice from the buffer
 */
void
xmms_transport_buffering_start (xmms_transport_t *transport)
{
	g_mutex_lock (transport->mutex);
	transport->buffering = TRUE;
	g_cond_signal (transport->cond);
	g_mutex_unlock (transport->mutex);
}

/**
 * Read #len bytes into buffer.
 *
 * This function reads from the transport thread buffer, if you want to
 * read more then currently are buffered, it will wait for you. Does not
 * guarantee that all bytes are read, may return less bytes.
 *
 * @param transport transport to read from.
 * @param buffer where to store read data.
 * @param len number of bytes to read.
 * @param error a #xmms_error_t structure that can hold errors.
 * @returns number of bytes actually read, or -1 on error.
 *
 */
gint
xmms_transport_read (xmms_transport_t *transport, gchar *buffer, guint len, xmms_error_t *error)
{
	gint ret;

	g_return_val_if_fail (transport, -1);
	g_return_val_if_fail (buffer, -1);
	g_return_val_if_fail (len > 0, -1);
	g_return_val_if_fail (error, -1);

	g_mutex_lock (transport->mutex);

	/* Unbuffered read */

	if (!transport->buffering) {
		xmms_transport_read_method_t read_method;

		read_method = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_READ);
		if (!read_method) {
			g_mutex_unlock (transport->mutex);
			return -1;
		}

		g_mutex_unlock (transport->mutex);
		ret = read_method (transport, buffer, len, error);
		g_mutex_lock (transport->mutex);

		if (ret <= 0) {
			xmms_error_set (&transport->status, error->code, error->message);
		}

		if (ret != -1) {
			transport->current_position += ret;
			transport->total_bytes += ret;
		}

		if (transport->running && !transport->buffering && transport->numread++ > 1) {
			XMMS_DBG ("Let's start buffering");
			transport->buffering = TRUE;
			g_cond_signal (transport->cond);
			xmms_ringbuf_clear (transport->buffer);
			xmms_ringbuf_set_eos (transport->buffer, FALSE);
		}


		g_mutex_unlock (transport->mutex);
		return ret;
	}

	/* Buffered read */

	if (len > xmms_ringbuf_size (transport->buffer)) {
		len = xmms_ringbuf_size (transport->buffer);;
	}

	if (xmms_ringbuf_iseos (transport->buffer)) {
		gint val = -1;

		xmms_error_set (error, transport->status.code, transport->status.message);
		if (transport->status.code == XMMS_ERROR_EOS) {
			val = 0;
		}

		g_mutex_unlock (transport->mutex);
		return val;
	}

	ret = xmms_ringbuf_read_wait (transport->buffer, buffer, len, transport->mutex);

	if (ret < len) {
		transport->buffer_underruns ++;
	}

	transport->total_bytes += ret;
	transport->current_position += ret; 

	g_mutex_unlock (transport->mutex);

	return ret;
}


/**
 * Read line.
 *
 * Reads one line from the transport. The length of the line can be up
 * to XMMS_TRANSPORT_MAX_LINE_SIZE bytes. Should not be mixed with
 * calls to xmms_transport_read.
 *
 * @param transport transport to read from.
 * @param line buffer to store the line in,
 *             must be atleast XMMS_TRANSPORT_MAX_LINE_SIZE bytes.
 * @param error a #xmms_error_t structure that can hold errors.
 * @returns the line or NULL on EOF or error.
 */
gchar *
xmms_transport_read_line (xmms_transport_t *transport, gchar *line, xmms_error_t *err)
{
	gchar *p;
	
	g_return_val_if_fail (transport, NULL);

	p = strchr (transport->lr.buf, '\n');
	
	if (!p) {
		gint l, r;

		l = (XMMS_TRANSPORT_MAX_LINE_SIZE - 1) - (transport->lr.bufend - transport->lr.buf);
		if (l) {
			r = xmms_transport_read (transport, transport->lr.bufend, l, err);
			if (r < 0) {
				return NULL;
			}
			transport->lr.bufend += r;
		}
		if (transport->lr.bufend == transport->lr.buf)
			return NULL;

		*(transport->lr.bufend) = '\0';

		p = strchr (transport->lr.buf, '\n');
		if (!p) {
			p = transport->lr.bufend;
		}
	}
	if (p > transport->lr.buf && *(p-1) == '\r') {
		*(p-1) = '\0';
	} else {
		*p = '\0';
	}

	strcpy (line, transport->lr.buf);
	memmove (transport->lr.buf, p + 1, transport->lr.bufend - p);
	transport->lr.bufend -= (p - transport->lr.buf) + 1;
	*transport->lr.bufend = '\0';
	return line;

}

/**
 * 
 * Seek to a specific offset in a transport. Emulates the behaviour of
 * lseek. Buffering is disabled after a seek (automatically enabled
 * after two reads).
 *
 * The whence parameter should be one of:
 * @li @c XMMS_TRANSPORT_SEEK_SET Sets position to offset from start of file
 * @li @c XMMS_TRANSPORT_SEEK_END Sets position to offset from end of file
 * @li @c XMMS_TRANSPORT_SEEK_CUR Sets position to offset from current position
 *
 * @param transport the transport to modify
 * @param offset offset in bytes
 * @param whence se above
 * @returns new position, or -1 on error
 * 
 */
gint
xmms_transport_seek (xmms_transport_t *transport, gint offset, gint whence)
{
	xmms_transport_seek_method_t seek_method;
	gboolean ret;

	g_return_val_if_fail (transport, FALSE);

	g_mutex_lock (transport->mutex);

	if (!xmms_plugin_properties_check (transport->plugin, XMMS_PLUGIN_PROPERTY_SEEK)) {
		g_mutex_unlock (transport->mutex);
		return -1;
	}
	
	seek_method = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_SEEK);
	if (!seek_method) {
		xmms_log_error ("This plugin has XMMS_PLUGIN_PROPERTY_SEEK but no seek method, that's stupid");
		g_mutex_unlock (transport->mutex);
		return -1;
	}

	/* reset the buffer */
	transport->buffering = FALSE;
	transport->numread = 0;
	xmms_ringbuf_clear (transport->buffer);
	xmms_ringbuf_set_eos (transport->buffer, FALSE);

	if (whence == XMMS_TRANSPORT_SEEK_CUR) {
		whence = XMMS_TRANSPORT_SEEK_SET;
		offset = transport->current_position + offset;
	}

	ret = seek_method (transport, offset, whence);

	if (ret != -1)
		transport->current_position = ret; 

	g_mutex_unlock (transport->mutex);

	return ret;
}

/**
 * Obtain the current value of the stream position indicator for transport
 * 
 * @returns current position in bytes.
 */
guint64
xmms_transport_tell (xmms_transport_t *transport)
{
	g_return_val_if_fail (transport, -1); 

	return transport->current_position; 
}

/**
 * Query the transport to check wheter it's EOFed or
 * not.
 * @returns TRUE if the stream is EOFed.
 */
gboolean
xmms_transport_iseos (xmms_transport_t *transport)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (transport, FALSE);

	g_mutex_lock (transport->mutex);

	if (transport->buffering) {
		ret = xmms_ringbuf_iseos (transport->buffer);
	} else if (xmms_error_iserror (&transport->status)) {
		ret = TRUE;
	}

	g_mutex_unlock (transport->mutex);
	return ret;
}

/**
 * Get the total size in bytes of the transports source.
 * @returns size of the media, or -1 if it can't be determined.
 */
guint64
xmms_transport_size (xmms_transport_t *transport)
{
	xmms_transport_size_method_t size;
	g_return_val_if_fail (transport, -1);

	size = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_SIZE);
	g_return_val_if_fail (size, -1);

	return size (transport);
}

/**
 * Get the plugin that was used to instantiate this transport.
 */
xmms_plugin_t *
xmms_transport_get_plugin (const xmms_transport_t *transport)
{
	g_return_val_if_fail (transport, NULL);

	return transport->plugin;
}

/**
 * Open the transport plugin. This is called by #xmms_transport_plugin_find
 * which is called by #xmms_transport_open.
 *
 * @return TRUE if the operation was a success.
 */
gboolean
xmms_transport_plugin_open (xmms_transport_t *transport, xmms_medialib_entry_t entry, 
		gpointer data)
{
	xmms_transport_open_method_t init_method;
	xmms_transport_lmod_method_t lmod_method;
	xmms_plugin_t *plugin;
	gchar *url;
	
	plugin = transport->plugin;
	
	init_method = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_INIT);
	lmod_method = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_LMOD);

	if (!init_method) {
		xmms_log_error ("Transport has no init method!");
		return FALSE;
	}

	xmms_transport_private_data_set (transport, data);

	url = xmms_medialib_entry_property_get_str (transport->entry, 
						    XMMS_MEDIALIB_ENTRY_PROPERTY_URL);

	if (!init_method (transport, url)) {
		g_free (url);
		return FALSE;
	}

	g_free (url);

	if (lmod_method) {
		guint lmod;
		lmod = lmod_method (transport);
		xmms_medialib_entry_property_set_int (transport->entry, XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD, lmod);
	}


	return TRUE;
}

/** @} */

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
	xmms_object_ref (transport);
	transport->thread = g_thread_create (xmms_transport_thread, transport, TRUE, NULL); 
}


/**
  * Tell the transport to stop buffering.
  * You still have to deref the object to free memory.
  *
  * @internal
  */

void
xmms_transport_stop (xmms_transport_t *transport)
{
	g_return_if_fail (transport);

	if (transport->thread) {
		g_mutex_lock (transport->mutex);
		transport->running = FALSE;
		xmms_ringbuf_set_eos (transport->buffer, TRUE);
		g_cond_signal (transport->cond);
		g_mutex_unlock (transport->mutex);
		g_thread_join (transport->thread);
		transport->thread = NULL;
	}
}

/*
 * Static functions
 */

/**
 * Destroy function. Called when all references to the 
 * #xmms_transport_t is gone. Will free up memory and 
 * close sockets.
 */
static void
xmms_transport_destroy (xmms_object_t *object)
{
	xmms_transport_t *transport = (xmms_transport_t *)object;
	xmms_transport_close_method_t close_method;

	close_method = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_CLOSE);
	
	if (close_method)
		close_method (transport);

	xmms_object_unref (transport->plugin);

	xmms_ringbuf_destroy (transport->buffer);
	g_cond_free (transport->mime_cond);
	g_cond_free (transport->cond);
	g_mutex_free (transport->mutex);
	
	if (transport->mimetype)
		g_free (transport->mimetype);
}

/**
 * Find a transportplugin for this URL.
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
		XMMS_DBG ("Trying plugin: %s", xmms_plugin_shortname_get (plugin));
		can_handle = xmms_plugin_method_get (plugin, XMMS_PLUGIN_METHOD_CAN_HANDLE);
		
		if (!can_handle)
			continue;

		if (can_handle (url)) {
			xmms_object_ref (plugin);
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
	xmms_error_t error;
	xmms_transport_read_method_t read_method;
	gint ret;

	g_return_val_if_fail (transport, NULL);
	
	read_method = xmms_plugin_method_get (transport->plugin, XMMS_PLUGIN_METHOD_READ);
	if (!read_method)
		return NULL;

	xmms_error_reset (&error);

	g_mutex_lock (transport->mutex);
	while (transport->running) {

		if (!transport->buffering) {
			g_cond_wait (transport->cond, transport->mutex);
		}

		if (!transport->buffering)
			continue;

		g_mutex_unlock (transport->mutex);
		ret = read_method (transport, buffer, sizeof(buffer), &error);
		g_mutex_lock (transport->mutex);

		if (!transport->running)
			break;

		if (ret > 0) {
			xmms_ringbuf_write_wait (transport->buffer, buffer, ret, transport->mutex);
		} else {
			xmms_ringbuf_set_eos (transport->buffer, TRUE);
			xmms_error_set (&transport->status, error.code, error.message);
			g_cond_wait (transport->cond, transport->mutex);
		}
	}
	g_mutex_unlock (transport->mutex);

	XMMS_DBG ("xmms_transport_thread: cleaning up");
	
	xmms_object_unref (transport);
	
	return NULL;
}
