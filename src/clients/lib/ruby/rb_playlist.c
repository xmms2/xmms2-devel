/*  XMMS2 - X Music Multiplexer System
 *
 *  Copyright (C) 2003-2016 XMMS2 Team
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

#include "rb_playlist.h"
#include "rb_xmmsclient.h"
#include "rb_result.h"
#include "rb_collection.h"

#define DEF_CONST(mod, prefix, name) \
	rb_define_const ((mod), #name, \
	                 INT2FIX (prefix##name));

#define PLAYLIST_METHOD_HANDLER_HEADER \
	RbPlaylist *pl = NULL; \
	RbXmmsClient *xmms = NULL; \
	xmmsc_result_t *res; \
\
	Data_Get_Struct (self, RbPlaylist, pl); \
	Data_Get_Struct (pl->xmms, RbXmmsClient, xmms); \
	CHECK_DELETED (xmms);

#define PLAYLIST_METHOD_HANDLER_FOOTER \
	return TO_XMMS_CLIENT_RESULT (pl->xmms, res);

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

#define PLAYLIST_METHOD_ADD_HANDLER_INT_STR(action, arg1, arg2)	\
	PLAYLIST_METHOD_HANDLER_HEADER \
\
	res = xmmsc_playlist_##action (xmms->real, pl->name, \
	                               check_int32 (arg1), \
	                               StringValuePtr (arg2)); \
\
	PLAYLIST_METHOD_HANDLER_FOOTER

#define PLAYLIST_METHOD_ADD_HANDLER_INT(action, arg) \
	PLAYLIST_METHOD_HANDLER_HEADER \
\
	res = xmmsc_playlist_##action (xmms->real, pl->name, \
	                               check_int32 (arg)); \
\
	PLAYLIST_METHOD_HANDLER_FOOTER

#define PLAYLIST_METHOD_ADD_HANDLER_INT_INT(action, arg1, arg2) \
	PLAYLIST_METHOD_HANDLER_HEADER \
\
	res = xmmsc_playlist_##action (xmms->real, pl->name, \
	                               check_int32 (arg1), \
	                               check_int32 (arg2)); \
\
	PLAYLIST_METHOD_HANDLER_FOOTER

typedef struct {
	VALUE xmms;

	VALUE name_value;
	const char *name;
} RbPlaylist;

static VALUE ePlaylistError, eDisconnectedError, eClientError;

static void
c_mark (RbPlaylist *pl)
{
	rb_gc_mark (pl->xmms);
	rb_gc_mark (pl->name_value);
}

static VALUE
c_alloc (VALUE klass)
{
	RbPlaylist *pl;

	return Data_Make_Struct (klass, RbPlaylist, c_mark, NULL, pl);
}

/*
 * call-seq:
 *  pl = Xmms::Playlist.new(xc, [name])
 * Initializes a new Xmms::Playlist using the playlist named _name_ and the
 * Xmms::Client instance _xc_. Xmms::Client#playlist(name) is a useful
 * shortcut. _name_ is is the name of the playlist and the active playlist will
 * be used if it is not specified. Raises PlaylistError if the playlist name is
 * invalid.
 */
