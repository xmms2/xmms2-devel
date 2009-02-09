/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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




#ifndef _XMMS_OUTPUTPLUGIN_H_
#define _XMMS_OUTPUTPLUGIN_H_

#include <glib.h>
#include <string.h> /* for memset() */

#include "xmmsc/xmmsc_idnumbers.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_plugin.h"
#include "xmms/xmms_error.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_streamtype.h"


G_BEGIN_DECLS

/**
 * @defgroup OutputPlugin OutputPlugin
 * @ingroup XMMSPlugin
 * @{
 */


typedef struct xmms_output_St xmms_output_t;

/**
 * The current API version.
 */
#define XMMS_OUTPUT_API_VERSION 8

struct xmms_output_plugin_St;
typedef struct xmms_output_plugin_St xmms_output_plugin_t;

/**
 * Output functions that lets XMMS2 talk to the soundcard.
 * An output plugin can behave in two diffrent ways. It can either use
 * it's own event system, or it can depend on the one XMMS2 provides.
 * If the architechture uses its own event mechanism the plugin should
 * not implement open/close/write. Instead a status function is implemented
 * which will be notified on playback status updates, and perform the
 * proper actions based on this.
 */
typedef struct xmms_output_methods_St {
	/**
	 * Initiate the output plugin.
	 *
	 * This function should setup everything that is required to function
	 * once the output device is opened for playback. This may for example
	 * include probing for usable sound formats (#xmms_output_format_add)
	 * and setting up the mixer. Blocking calls in this function are to be
	 * avoided as far as possible as this would freeze the startup of xmms2d.
	 *
	 * @param output an output object
	 * @return TRUE on successful init, otherwise FALSE
	 */
	gboolean (*new)(xmms_output_t *output);

	/**
	 * Destroy the output plugin.
	 *
	 * Tear down the data initialized in new.
	 *
	 * @param output an output object
	 */
	void (*destroy)(xmms_output_t *output);

	/**
	 * Open the output device.
	 *
	 * Blocking calls in this function are to be avoided as far as
	 * possible as this would freeze xmms2d. This function cannot coexist
	 * with #status.
	 *
	 * @param output an output object
	 * @return TRUE on successful opening of the output device, otherwise FALSE
	 */
	gboolean (*open)(xmms_output_t *output);

	/**
	 * Close the output device.
	 *
	 * This function cannot coexist with #status.
	 *
	 * @param output an output object
	 */
	void (*close)(xmms_output_t *output);

	/**
	 * Flush the soundcard buffer.
	 *
	 * This should tell the soundcard to drop whatever it is doing and
	 * empty the buffer.
	 *
	 * @param output an output object
	 */
	void (*flush)(xmms_output_t *output);

	/**
	 * Update the sample format.
	 *
	 * This should tell the soundcard what sample format that will be used
	 * for the next track. This is only called when there is an actual
	 * change in the sample format. This function cannot coexist with
	 * #format_set_always.
	 *
	 * @param output an output object
	 * @param type the stream type to use
	 * @return TRUE if the format was successfully set.
	 */
	gboolean (*format_set)(xmms_output_t *output,
	                       const xmms_stream_type_t *type);

	/**
	 * Update the sample format.
	 *
	 * This should tell the soundcard what sample format that will be used
	 * for the next track. This is called each time a track changes even
	 * if the sample format is identical to the previous one. This function
	 * cannot coexist with #format_set.
	 *
	 * @param output an output object
	 * @param type the stream type to use
	 * @return TRUE if the format was successfully set.
	 */
	gboolean (*format_set_always)(xmms_output_t *output,
	                              const xmms_stream_type_t *type);

	/**
	 * Update the output plugin with the current playback status.
	 *
	 * This function is used when the output architecture is driven by an
	 * external thread. When status is set to XMMS_PLAYBACK_STATUS_PLAY,
	 * the external thread should be activated, and will then get its data
	 * from #xmms_output_read, and render it to the soundcard buffer.
	 * This function cannot coexist with #open, #close or #write.
	 *
	 * @param output an output object
	 * @param status the new playback status
	 */
	gboolean (*status)(xmms_output_t *output, xmms_playback_status_t status);

	/**
	 * Set volume.
	 *
	 * @param output an output object
	 * @param chan the name of the channel to set volume on
	 * @param val the volume level to set
	 * @return TRUE if the update was successful, else FALSE
	 */
	gboolean (*volume_set)(xmms_output_t *output, const gchar *chan, guint val);

	/**
	 * Get volume.
	 *
	 * This function is typically called twice. The first run NULL will be
	 * passed to parameters names and levels, and the output plugin will then
	 * set the number of available channels to nchans and return TRUE. When
	 * the channels are known memory will be allocated for the channel names
	 * and volume level lists and the function will be called again, and this
	 * time the volume levels are extracted for real.
	 *
	 * @param output an output object
	 * @param names a pointer to a list that is to be filled with channel names
	 * @param levels a pointer to a list that is to be filled with volume levels
	 * @param nchans a pointer to a list that is to be filled with the nbr of chns
	 * @return TRUE if the volume/chn count successfully retrieved, else FALSE
	 */
	gboolean (*volume_get)(xmms_output_t *output, const gchar **names,
	                       guint *levels, guint *nchans);

	/**
	 * Write audio data to the output device.
	 *
	 * This function is called from a separate thread and should block until
	 * the input buffer has been written to the soundcard. This function cannot
	 * coexist with #status.
	 *
	 * @param output an output object
	 * @param buffer a buffer with audio data to write to the soundcard
	 * @param size the number of bytes in the buffer
	 * @param err an error struct
	 */
	void (*write)(xmms_output_t *output, gpointer buffer,
	              gint size, xmms_error_t *err);

	/**
	 * Get the number of bytes in the soundcard buffer.
	 *
	 * This is needed for the visualization to perform correct synchronization
	 * between audio and graphics for example.
	 *
	 * @param output an output object
	 * @return the number of bytes in the soundcard buffer or 0 on failure
	 */
	guint (*latency_get)(xmms_output_t *);
} xmms_output_methods_t;

