/** @file
 * Extremly simple example of a visualisation for xmms2
 * (should probably just be put in /usr/share/doc/xmms2-dev/examples)
 */


#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <glib.h>
#include <math.h>

#include "xmmsclient.h"
#include "xmms/signal_xmms.h"

/** @todo these should be pulled in from an include-file */
#define FFT_BITS 10
#define FFT_LEN (1<<FFT_BITS)

static GMainLoop *mainloop;

static GTimer *timer = NULL;
static guint32 basetime = 0;

static GTrashStack *free_buffers=NULL;
static GList *queue = NULL;
static GList *time_queue = NULL;

static TTF_Font *font;
static SDL_Surface *text = NULL;
static gchar mediainfo[64];

static xmmsc_connection_t *connection;


/*
 * 
 */

static void
enqueue (guint32 time, float *buf)
{
	queue = g_list_append (queue, buf);
	time_queue = g_list_append (time_queue, GINT_TO_POINTER (time));
}

static float *
dequeue (guint32 time)
{
	float *res;

	if (!queue) {
		return NULL;
	}
	while (time > GPOINTER_TO_INT(time_queue->data)) {
		if (queue->next) {
			g_trash_stack_push (&free_buffers, queue->data);
			queue = g_list_delete_link (queue, queue);
			time_queue = g_list_delete_link (time_queue, time_queue);
		} else {
			break;
		}
	}

	res = queue->data;

	time_queue = g_list_delete_link (time_queue, time_queue);
	queue = g_list_delete_link (queue, queue);

	return res;
}

static void
draw_bar (SDL_Surface *bar, int val, int xpos)
{
	int y,x;
	guint32 *lfb;

	SDL_LockSurface (bar);

	lfb = bar->pixels;

	for (y=256-val; y<256; y++) {
		guint32 col = ((255-y) << 16) | ( (y) << 8 );

		for (x=0; x<30; x++) {
			lfb[bar->pitch/4*y + xpos + x] = col;
		}

	}

	SDL_UnlockSurface (bar);
}


/* called periodically from the mainloop */
static gboolean
render_vis (gpointer data)
{
	SDL_Surface *surf = (SDL_Surface *)data;
	SDL_Event event;
	int i;
	gulong apa;
	float *spec;


	while (SDL_PollEvent (&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
			case SDLK_F1:
				SDL_WM_ToggleFullScreen (surf);
				break;
			case 'n':
				xmmsc_play_next (connection);
				break;
			case 'q':
			case SDLK_ESCAPE:
				g_main_loop_quit (mainloop);
				return FALSE;
				break;
			default:
				;
			}
		}
	}

	if (!basetime) {
		/* wait 'til we have timing info */
		return TRUE;
	}

	spec = dequeue (basetime + (int)(g_timer_elapsed (timer,&apa)*1000.0+0.5));

	if (!spec) {
		return TRUE;
	}

	SDL_FillRect (surf, NULL, 0xff000000);

	if (text) {
                SDL_BlitSurface(text, NULL, surf, NULL);
	}

	for (i=0; i<FFT_LEN/32/2; i++) {
		float sum = 0.0f;
		int j;
		for (j=0; j<32; j++){
			sum += spec[i*16+j];
		}
		if (sum != 0) {
			sum = log (sum/32);
		}

		draw_bar (surf, MIN (255, sum*64), i*32 + (surf->w-(FFT_LEN/2))/2);
	}

	SDL_UpdateRect (surf, 0, 0, 0, 0);

	g_trash_stack_push (&free_buffers, spec);

	return TRUE;
}


static void
set_mediainfo (xmmsc_connection_t *conn, guint id)
{
	GHashTable *entry;

	entry = xmmsc_playlist_get_mediainfo (conn, id);

	if (!entry) {
		fprintf (stderr, "id %d doesn't exist in playlist\n", id);
		return;
	}

	snprintf (mediainfo, 63, "%s - %s",
		  (gchar *)g_hash_table_lookup (entry, "artist"),
		  (gchar *)g_hash_table_lookup (entry, "title"));

}

static void
handle_playtime (void *userdata, void *arg)
{
	guint tme = GPOINTER_TO_UINT (arg);
	SDL_Color white = { 0xFF, 0xFF, 0xFF, 0 };
	static guint lasttime = 0xffffffff;

	g_timer_start (timer); /* restart timer */
	basetime = tme;

	if (tme/1000 != lasttime) {
		gchar buf[64];
		if (text) {
			SDL_FreeSurface (text);
		}

		snprintf (buf, 63, "%02d:%02d : %s",
			  tme/60000, (tme/1000)%60, mediainfo);
		lasttime = tme / 1000;
		text = TTF_RenderUTF8_Blended (font, buf, white);
	}
}

static void
new_data (void *userdata, void *arg) 
{
	gdouble *s = arg;
	guint32 time=s[0];
	int i;
	float *spec;

	if (free_buffers) {
		spec = g_trash_stack_pop (&free_buffers);
	} else {
		spec = g_malloc(FFT_LEN/2*sizeof(float));
	}

	for (i=0; i<FFT_LEN/2; i++) {
		spec[i] = s[i+1];
	}

	enqueue (time-300, spec); /* @todo measure dbus-delay for real! */

}

static void
free_queue_entry (gpointer data, gpointer udata)
{
	g_trash_stack_push (&free_buffers, data);
}


int
main()
{
	SDL_Surface *screen;

	connection = xmmsc_init ();

	if(!connection){
		printf ("bad\n");
		return 1;
	}

	if (!xmmsc_connect (connection)){
		printf ("couldn't connect to xmms2d: %s\n",
			xmmsc_get_last_error(connection));
		return 1;
	}

	mainloop = g_main_loop_new (NULL, FALSE);

	timer = g_timer_new ();

	xmmsc_glib_setup_mainloop (connection, NULL);

        if (SDL_Init(SDL_INIT_VIDEO) > 0) {
                fprintf(stderr, "Unable to init SDL: %s \n", SDL_GetError());
		return 1;
        }

        if ( TTF_Init() < 0 ) {
                fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
		return 1;
        }

	font = TTF_OpenFont("font.ttf", 14);

	if(!font){
		fprintf(stderr, "couldn't open font.ttf\n");
		return 1;
	}

	screen = SDL_SetVideoMode(640, 480, 32, 0);

	set_mediainfo (connection, xmmsc_get_playing_id (connection));

	xmmsc_set_callback (connection, XMMS_SIGNAL_VISUALISATION_SPECTRUM,
			    new_data, NULL);
	xmmsc_set_callback (connection, XMMS_SIGNAL_PLAYBACK_PLAYTIME,
			    handle_playtime, NULL);
	xmmsc_set_callback (connection, XMMS_SIGNAL_PLAYBACK_CURRENTID,
			    set_mediainfo, connection);

	g_timeout_add (20, render_vis, (gpointer)screen);

	g_main_loop_run (mainloop); /* GO GO GO! */

	if (connection) {
		xmmsc_deinit (connection);
	}

	g_list_foreach (queue, free_queue_entry, NULL);
	g_list_free (queue);
	g_list_free (time_queue);

	while (free_buffers) {
		g_free (g_trash_stack_pop (&free_buffers));
	}

	TTF_Quit ();
	SDL_Quit ();

	return 0;
}
