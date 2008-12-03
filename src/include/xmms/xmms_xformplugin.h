/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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




#ifndef __XMMS_XFORMPLUGIN_H__
#define __XMMS_XFORMPLUGIN_H__

#include <glib.h>
#include <string.h>

/**
 * @defgroup XForm XForm
 * @ingroup XMMSServer
 * @brief XForm API
 *
 * An xform (transform) is something that reads data and applies some
 * kind of transformation to it such as decoding or demuxing or
 * applying an effect.
 *
 * The xform api is designed to allow xforms to be connected in a
 * chain where each xform does a different transformation step. Each
 * xform provides a "read" method, which should return transformed
 * data and when it needs more input data, it should call the read
 * method of the previous xform in the chain.
 *
 * The type of the data flowing from one xform to another is described
 * by an xmms_stream_type_t. So an xform registers which
 * xmms_stream_type_t it wants as input and when initialised it tells
 * what type the output data is. This allows the chain of xforms to
 * easily be built.
 *
 * @{
 */



#define XMMS_XFORM_API_VERSION 7

#include "xmms/xmms_error.h"
#include "xmms/xmms_plugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_streamtype.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_medialib.h"


G_BEGIN_DECLS

struct xmms_xform_plugin_St;
/**
 * Xform plugin.
 */
typedef struct xmms_xform_plugin_St xmms_xform_plugin_t;

/**
 * Declare an xform plugin.
 * Use this macro _ONCE_ for each plugin.
 *
 * @param shname Short name of the plugin, should not contain any
 * special characters, just a-z A-Z 0-9 and _.
 * @param name Full name, display name for plugin.
 * @param ver Version of plugin, as string.
 * @param desc Description of plugin and its uses.
 * @param setupfunc Function to be called when initializing plugin.
 *
 *
 * example:
 * XMMS_XFORM_PLUGIN("example",
 *                   "Example decoder",
 *                   "1.3.37-beta",
 *                   "Decoder for playing example files",
 *                   xmms_example_setup);
 *
 */
#define XMMS_XFORM_PLUGIN(shname, name, ver, desc, setupfunc) XMMS_PLUGIN(XMMS_PLUGIN_TYPE_XFORM, XMMS_XFORM_API_VERSION, shname, name, ver, desc, (gboolean (*)(gpointer))setupfunc)

/* */

struct xmms_xform_St;
/* xform */
typedef struct xmms_xform_St xmms_xform_t;

/**
 * Seek direction argument.
 */
typedef enum xmms_xform_seek_mode_E {
	XMMS_XFORM_SEEK_CUR = 1,
	XMMS_XFORM_SEEK_SET = 2,
	XMMS_XFORM_SEEK_END = 3
} xmms_xform_seek_mode_t;

/**
 * Methods provided by an xform plugin.
 */
typedef struct xmms_xform_methods_St {
	/**
	 * Initialisation method.
	 *
	 * Called when a new xform is to be instantiated.  It should
	 * prepare for the transformation and use
	 * #xmms_xform_outdata_type_add to inform what type of data it
	 * outputs.
	 *
	 * @returns TRUE if initialisation was successful, FALSE otherwise.
	 */
	gboolean (*init)(xmms_xform_t *);
	/**
	 * Destruction method.
	 *
	 * Called when the xform isn't needed anymore. Should free any
	 * resources used by the xform.
	 */
	void (*destroy)(xmms_xform_t *);
	/**
	 * Read method.
	 *
	 * Called to read data from the xform.
	 */
	gint (*read)(xmms_xform_t *, gpointer, gint, xmms_error_t *);
	/**
	 * Seek method.
	 *
	 * Called to change the offset in the stream.  Observe that
	 * the offset is measured in "natural" units; audio/pcm-data
	 * is measured in samples, application/octet-stream in bytes.
	 * 
	 */
	gint64 (*seek)(xmms_xform_t *, gint64, xmms_xform_seek_mode_t, xmms_error_t *);

	/**
	 * browse method.
	 *
	 * Called when a users wants to do some server side browsing.
	 * This is called without init() beeing called.
	 */
	gboolean (*browse)(xmms_xform_t *, const gchar *, xmms_error_t *);
} xmms_xform_methods_t;