/**
 * Register the output plugin.
 *
 * @param shname short name of the plugin
 * @param name long name of the plugin
 * @param ver the version of the plugin, usually the XMMS_VERSION macro
 * @param desc a description of the plugin
 * @param setupfunc the function that sets up the plugin functions
 */
#define XMMS_OUTPUT_PLUGIN(shname, name, ver, desc, setupfunc) XMMS_PLUGIN(XMMS_PLUGIN_TYPE_OUTPUT, XMMS_OUTPUT_API_VERSION, shname, name, ver, desc, (gboolean (*)(gpointer))setupfunc)

/**
 * Initialize the #xmms_output_methods_t struct.
 *
 * This should be run before any functions are associated.
 *
 * @param m the #xmms_output_methods_t struct to initialize
 */
#define XMMS_OUTPUT_METHODS_INIT(m) memset (&m, 0, sizeof (xmms_output_methods_t))


/**
 * Register the output plugin functions.
 *
 * Performs basic validation, see #xmms_output_methods_St for more information.
 *
 * @param output an output plugin object
 * @param methods a struct pointing to the plugin specific functions
 */
void xmms_output_plugin_methods_set (xmms_output_plugin_t *output, xmms_output_methods_t *methods);


/**
 * Retrieve the private data for the plugin that was set with
 * #xmms_output_private_data_set.
 *
 * @param output an output object
 * @return the private data
 */
gpointer xmms_output_private_data_get (xmms_output_t *output);

/**
 * Set the private data for the plugin that can be retrived
 * with #xmms_output_private_data_get later.
 *
 * @param output an output object
 * @param data the private data
 */
void xmms_output_private_data_set (xmms_output_t *output, gpointer data);

