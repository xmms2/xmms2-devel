/** @file
 * Extremly simple example of a visualisation for xmms2
 * (should probably just be put in /usr/share/doc/xmms2-dev/examples)
 */


#include <SDL/SDL.h>
#include <glib.h>
#include <math.h>

#include "xmmsclient.h"

/** @todo these should be pulled in from an include-file */
#define FFT_BITS 10
#define FFT_LEN (1<<FFT_BITS)

static GMainLoop *mainloop;

static GTimer *timer = NULL;
static guint32 basetime = 0;

static GTrashStack *free_buffers=NULL;
static GList *queue = NULL;
static GList *time_queue = NULL;

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
draw_bar(SDL_Surface *bar, int val,int xpos)
{
	int y,x;
	guint32 *lfb;

	SDL_LockSurface (bar);

	lfb = bar->pixels;

	for (y=256-val; y<256; y++) {
		guint32 col = ((255-y) << 16) | ( (y) << 8 );

		for (x=0; x<30; x++) {
			lfb[512*y + xpos + x] = col;
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
			g_main_loop_quit (mainloop);
			return FALSE;
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

	for (i=0; i<FFT_LEN/32/2; i++) {
		float sum=0.0f;
		int j;
		for(j=0;j<32;j++){
			sum+=spec[i*16+j];
		}
		if(sum!=0){
			sum=log (sum/32);
		}

		draw_bar (surf, MIN (255, sum*64), i*32);
	}

	SDL_UpdateRect (surf, 0, 0, 0, 0);

	g_trash_stack_push (&free_buffers, spec);

	return TRUE;
}

static void
handle_playtime (void *userdata, void *arg)
{
	guint tme = GPOINTER_TO_UINT (arg);
	g_timer_start (timer); /* restart timer */
	basetime = tme;
}

static void
new_data (void *userdata, void *arg) 
{
	gdouble *s = arg;
	guint32 time;
	int i;
	time=s[0];
	float *spec;

	if (free_buffers) {
		spec = g_trash_stack_pop (&free_buffers);
	} else {
		spec = g_malloc(FFT_LEN/2*sizeof(float));
	}

	for (i=0; i<FFT_LEN/2; i++) {
		spec[i]=s[i+1];
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
	xmmsc_connection_t *c;
	SDL_Surface *screen;

	c = xmmsc_init ();

	if(!c){
		printf ("bad\n");
		return 1;
	}

	if (!xmmsc_connect (c)){
		printf ("couldn't connect to xmms2d: %s\n",
			xmmsc_get_last_error(c));
		return 1;
	}

	mainloop = g_main_loop_new (NULL, FALSE);

	timer = g_timer_new ();

	xmmsc_glib_setup_mainloop (c, NULL);

        if (SDL_Init(SDL_INIT_VIDEO) > 0) {
                fprintf(stderr, "Unable to init SDL: %s \n", SDL_GetError());
        }

	screen = SDL_SetVideoMode(512, 300, 32, 0);

	xmmsc_set_callback (c, XMMSC_CALLBACK_VISUALISATION_SPECTRUM,
			    new_data, NULL);
	xmmsc_set_callback (c, XMMSC_CALLBACK_PLAYTIME_CHANGED,
			    handle_playtime, NULL);

	g_timeout_add (20, render_vis, (gpointer)screen);

	g_main_loop_run (mainloop); /* GO GO GO! */

	if (c) {
		xmmsc_deinit (c);
	}

	g_list_foreach (queue, free_queue_entry, NULL);
	g_list_free (queue);
	g_list_free (time_queue);

	while (free_buffers) {
		g_free (g_trash_stack_pop (&free_buffers));
	}

	SDL_Quit ();

	return 0;
}