#define XMMS_XFORM_METHODS_INIT(m) memset (&m, 0, sizeof (xmms_xform_methods_t))


/**
 *
 * Should be called _once_ from the plugin's setupfunc.
 */
void xmms_xform_plugin_methods_set (xmms_xform_plugin_t *plugin, xmms_xform_methods_t *methods);
/**
 * Add a valid input type to the plugin.
 *
 * The varargs should contain key-value pairs terminated with
 * XMMS_STREAM_TYPE_END.
 *
 * Should be called from the plugin's setupfunc.
 *
 * @param plugin the plugin
 * @param ... variable length arguments, terminated with XMMS_STREAM_TYPE_END
 *
 * example:
 * xmms_xform_plugin_indata_add (plugin,
 *                               XMMS_STREAM_TYPE_MIMETYPE,
 *                               "application/example",
 *                               XMMS_STREAM_TYPE_END);
 */
void xmms_xform_plugin_indata_add (xmms_xform_plugin_t *plugin, ...);

/**
 * Get private data for this xform.
 *
 * @param xform current xform
 * @returns the data set with #xmms_xform_private_data_set
 */
gpointer xmms_xform_private_data_get (xmms_xform_t *xform);
/**
 * Set private data for this xform.
 *
 * Allows keeping information across calls to methods of the
 * xform. Usually set from init method and accessed with
 * #xmms_xform_private_data_get in read, seek and destroy methods.
 *
 * @param xform current xform
 * @param data 
 */
void xmms_xform_private_data_set (xmms_xform_t *xform, gpointer data);


void xmms_xform_outdata_type_add (xmms_xform_t *xform, ...);
void xmms_xform_outdata_type_copy (xmms_xform_t *xform);

/**
 * Set numeric metadata for the media transformed by this xform.
 *
 * @param xform
 * @param key Metadatum key to set. Should preferably be one of the XMMS_MEDIALIB_ENTRY_PROPERTY_* values.
 * @param val
 */
void xmms_xform_metadata_set_int (xmms_xform_t *xform, const gchar *key, int val);
/**
 * Set string metadata for the media transformed by this xform.
 *
 * @param xform
 * @param key Metadatum key to set. Should preferably be one of the XMMS_MEDIALIB_ENTRY_PROPERTY_* values.
 * @param val
 */
void xmms_xform_metadata_set_str (xmms_xform_t *xform, const gchar *key, const char *val);

gboolean xmms_xform_metadata_has_val (xmms_xform_t *xform, const gchar *key);
gboolean xmms_xform_metadata_get_int (xmms_xform_t *xform, const gchar *key,
                                      gint *val);
gboolean xmms_xform_metadata_get_str (xmms_xform_t *xform, const gchar *key,
                                      const gchar **val);
void xmms_xform_auxdata_barrier (xmms_xform_t *xform);
void xmms_xform_auxdata_set_int (xmms_xform_t *xform, const gchar *key, gint32 val);
void xmms_xform_auxdata_set_str (xmms_xform_t *xform, const gchar *key, const gchar *val);
void xmms_xform_auxdata_set_bin (xmms_xform_t *xform, const gchar *key, gpointer data, gssize len);
gboolean xmms_xform_auxdata_has_val (xmms_xform_t *xform, const gchar *key);
gboolean xmms_xform_auxdata_get_int (xmms_xform_t *xform, const gchar *key, gint32 *val);
gboolean xmms_xform_auxdata_get_str (xmms_xform_t *xform, const gchar *key, const gchar **val);
gboolean xmms_xform_auxdata_get_bin (xmms_xform_t *xform, const gchar *key, gpointer *data, gssize *datalen);

const char *xmms_xform_indata_get_str (xmms_xform_t *xform, xmms_stream_type_key_t key);
gint xmms_xform_indata_get_int (xmms_xform_t *xform, xmms_stream_type_key_t key);

