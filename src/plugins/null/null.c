/** @file null.c 
 *  Null output where no soundcard available.
 *  Known to work with kernel 2.6.
 * 
 *  Copyright (C) 2003  Daniel Svensson, <nano@nittioonio.nu>
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
 *
 *  @todo Proper error handling and less buskis code for mixer.
 *  @todo Handle config stuff nice.
 *  @todo ungay xmms_null_buffersize_get
 */

#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/xmms.h"
#include "xmms/object.h"
#include "xmms/ringbuf.h"
#include "xmms/signal_xmms.h"

#include <glib.h>


/*
 *  Defines
 */


/*
 * Function prototypes
 */
static gboolean xmms_null_new (xmms_output_t *output);
static gboolean xmms_null_open (xmms_output_t *output);
static void xmms_null_close (xmms_output_t *output);
static guint xmms_null_samplerate_set (xmms_output_t *output, guint rate);
static void xmms_null_flush (xmms_output_t *output);
static void xmms_null_write (xmms_output_t *output, gchar *buffer, gint len);


/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "null",
			"NULL Output" XMMS_VERSION,
			"NULL output plugin");

	xmms_plugin_info_add (plugin, "URL", "http://www.nittionio.nu/");
	xmms_plugin_info_add (plugin, "Author", "Daniel Svensson");
	xmms_plugin_info_add (plugin, "E-Mail", "nano@nittionino.nu");


	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_WRITE, 
							xmms_null_write);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_OPEN, 
							xmms_null_open);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_NEW, 
							xmms_null_new);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_CLOSE, 
							xmms_null_close);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_FLUSH, 
							xmms_null_flush);
	xmms_plugin_method_add (plugin, XMMS_PLUGIN_METHOD_SAMPLERATE_SET, 
							xmms_null_samplerate_set);


	return (plugin);
}

/*
 * Member functions
 */


/**
 * Do nothing. ;)
 *
 * @param output The output structure
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_null_new (xmms_output_t *output) 
{
	return TRUE; 
}


/** 
 * Do nothing. ;)
 *
 * @param output The outputstructure we are supposed to fill in with our 
 *               private alsa-blessed data
 *
 * @return TRUE on success, FALSE on error
 */
static gboolean
xmms_null_open (xmms_output_t *output)
{
	return TRUE;
}



/**
 * Do nothing. ;)
 *
 * @param output The output structure filled with alsa data.  
 */
static void
xmms_null_close (xmms_output_t *output)
{
	return;
}



/**
 * Do nothing. ;)
 *
 * @param output The output structure.
 * @param rate The to-be-set sample rate.
 *
 * @return the new sample rate or 0 on error
 */
static guint
xmms_null_samplerate_set (xmms_output_t *output, guint rate)
{
	return rate;
}



/**
 * Do nothing. ;)
 *
 * @param output The output structure
 */
static void
xmms_null_flush (xmms_output_t *output) 
{
	return;
}




/**
 * Do nothing. ;) 
 *
 * @param output The output structure filled with alsa data.
 * @param buffer Audio data to be written to audio device.
 * @param len The length of audio data.
 */
static void
xmms_null_write (xmms_output_t *output, gchar *buffer, gint len)
{
	return;
}
