/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsc/xmmsc_idnumbers.h>

#include <ruby.h>

#include "rb_xmmsclient_playlist.h"
#include "rb_xmmsclient.h"
#include "rb_result.h"

#define PLAYLIST_METHOD_HANDLER_HEADER \
	RbPlaylist *pl = NULL; \
	RbXmmsClient *xmms = NULL; \
	xmmsc_result_t *res; \
\
	Data_Get_Struct (self, RbPlaylist, pl); \
	Data_Get_Struct (pl->xmms, RbXmmsClient, xmms); \
	CHECK_DELETED (xmms);

#define PLAYLIST_METHOD_HANDLER_FOOTER \
	return TO_XMMS_CLIENT_RESULT (self, res);

#define PLAYLIST_METHOD_ADD_HANDLER(action) \
	PLAYLIST_METHOD_HANDLER_HEADER \
\
	res = xmmsc_playlist_##action (xmms->real, pl->name); \
\
	PLAYLIST_METHOD_HANDLER_FOOTER

#define PLAYLIST_METHOD_ADD_HANDLER_STR(action, arg) \
	PLAYLIST_METHOD_HANDLER_HEADER \
\
	res = xmmsc_playlist_##action (xmms->real, pl->name, StringValuePtr (arg)); \
\
	PLAYLIST_METHOD_HANDLER_FOOTER

#define PLAYLIST_METHOD_ADD_HANDLER_UINT(action, arg) \
	PLAYLIST_METHOD_HANDLER_HEADER \
\
	Check_Type (arg, T_FIXNUM); \
\
	res = xmmsc_playlist_##action (xmms->real, pl->name, \
	                               NUM2UINT (arg)); \
\
	PLAYLIST_METHOD_HANDLER_FOOTER

#define PLAYLIST_METHOD_ADD_HANDLER_UINT_UINT(action, arg1, arg2) \
	PLAYLIST_METHOD_HANDLER_HEADER \
\
	Check_Type (arg1, T_FIXNUM); \
	Check_Type (arg2, T_FIXNUM); \
\
	res = xmmsc_playlist_##action (xmms->real, pl->name, \
	                               NUM2UINT (arg1), NUM2UINT (arg2)); \
\
	PLAYLIST_METHOD_HANDLER_FOOTER

typedef struct {
	VALUE xmms;

	VALUE name_value;
	const char *name;
} RbPlaylist;

static VALUE cPlaylist, ePlaylistError, eDisconnectedError, eClientError;

static void
c_mark (RbPlaylist *pl)
{
	rb_gc_mark (pl->xmms);
	rb_gc_mark (pl->name_value);
}

static void
c_free (RbPlaylist *pl)
{
	/* FIXME */
}

VALUE
playlist_new (VALUE xmms, VALUE name)
{
	VALUE self;
	RbPlaylist *pl = NULL;

	self = Data_Make_Struct (cPlaylist, RbPlaylist, c_mark, c_free, pl);

	pl->xmms = xmms;

	if (NIL_P (name))
		pl->name_value = rb_str_new2 (XMMS_ACTIVE_PLAYLIST);
	else
		pl->name_value = rb_str_dup (name);

	OBJ_FREEZE (pl->name_value);

	pl->name = StringValuePtr (pl->name_value);

	return self;
}

/*
 * call-seq:
 *  pl.name -> string
 * Returns the name of the playlist in the medialib as a String.
 */
VALUE
c_name (VALUE self)
{
	RbPlaylist *pl = NULL;

	Data_Get_Struct (self, RbPlaylist, pl);

	return pl->name_value;
}

/*
 * call-seq:
 *  pl.current_pos -> result
 *
 * Retrieves the current position of the playlist.
 * May raise an Xmms::Result::ValueError exception if the current position is
 * undefined.
 */
static VALUE
c_current_pos (VALUE self)
{
	PLAYLIST_METHOD_ADD_HANDLER (current_pos)
}

/*
 * call-seq:
 *  pl.entries -> result
 *
 * Retrieves an array containing ids for each position of the playlist.
 */
static VALUE
c_list_entries (VALUE self)
{
	PLAYLIST_METHOD_ADD_HANDLER (list_entries)
}

/*
 * call-seq:
 *  pl.add_entry(arg) -> result
 *
 * Adds an entry to the playlist. _arg_ can be either a URL or an id.
 */