/**
 * Preview data from previous xform.
 *
 * Allows an xform to look at its input data without consuming it so
 * that a subsequent call to #xmms_xform_read will get the same
 * data. Up to siz bytes are read into the supplied buffer starting at
 * buf. If siz is less than one xmms_xform_read just returns zero. On
 * error -1 is returned and the error is stored in the supplied
 * #xmms_error_t. On end of stream zero is returned.
 *
 * @param xform
 * @param buf buffer to read data into
 * @param siz size of buffer
 * @param err error container which is filled in if error occours.
 * @returns the number of bytes read or -1 to indicate error and 0 when end of stream.
 */
gint xmms_xform_peek (xmms_xform_t *xform, gpointer buf, gint siz, xmms_error_t *err);

/**
 * Read one line from previous xform.
 *
 * Reads a line from the prev xform into buf.
 *
 * @param xform
 * @param buf buffer to write the line to, should be at least XMMS_XFORM_MAX_LINE_SIZE
 * @param err error container which is filled in if error occours.
 * @returns the line read from the parent or NULL to indicate error.
 */
gchar *xmms_xform_read_line (xmms_xform_t *xform, gchar *buf, xmms_error_t *err);         

/**
 * Read data from previous xform.
 *
 * Reads up to siz bytes into the supplied buffer starting at buf. If
 * siz is less than one xmms_xform_read just returns zero. On error -1
 * is returned and the error is stored in the supplied
 * #xmms_error_t. On end of stream zero is returned.
 *
 * @param xform
 * @param buf buffer to read data into
 * @param siz size of buffer
 * @param err error container which is filled in if error occours.
 * @returns the number of bytes read or -1 to indicate error and 0 when end of stream.
 */
gint xmms_xform_read (xmms_xform_t *xform, gpointer buf, gint siz, xmms_error_t *err);

/**
 * Change offset in stream.
 *
 * Tries to change the offset from which data is read.
 *
 * @param xform
 * @param offset offset to seek to, measured in "natural" units
 * @param whence one of XMMS_XFORM_SEEK_{CUR,END,SET}
 * @param err error container which is filled in if error occours.
 * @returns new offset in stream, or -1 on error.
 *
 */
gint64 xmms_xform_seek (xmms_xform_t *xform, gint64 offset, xmms_xform_seek_mode_t whence, xmms_error_t *err);
gboolean xmms_xform_iseos (xmms_xform_t *xform);

gboolean xmms_magic_add (const gchar *desc, const gchar *mime, ...);
gboolean xmms_magic_extension_add (const gchar *mime, const gchar *ext);

xmms_config_property_t *xmms_xform_plugin_config_property_register (
	xmms_xform_plugin_t *xform_plugin,
	const gchar *name,
	const gchar *default_value,
	xmms_object_handler_t cb,
	gpointer userdata);
xmms_config_property_t *xmms_xform_config_lookup (xmms_xform_t *xform,
                                                  const gchar *path);

/**
 * Get the medialib entry played by this xform.
 *
 * @param xform
 * @returns 
 */
xmms_medialib_entry_t xmms_xform_entry_get (xmms_xform_t *xform);
const gchar *xmms_xform_get_url (xmms_xform_t *xform);

#define XMMS_XFORM_BROWSE_FLAG_DIR (1 << 0)

void xmms_xform_browse_add_entry (xmms_xform_t *xform, const gchar *path, guint32 flags);
void xmms_xform_browse_add_entry_property (xmms_xform_t *xform, const gchar *key, xmmsv_t *val);
void xmms_xform_browse_add_entry_property_str (xmms_xform_t *xform,
                                               const gchar *key,
                                               const gchar *value);
void xmms_xform_browse_add_entry_property_int (xmms_xform_t *xform,
                                               const gchar *key,
                                               gint value);
void xmms_xform_browse_add_symlink (xmms_xform_t *xform, const gchar *basename, const gchar *url);
void xmms_xform_browse_add_symlink_args (xmms_xform_t *xform, const gchar *basename, const gchar *url, gint nargs, char **args);

#define XMMS_XFORM_MAX_LINE_SIZE 1024

/**
 * @}
 */
G_END_DECLS

#endif
