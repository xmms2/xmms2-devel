#include "xmms/plugin.h"
#include "xmms/output.h"
#include "xmms/util.h"
#include "xmms/ringbuf.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL/SDL_audio.h>

#include <glib.h>

/*
 * Type definitions
 */

typedef struct xmms_sdlout_data_St {
	xmms_ringbuf_t *buffer;
	GMutex *mutex;
} xmms_sdlout_data_t;

/*
 * Function prototypes
 */

static gboolean xmms_sdlout_open (xmms_output_t *output, const gchar *path);
void xmms_sdlout_write (xmms_output_t *output, gchar *buffer, gint len);
void xmms_sdlout_fill_audio (void *udata, guint8 *stream, gint len);

/*
 * Plugin header
 */

xmms_plugin_t *
xmms_plugin_get (void)
{
	xmms_plugin_t *plugin;

	plugin = xmms_plugin_new (XMMS_PLUGIN_TYPE_OUTPUT, "sdlout",
			"SDL Output " VERSION,
			"Uses SDLlib for output");
	
	xmms_plugin_method_add (plugin, XMMS_METHOD_WRITE, xmms_sdlout_write);
	xmms_plugin_method_add (plugin, XMMS_METHOD_OPEN, xmms_sdlout_open);
	
	return plugin;
}

/*
 * Member functions
 */

static gboolean
xmms_sdlout_open (xmms_output_t *output, const gchar *path)
{
	xmms_sdlout_data_t *data;
	SDL_AudioSpec wanted;

	XMMS_DBG ("xmms_sdlout_open (%p, %s)", output, path);
	
	g_return_val_if_fail (output, FALSE);
	g_return_val_if_fail (path, FALSE);
	
	data = g_new0 (xmms_sdlout_data_t, 1);

	wanted.freq = 44100;
	wanted.format = AUDIO_S16;
	wanted.channels = 2;    /* 1 = mono, 2 = stereo */
	wanted.samples = 1024;  /* Good low-latency value for callback */
	wanted.callback = xmms_sdlout_fill_audio;
	wanted.userdata = data;

	if ( SDL_OpenAudio (&wanted, NULL) < 0 ) {
		XMMS_DBG ("Couldn't open audio: %s", SDL_GetError ());
		g_free (data);
		return FALSE;
	}

	data->buffer = xmms_ringbuf_new (32768);
	data->mutex = g_mutex_new ();

	xmms_output_plugin_data_set (output, data);

	SDL_PauseAudio(0);
	
	return TRUE;
}

void 
xmms_sdlout_fill_audio (void *udata, guint8 *stream, gint len)
{
	xmms_sdlout_data_t *data = (xmms_sdlout_data_t *)udata;
	gchar *buf;

	buf = g_alloca (len);

	g_mutex_lock (data->mutex);
	xmms_ringbuf_wait_used (data->buffer, len, data->mutex);
	xmms_ringbuf_read (data->buffer, buf, len);
	g_mutex_unlock (data->mutex);

	SDL_MixAudio (stream, buf, len, SDL_MIX_MAXVOLUME);
}

void
xmms_sdlout_write (xmms_output_t *output, gchar *buffer, gint len)
{
	xmms_sdlout_data_t *data;
	
	g_return_if_fail (output);
	g_return_if_fail (buffer);
	g_return_if_fail (len > 0);

	data = xmms_output_plugin_data_get (output);
	g_return_if_fail (data);

	g_mutex_lock (data->mutex);
	xmms_ringbuf_wait_free (data->buffer, len, data->mutex);
	xmms_ringbuf_write (data->buffer, buffer, len);
	g_mutex_unlock (data->mutex);
}