static VALUE
c_add_entry (VALUE self, VALUE arg)
{
	PLAYLIST_METHOD_HANDLER_HEADER

	if (!NIL_P (rb_check_string_type (arg)))
		res = xmmsc_playlist_add_url (xmms->real, pl->name,
		                              StringValuePtr (arg));
	else if (rb_obj_is_kind_of (arg, rb_cFixnum))
		res = xmmsc_playlist_add_id (xmms->real, pl->name,
		                             NUM2UINT (arg));
	else
		rb_raise (eClientError, "unsupported argument");

	PLAYLIST_METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 *  pl.insert_entry(pos, arg) -> result
 *
 * Inserts an entry to the current playlist at position _pos_ in the playlist.
 * _arg_ can be either a URL or an id.
 */
static VALUE
c_insert_entry (VALUE self, VALUE pos, VALUE arg)
{
	PLAYLIST_METHOD_HANDLER_HEADER

	Check_Type (pos, T_FIXNUM);

	if (!NIL_P (rb_check_string_type (arg)))
		res = xmmsc_playlist_insert_url (xmms->real, pl->name,
		                                 NUM2UINT (pos), StringValuePtr (arg));
	else if (rb_obj_is_kind_of (arg, rb_cFixnum))
		res = xmmsc_playlist_insert_id (xmms->real, pl->name,
		                                NUM2UINT (pos), NUM2UINT (arg));
	else
		rb_raise (ePlaylistError, "unsupported argument");

	PLAYLIST_METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 *  pl.remove_entry(pos) -> result
 *
 * Removes the entry at _pos_ from the playlist.
 */
static VALUE
c_remove_entry (VALUE self, VALUE pos)
{
	PLAYLIST_METHOD_ADD_HANDLER_UINT (remove_entry, pos)
}

/*
 * call-seq:
 *  pl.playlist_move_entry(current_pos, new_pos) -> result
 *
 * Moves the entry at _current_pos_ to _new_pos_ in the playlist.
 */
static VALUE
c_move_entry (VALUE self, VALUE cur_pos, VALUE new_pos)
{
	PLAYLIST_METHOD_ADD_HANDLER_UINT_UINT (move_entry, cur_pos, new_pos)
}

/*
 * call-seq:
 *  pl.shuffle -> result
 *
 * Shuffles the playlist.
 */
static VALUE
c_shuffle (VALUE self)
{
	PLAYLIST_METHOD_ADD_HANDLER (shuffle)
}

/*
 * call-seq:
 *  pl.clear -> result
 *
 * Clears the playlist.
 */
static VALUE
c_clear (VALUE self)
{
	PLAYLIST_METHOD_ADD_HANDLER (clear)
}

/*
 * call-seq:
 *  pl.sort(properties) -> result
 *
 * Sorts the playlist on _properties_, which is an array of medialib properties
 * such as ["title", "artist"].
 */
static VALUE
c_sort (VALUE self, VALUE props)
{
	struct RArray *ary;
	const char **cprops;
	int i;
	PLAYLIST_METHOD_HANDLER_HEADER

	if (!NIL_P (props = rb_check_array_type (props))) {
		ary = RARRAY (props);

		cprops = malloc (sizeof (char *) * (ary->len + 1));

		for (i = 0; i < ary->len; i++)
			cprops[i] = StringValuePtr (ary->ptr[i]);

		cprops[i] = NULL;
	} else if (!NIL_P (rb_check_string_type (props))) {
		cprops = malloc (sizeof (char *) * 2);
		cprops[0] = StringValuePtr (props);
		cprops[1] = NULL;
	} else
		rb_raise (ePlaylistError, "unsupported argument");

	res = xmmsc_playlist_sort (xmms->real, pl->name, cprops);

	free (cprops);

	PLAYLIST_METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 *  pl.load -> result
 *
 * Loads the playlist as the current active playlist.
 */
static VALUE
c_load (VALUE self)
{
	PLAYLIST_METHOD_ADD_HANDLER (load);
}

/*
 * call-seq:
 *  pl.remove -> result
 *
 * Removes the playlist from the medialib.
 */
static VALUE
c_remove (VALUE self)
{
	PLAYLIST_METHOD_ADD_HANDLER (remove);
}

void
Init_Playlist (VALUE cClient)
{
	cPlaylist = rb_define_class_under (cClient, "Playlist", rb_cObject);

	/* ugh, we have to define the "new" method,
	 * so we can remove it again :(
	 */
	rb_define_singleton_method (cPlaylist, "new", NULL, 0);
	rb_undef_method (rb_singleton_class (cPlaylist), "new");

	rb_define_method (cPlaylist, "name", c_name, 0);
	rb_define_method (cPlaylist, "current_pos", c_current_pos, 0);
	rb_define_method (cPlaylist, "sort", c_sort, 1);
	rb_define_method (cPlaylist, "shuffle", c_shuffle, 0);
	rb_define_method (cPlaylist, "clear", c_clear, 0);
	rb_define_method (cPlaylist, "add_entry", c_add_entry, 1);
	rb_define_method (cPlaylist, "insert_entry", c_insert_entry, 2);
	rb_define_method (cPlaylist, "remove_entry", c_remove_entry, 1);
	rb_define_method (cPlaylist, "move_entry", c_move_entry, 2);
	rb_define_method (cPlaylist, "entries", c_list_entries, 0);
	rb_define_method (cPlaylist, "load", c_load, 0);
	rb_define_method (cPlaylist, "remove", c_remove, 0);

	ePlaylistError = rb_define_class_under (cPlaylist, "PlaylistError",
	                                        rb_eStandardError);
	eClientError = rb_define_class_under (cPlaylist, "ClientError",
	                                      rb_eStandardError);
	eDisconnectedError = rb_define_class_under (cPlaylist, "DisconnectedError",
	                                            eClientError);
}
