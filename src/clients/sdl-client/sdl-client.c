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
 * An sdl vis client based on anders initial code
 * following keypresses are honoured:
 *
 * s = stop
 * b = play
 * n = next
 * p = prev
 * left arrow = seek - 1s
 * right arrow = seek + 1s
 * up arrow = seek + 5s
 * down arrow = seek - 5s
 * q or esc = quit
 * 
 */

#define DEFAULT_FORMAT "%a - %t (%s:%m)"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <glib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "xmms/xmmsclient.h"
#include "xmms/xmmsclient-glib.h"
#include "xmms/signal_xmms.h"

static xmmsc_connection_t *connection;
static GMainLoop *mainloop;
GTimer *timer = NULL;
static guint curtime = 0;
static TTF_Font *font;
static SDL_Surface *text = NULL;
static int donotupdate = 0;
static gchar *format;

gboolean
render (gpointer data)
{
	xmmsc_result_t *res;

	SDL_Surface *surf = (SDL_Surface *) data;
	SDL_Event event;

	while (SDL_PollEvent (&event)) {
		switch (event.type) {
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
					case 'q':
					case SDLK_ESCAPE:
						g_main_loop_quit (mainloop);
						return FALSE;
						break;
					case 'n':
						res = xmmsc_playlist_set_next (connection, 0, 1);
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						res = xmmsc_playback_next (connection);
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						break;
					case 'p':
						res = xmmsc_playlist_set_next (connection, 0, -1);
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						res = xmmsc_playback_next (connection);
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						break;
					case 's':
						res = xmmsc_playback_stop (connection);
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						break;
					case 'b':
						res = xmmsc_playback_start (connection);
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						break;
					case SDLK_RIGHT:
						res = xmmsc_playback_seek_ms (connection, curtime+1000);
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						break;
					case SDLK_LEFT:
						res = xmmsc_playback_seek_ms (connection, curtime-1000);
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						break;
					case SDLK_UP:
						res = xmmsc_playback_seek_ms (connection, curtime+(5*1000));
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						break;
					case SDLK_DOWN:
						res = xmmsc_playback_seek_ms (connection, curtime-(5*1000));
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						break;
					case SDLK_SPACE:
						if (donotupdate) {
							res = xmmsc_playback_start (connection);
						} else {
							res = xmmsc_playback_pause (connection);
						}
						xmmsc_result_wait (res);
						xmmsc_result_unref (res);
						break;
					default:
						
						;
				}
		}
	}

	if (!curtime) {
		return TRUE;
	}

	/* Reset */
	SDL_FillRect (surf, NULL, 0xff000000);

	if (text) {
		SDL_BlitSurface (text, NULL, surf, NULL);
	}

	SDL_UpdateRect (surf, 0, 0, 0, 0);

	return TRUE;
}

guint id;
x_hash_t *mediainfo;

static void
handle_status (xmmsc_result_t *res, void *udata)
{
	gint status;

	xmmsc_result_get_uint (res, &status);

	if (status == XMMSC_PLAYBACK_STOP || status == XMMSC_PLAYBACK_PAUSE) {
		gchar buf[256];
		SDL_Color red = { 0xFF, 0x00, 0x00, 0 };

		memset (buf, 0, 256);

		if (text) {
			SDL_FreeSurface (text);
		}

		snprintf (buf, 7, "%02d:%02d ", curtime/60000, (curtime/1000)%60);

		if (mediainfo) {
			if (!x_hash_lookup (mediainfo, "title")) {
				xmmsc_entry_format (buf+6, 251, "%f (%m:%s)", mediainfo);
			} else {
				xmmsc_entry_format (buf+6, 251, format, mediainfo);
			}
		}

		text = TTF_RenderUTF8_Blended (font, buf, red);
		donotupdate = 1;
	} else {
		donotupdate = 0;
	}
	
	xmmsc_result_unref (xmmsc_result_restart (res));
	xmmsc_result_unref (res);
	
}

static void
handle_mediainfo (xmmsc_result_t *res, void *udata)
{
	if (!xmmsc_result_get_uint (res, &id)) 
		id = 0;

	mediainfo = xmmscs_playlist_get_mediainfo (connection, id);

	xmmsc_result_unref (xmmsc_result_restart (res));
	xmmsc_result_unref (res);
}

