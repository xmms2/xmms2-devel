
#include <xmms/xmms_log.h>
#include <xmmspriv/xmms_medialib.h>
#include <xmmspriv/xmms_ringbuf.h>
#include <xmmspriv/xmms_xform.h>

/*
   - producer:
     want_buffer -> buffering
     want_seek -> seeked
     want_stop -> stopped

   - consumer:
     * -> want_seek
          wait until state == seek_done
	  seek_done -> want_buffer

     * -> want_stop
          wait until state == is_stopped

*/
typedef enum xmms_buffer_state_E {
	STATE_WANT_BUFFER,
	STATE_BUFFERING,
	STATE_WANT_SEEK,
	STATE_SEEK_DONE,
	STATE_WANT_STOP,
	STATE_IS_STOPPED
} xmms_buffer_state_t;

typedef struct xmms_ringbuf_priv_St {
	GThread *thread;

	xmms_ringbuf_t *buffer;
	GMutex buffer_lock;

	xmms_buffer_state_t state;
	GCond state_cond;
	GMutex state_lock;
} xmms_ringbuf_priv_t;

static xmms_xform_plugin_t *ringbuf_plugin;

static gpointer xmms_ringbuf_xform_thread (gpointer data);

static gboolean
xmms_ringbuf_plugin_init (xmms_xform_t *xform)
{
	xmms_ringbuf_priv_t *priv;

	priv = g_new0 (xmms_ringbuf_priv_t, 1);

	xmms_xform_private_data_set (xform, priv);

	g_cond_init (&priv->state_cond);
	g_mutex_init (&priv->state_lock);
	g_mutex_init (&priv->buffer_lock);

	priv->state = STATE_WANT_BUFFER;
	priv->buffer = xmms_ringbuf_new (4096*8);
	priv->thread = g_thread_new ("x2 ringbuf", xmms_ringbuf_xform_thread, xform);

	xmms_xform_outdata_type_copy (xform);

	return TRUE;
}

static void
xmms_ringbuf_plugin_destroy (xmms_xform_t *xform)
{
	xmms_ringbuf_priv_t *priv;
	priv = xmms_xform_private_data_get (xform);

	g_mutex_lock (&priv->state_lock);
	xmms_ringbuf_clear (priv->buffer);
	while (priv->state != STATE_IS_STOPPED) {
		priv->state = STATE_WANT_STOP;
		g_cond_wait (&priv->state_cond, &priv->state_lock);
	}
	g_mutex_unlock (&priv->state_lock);

	g_thread_join (priv->thread);

	XMMS_DBG ("Ringbuf destroyed!");
}

static gint
xmms_ringbuf_plugin_read (xmms_xform_t *xform, void *buffer, gint len, xmms_error_t *error)
{
	xmms_ringbuf_priv_t *priv;
	priv = xmms_xform_private_data_get (xform);

	return xmms_ringbuf_read_wait (priv->buffer, buffer, len, &priv->buffer_lock);
}


static gboolean
xmms_ringbuf_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_ringbuf_plugin_init;
	methods.destroy = xmms_ringbuf_plugin_destroy;
	methods.read = xmms_ringbuf_plugin_read;
	/*
	  methods.seek
	*/

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE, "audio/pcm",
	                              XMMS_STREAM_TYPE_END);

	ringbuf_plugin = xform_plugin;

	return TRUE;
}

static void
fill (xmms_xform_t *xform, xmms_ringbuf_priv_t *priv)
{
	xmms_error_t err;
	char buf[4096];
	int res;

	res = xmms_xform_read (xform, buf, sizeof (buf), &err);
	if (res > 0) {
		xmms_ringbuf_write_wait (priv->buffer, buf, res, &priv->buffer_lock);
	} else if (res == -1) {
		/* XXX copy error */
		g_mutex_lock (&priv->state_lock);
		priv->state = STATE_WANT_STOP;
	} else {
		xmms_ringbuf_set_eos (priv->buffer, TRUE);
		priv->state = STATE_WANT_STOP;
	}
}

static gpointer
xmms_ringbuf_xform_thread (gpointer data)
{
	xmms_xform_t *xform = (xmms_xform_t *)data;
	xmms_ringbuf_priv_t *priv;

	priv = xmms_xform_private_data_get (xform);

	g_mutex_lock (&priv->state_lock);
	while (priv->state != STATE_WANT_STOP) {
		if (priv->state == STATE_WANT_BUFFER) {
			priv->state = STATE_BUFFERING;
			g_cond_signal (&priv->state_cond);
			while (priv->state == STATE_BUFFERING) {
				g_mutex_unlock (&priv->state_lock);
				fill (xform, priv);
				g_mutex_lock (&priv->state_lock);
			}
		} else if (priv->state == STATE_WANT_SEEK) {
			/** **/
			priv->state = STATE_SEEK_DONE;
			g_cond_signal (&priv->state_cond);
			while (priv->state == STATE_SEEK_DONE) {
				g_cond_wait (&priv->state_cond, &priv->state_lock);
			}
		}
		XMMS_DBG ("thread: state: %d", priv->state);
	}
	priv->state = STATE_IS_STOPPED;
	g_cond_signal (&priv->state_cond);
	g_mutex_unlock (&priv->state_lock);

	return NULL;
}

XMMS_XFORM_BUILTIN (ringbuf,
                    "Ringbuffer",
                    XMMS_VERSION,
                    "Buffer",
                    xmms_ringbuf_plugin_setup);