/**
 * Add a format that the output plugin can feed the soundcard with.
 *
 * @param output an output object
 * @param fmt a #xmms_sample_format_t
 * @param ch the number of channels
 * @param rate the sample rate
 */
#define xmms_output_format_add(output, fmt, ch, rate)			\
        xmms_output_stream_type_add (output,                            \
                                     XMMS_STREAM_TYPE_MIMETYPE,         \
                                     "audio/pcm",                       \
                                     XMMS_STREAM_TYPE_FMT_FORMAT,       \
                                     fmt,                               \
                                     XMMS_STREAM_TYPE_FMT_CHANNELS,     \
                                     ch,                                \
                                     XMMS_STREAM_TYPE_FMT_SAMPLERATE,   \
                                     rate,                              \
                                     XMMS_STREAM_TYPE_END)


/**
 * Add format to list of supported formats.
 * Should be called from initialisation function for every supported
 * format. Any call to the format_set function will be with one of these
 * formats.
 *
 * @param output an output object
 * @param ... pairs of #xmms_stream_type_key_t, value
 */
void xmms_output_stream_type_add (xmms_output_t *output, ...);

/**
 * Read a number of bytes of data from the output buffer into a buffer.
 *
 * This is typically used when the output plugin is event driven, and is
 * then used when the status is set to playing, and the output needs more
 * data from xmms2 to write to the soundcard.
 *
 * @param output an output object
 * @param buffer a buffer to store the read data in
 * @param len the number of bytes to read
 * @return the number of bytes read
 */
gint xmms_output_read (xmms_output_t *output, char *buffer, gint len);

/**
 * Set an error.
 *
 * When an error occurs in an asynchronous function, the error can be
 * propagated using this function.
 *
 * @param output an output object
 * @param error an error object
 */
void xmms_output_set_error (xmms_output_t *output, xmms_error_t *error);

/**
 * Get the current medialib id.
 *
 * @param output an output object
 * @param error an error object
 * @return the current medialib id
 */
gint xmms_output_current_id (xmms_output_t *output, xmms_error_t *error);

/**
 * Check if an output plugin needs format updates on each track change.
 *
 * @param plugin an output plugin object
 * @return TRUE if the plugin should always be notified, otherwise FALSE
 */
gboolean xmms_output_plugin_format_set_always (xmms_output_plugin_t *plugin);

/**
 * Register a configuration directive in the plugin setup function.
 *
 * As an optional, but recomended functionality the plugin can decide to
 * subscribe on the configuration value and will thus be notified when it
 * changes by passing a callback, and if needed, userdata.
 *
 * @param plugin an output plugin object
 * @param name the name of the configuration directive
 * @param default_value the default value of the configuration directive
 * @param cb the function to call on configuration value changes
 * @param userdata a user specified variable to be passed to the callback
 * @return a #xmms_config_property_t based on the given input
 */
xmms_config_property_t *xmms_output_plugin_config_property_register (xmms_output_plugin_t *plugin, const gchar *name, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);

/**
 * Register a configuration directive.
 *
 * As an optional, but recomended functionality the plugin can decide to
 * subscribe on the configuration value and will thus be notified when it
 * changes by passing a callback, and if needed, userdata.
 *
 * @param output an output object
 * @param name the name of the configuration directive
 * @param default_value the default value of the configuration directive
 * @param cb the function to call on configuration value changes
 * @param userdata a user specified variable to be passed to the callback
 * @return a #xmms_config_property_t based on the given input
 */

xmms_config_property_t *xmms_output_config_property_register (xmms_output_t *output, const gchar *name, const gchar *default_value, xmms_object_handler_t cb, gpointer userdata);

/**
 * Lookup a configuration directive for the output plugin.
 *
 * @param output an output object
 * @param path the path to the configuration value
 * @return a #xmms_config_property_t found at the given path
 */
xmms_config_property_t *xmms_output_config_lookup (xmms_output_t *output, const gchar *path);

/** @} */

G_END_DECLS

#endif