static VALUE
c_init (int argc, VALUE *argv, VALUE self)
{
	RbPlaylist *pl = NULL;
	VALUE name, xmms = Qnil;

	Data_Get_Struct (self, RbPlaylist, pl);

	rb_scan_args (argc, argv, "11", &xmms, &name);

	/* FIXME: Check type! */
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
static VALUE
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
	int32_t id;

	PLAYLIST_METHOD_HANDLER_HEADER

	if (!NIL_P (rb_check_string_type (arg)))
		res = xmmsc_playlist_add_url (xmms->real, pl->name,
		                              StringValuePtr (arg));
	else {
		id = check_int32 (arg);
		res = xmmsc_playlist_add_id (xmms->real, pl->name, id);
	}

	PLAYLIST_METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 *  pl.radd(path) -> result
 *
 * Recursively imports all media files under _path_ to the playlist.
 */
static VALUE
c_radd (VALUE self, VALUE path)
{
	PLAYLIST_METHOD_ADD_HANDLER_STR (radd, path);
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
	int32_t id;
	int32_t ipos;

	PLAYLIST_METHOD_HANDLER_HEADER

	ipos = check_int32 (pos);

	if (!NIL_P (rb_check_string_type (arg)))
		res = xmmsc_playlist_insert_url (xmms->real, pl->name,
		                                 ipos, StringValuePtr (arg));
	else {
		id = check_int32 (arg);
		res = xmmsc_playlist_insert_id (xmms->real, pl->name,
		                                ipos, id);
	}

	PLAYLIST_METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 *  pl.rinsert(pos, path) -> result
 *
 * Recursively imports all media files under _path_ at position _pos_
 * in the playlist.
 */
static VALUE
c_rinsert (VALUE self, VALUE pos, VALUE path)
{
	PLAYLIST_METHOD_ADD_HANDLER_INT_STR (rinsert, pos, path);
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
	PLAYLIST_METHOD_ADD_HANDLER_INT (remove_entry, pos)
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
	PLAYLIST_METHOD_ADD_HANDLER_INT_INT (move_entry, cur_pos, new_pos)
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
	xmmsv_t *cprops;
	PLAYLIST_METHOD_HANDLER_HEADER

	cprops = parse_string_array2 (props);
	res = xmmsc_playlist_sort (xmms->real, pl->name, cprops);
	xmmsv_unref (cprops);

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

/*
 * call-seq:
 *  pl.add_collection(coll [, order]) -> result
 *
 * Adds the collection _coll_ to the playlist.
 */
static VALUE
c_add_collection (int argc, VALUE *argv, VALUE self)
{
	PLAYLIST_METHOD_HANDLER_HEADER

	VALUE rbcoll, order = Qnil;
	xmmsv_t *corder = NULL;
	xmmsc_coll_t *coll;

	rb_scan_args (argc, argv, "11", &rbcoll, &order);

	coll = FROM_XMMS_CLIENT_COLLECTION (rbcoll);

	if (!NIL_P (order))
		corder = parse_string_array2 (order);

	res = xmmsc_playlist_add_collection (xmms->real, pl->name,
	                                     coll, corder);

	if (corder)
		xmmsv_unref (corder);

	PLAYLIST_METHOD_HANDLER_FOOTER
}

VALUE
Init_Playlist (VALUE mXmms)
{
	VALUE c;

	c = rb_define_class_under (mXmms, "Playlist", rb_cObject);

	rb_define_alloc_func (c, c_alloc);
	rb_define_method (c, "initialize", c_init, -1);
	rb_define_method (c, "name", c_name, 0);
	rb_define_method (c, "current_pos", c_current_pos, 0);
	rb_define_method (c, "sort", c_sort, 1);
	rb_define_method (c, "shuffle", c_shuffle, 0);
	rb_define_method (c, "clear", c_clear, 0);
	rb_define_method (c, "add_entry", c_add_entry, 1);
	rb_define_method (c, "radd", c_radd, 1);
	rb_define_method (c, "rinsert", c_rinsert, 2);
	rb_define_method (c, "insert_entry", c_insert_entry, 2);
	rb_define_method (c, "remove_entry", c_remove_entry, 1);
	rb_define_method (c, "move_entry", c_move_entry, 2);
	rb_define_method (c, "entries", c_list_entries, 0);
	rb_define_method (c, "load", c_load, 0);
	rb_define_method (c, "remove", c_remove, 0);
	rb_define_method (c, "add_collection", c_add_collection, -1);

	rb_define_const (c, "ACTIVE_NAME", INT2FIX (XMMS_ACTIVE_PLAYLIST));

	DEF_CONST (c, XMMS_PLAYLIST_CHANGED_, ADD);
	DEF_CONST (c, XMMS_PLAYLIST_CHANGED_, INSERT);
	DEF_CONST (c, XMMS_PLAYLIST_CHANGED_, SHUFFLE);
	DEF_CONST (c, XMMS_PLAYLIST_CHANGED_, REMOVE);
	DEF_CONST (c, XMMS_PLAYLIST_CHANGED_, CLEAR);
	DEF_CONST (c, XMMS_PLAYLIST_CHANGED_, MOVE);
	DEF_CONST (c, XMMS_PLAYLIST_CHANGED_, SORT);
	DEF_CONST (c, XMMS_PLAYLIST_CHANGED_, UPDATE);

	ePlaylistError = rb_define_class_under (c, "PlaylistError",
	                                        rb_eStandardError);
	eClientError = rb_define_class_under (c, "ClientError",
	                                      rb_eStandardError);
	eDisconnectedError = rb_define_class_under (c, "DisconnectedError",
	                                            eClientError);

	return c;
}
