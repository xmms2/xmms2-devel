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
 * Extremly simple example of a visualisation for xmms2
 * (should probably just be put in /usr/share/doc/xmms2-dev/examples)
 */


#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <glib.h>
#include <math.h>
#include <stdlib.h>

#include "xmms/xmmsclient.h"
#include "xmms/xmmsclient-glib.h"
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
	int skipped = 0;
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
		skipped++;
	}

	if (skipped)
		printf("Skipped %d\n", skipped);

	res = queue->data;

	time_queue = g_list_delete_link (time_queue, time_queue);
	queue = g_list_delete_link (queue, queue);

	return res;
}

static void
draw_bar (SDL_Surface *bar, int val, int xpos)
{
	int y,x;
	int ybase;
	guint32 *lfb;

	SDL_LockSurface (bar);

	ybase = (bar->h-256)/2;

	lfb = bar->pixels;

	for (y=256-val; y<256; y++) {
		guint32 col = ((255-y) << 16) | ( (y) << 8 );

		for (x=0; x<30; x++) {
			if (0 < y+ybase && y+ybase < bar->h)
				lfb[bar->pitch/4*(y+ybase) + xpos + x] = col;
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
				xmmsc_playback_next (connection);
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

	for (i=0; i<FFT_LEN/2/32 - 1; i++) {
		float sum = 0.0f;
		int j;
		for (j=0; j<32; j++){
			sum += spec[i*16+j];
		}
		sum /= 32.0;
		if (sum != 0.0) {
			sum = log10 (1.0 + 9.0 * sum);
		}

		draw_bar (surf, 256*sum, i*32 + (surf->w-(FFT_LEN/2))/2);
	}

	SDL_UpdateRect (surf, 0, 0, 0, 0);

	g_trash_stack_push (&free_buffers, spec);

	return TRUE;
}

static void
handle_mediainfo (xmmsc_result_t *res, void *userdata)
{
	x_hash_t *hash;
	gchar *tmp;
	gint mid;
	guint id;
	xmmsc_connection_t *c = userdata;

	if (!xmmsc_result_get_uint (res, &id)) {
		printf ("no id!\n");
		return;
	}

	hash = xmmscs_playlist_get_mediainfo (c, id);

	if (!hash) {
		printf ("no mediainfo!\n");
	} else {
		tmp = x_hash_lookup (hash, "id");

		mid = atoi (tmp);

		if (id == mid) {
			if (x_hash_lookup (hash, "channel") && x_hash_lookup (hash, "title")) {
				xmmsc_entry_format (mediainfo, sizeof (mediainfo),
				                    "[stream] ${title}", hash);
			} else if (x_hash_lookup (hash, "channel")) {
				xmmsc_entry_format (mediainfo, sizeof (mediainfo),
				                    "${channel}", hash);
			} else {
				xmmsc_entry_format (mediainfo, sizeof (mediainfo),
				                    "${artist} - ${title}", hash);
			}
		}
		x_hash_destroy (hash);
	}

}


static void
handle_playtime (xmmsc_result_t *res, void *userdata)
{
	guint tme;
	static guint lasttime = -1;
	SDL_Color white = { 0xFF, 0xFF, 0xFF, 0 };
	xmmsc_result_t *newres;

	if (xmmsc_result_iserror (res)) {
		return;
	}
	
	if (!xmmsc_result_get_uint (res, &tme)) {
		return;
	}

	g_timer_start (timer); /* restart timer */
	basetime = tme;

	newres = xmmsc_result_restart (res);
	xmmsc_result_unref (res);
	xmmsc_result_unref (newres);

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
handle_vis (xmmsc_result_t *res, void *userdata)
{
	guint32 time;
	int i;
	float *spec;
	xmmsc_result_t *newres;
	x_list_t *list, *n;

	if (xmmsc_result_iserror (res)) {
		return;
	}

	if (xmmsc_result_get_uintlist (res, &list)) {

		if (free_buffers) {
			spec = g_trash_stack_pop (&free_buffers);
		} else {
			spec = g_malloc(FFT_LEN/2*sizeof(float));
		}
		
		n = list;
		time = (guint) n->data;
		n = x_list_next (n);
		i = 0;
		for (; n; n = x_list_next (n)) {
			spec[i++] = ((gdouble)((guint32) n->data)) / ((gdouble)(G_MAXUINT32));
		}

		/* @todo measure ipc-delay for real! */
		enqueue (time-300, spec); 
		x_list_free (list);

	}

	newres = xmmsc_result_restart (res);
	xmmsc_result_unref (res);
	xmmsc_result_unref (newres);

}

static void
free_queue_entry (gpointer data, gpointer udata)
{
	g_trash_stack_push (&free_buffers, data);
}


int
main(int argc, char **argv)
{
	SDL_Surface *screen;
	gchar *path;

	connection = xmmsc_init ("XMMS2 SDL VIS");

	if(!connection){
		printf ("bad\n");
		return 1;
	}

	path = getenv ("XMMS_PATH");
	if (!xmmsc_connect (connection, path)){
		printf ("couldn't connect to xmms2d: %s\n",
			xmmsc_get_last_error(connection));
		return 1;
	}

	mainloop = g_main_loop_new (NULL, FALSE);

	timer = g_timer_new ();

	xmmsc_ipc_setup_with_gmain (connection);

        if (SDL_Init(SDL_INIT_VIDEO) > 0) {
                fprintf(stderr, "Unable to init SDL: %s \n", SDL_GetError());
		return 1;
        }

        if ( TTF_Init() < 0 ) {
                fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
		return 1;
        }

	font = TTF_OpenFont("font.ttf", 24);

	if(!font){
		fprintf(stderr, "couldn't open font.ttf\n");
		return 1;
	}

	screen = SDL_SetVideoMode(640, 480, 32, 0);

	//set_mediainfo (connection, xmmsc_get_playing_id (connection));

	XMMS_CALLBACK_SET (connection, xmmsc_signal_playback_playtime, handle_playtime, NULL);
	XMMS_CALLBACK_SET (connection, xmmsc_signal_visualisation_data, handle_vis, NULL);
	XMMS_CALLBACK_SET (connection, xmmsc_broadcast_playback_current_id, handle_mediainfo, connection);
	XMMS_CALLBACK_SET (connection, xmmsc_playback_current_id, handle_mediainfo, connection);

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

/*	TTF_Quit ();*/
	SDL_Quit ();

	return 0;
}