static void
handle_playtime (xmmsc_result_t *res, void *udata)
{
	gint tme;
	static guint lasttime = 0xffffffff;

	xmmsc_result_get_uint (res, &tme);

	g_timer_start (timer);
	curtime = tme;

	if (!donotupdate) {
		if (tme/1000 != lasttime) {
			gchar buf[256];
			SDL_Color white = { 0xFF, 0xFF, 0xFF, 0 };

			memset (buf, 0, 256);

			if (text) {
				SDL_FreeSurface (text);
			}

			snprintf (buf, 7, "%02d:%02d ", tme/60000, (tme/1000)%60);

			if (mediainfo) {
				if (!x_hash_lookup (mediainfo, "title")) {
					xmmsc_entry_format (buf+6, 251, "%f (%m:%s)", mediainfo);
				} else {
					xmmsc_entry_format (buf+6, 251, format, mediainfo);
				}
			}

			text = TTF_RenderUTF8_Blended (font, buf, white);
		}
	}

	xmmsc_result_unref (xmmsc_result_restart (res));
	xmmsc_result_unref (res);

}


int
main (int argc, char **argv)
{
	SDL_Surface *screen;
	gchar *path;
	gchar *dbus_path = NULL;
	gchar *fontfile = NULL;
	int opt;
	int x=0;
	int y=0;
	gchar *color = NULL;
	int fontsize = 14;

	connection = xmmsc_init ();


	while (42) {
		opt = getopt (argc, argv, "hp:f:x:y:c:s:a:");
		if (opt == -1)
			break;

		switch (opt) {
			case 'p':
				dbus_path = g_strdup (optarg);
				break;
			case 'f':
				fontfile = g_strdup (optarg);
				break;
			case 'x':
				x = atoi (optarg);
				break;
			case 'y':
				y = atoi (optarg);
				break;
			case 'c':
				color = g_strdup (optarg);
				break;
			case 's':
				fontsize = atoi (optarg);
				break;
			case 'a':
				format = g_strdup (optarg);
				break;
			case 'h':
				printf ("Simple SDL Client for XMMS2\n\nFollowing keystrokes is honoured:\n"
					"\ts = stop\n"
					"\tb = play\n"
					"\tn = next\n"
					"\tp = prev\n"
					"\tleft arrow = seek - 1s\n"
					"\tright arrow = seek + 1s\n"
					"\tup arrow = seek + 5s\n"
					"\tdown arrow = seek - 5s\n"
					"\tq or esc = quit\n"
					"\tspace = toggles pause/play\n"
					"\nFollowing switches:\n"
					"\t-f<fontfile>\n"
					"\t-y<y-size>\n"
					"\t-x<x-size>\n"
					"\t-s<fontsize>\n"
					"\t-p<dbuspath>\n"
					"\t-a<format> default: %s\n", DEFAULT_FORMAT);

				exit (0);
				break;
		}
	}
	

	if (!connection) {
		printf ("Could not init connection...\n");
		return 1;
	}

	if (dbus_path) {
		path = dbus_path;
	} else {
		path = g_strdup_printf ("unix:path=/tmp/xmms-dbus-%s", g_get_user_name ());
	}

	if (!xmmsc_connect (connection, path)){
		printf ("couldn't connect to xmms2d: %s\n",
			xmmsc_get_last_error(connection));
		return 1;
	}

	mainloop = g_main_loop_new (NULL, FALSE);

	timer = g_timer_new ();

	xmmsc_ipc_setup_with_gmain (connection, NULL);

        if (SDL_Init(SDL_INIT_VIDEO) > 0) {
                fprintf(stderr, "Unable to init SDL: %s \n", SDL_GetError());
		return 1;
        }

	SDL_WM_SetCaption ("XMMS2 - SDL Client", NULL);

        if ( TTF_Init() < 0 ) {
                fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
		return 1;
        }

	if (fontfile) {
		font = TTF_OpenFont (fontfile, fontsize);
	} else {
		font = TTF_OpenFont("font.ttf", fontsize);
	}

	if (!font){
		fprintf(stderr, "couldn't open font! try -f <fontfile>\n");
		return 1;
	}

	if (!format) {
		format = DEFAULT_FORMAT;
	}

	if (!x) x = 350;
	if (!y) y = 20;
	screen = SDL_SetVideoMode (x, y, 32, 0);


	XMMS_CALLBACK_SET (connection, xmmsc_playback_playtime, handle_playtime, NULL);
	XMMS_CALLBACK_SET (connection, xmmsc_playback_current_id, handle_mediainfo, NULL);
	XMMS_CALLBACK_SET (connection, xmmsc_playback_status, handle_status, NULL);

	g_timeout_add (20, render, (gpointer)screen);

	g_main_loop_run (mainloop); /* GO GO GO! */

	if (connection) {
		xmmsc_deinit (connection);
	}

	TTF_Quit ();
	SDL_Quit ();

	return 0;
}
