/*  XMMS2 - X Music Multiplexer System
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
#include <xmmsc/xmmsc_stdbool.h>

#include "rb_xmmsclient.h"
#include "rb_playlist.h"
#include "rb_collection.h"
#include "rb_result.h"

#define METHOD_HANDLER_HEADER \
	RbXmmsClient *xmms = NULL; \
	xmmsc_result_t *res; \
\
	Data_Get_Struct (self, RbXmmsClient, xmms); \
	CHECK_DELETED (xmms);

#define METHOD_HANDLER_FOOTER \
	return TO_XMMS_CLIENT_RESULT (self, res);

#define METHOD_ADD_HANDLER(name) \
	METHOD_HANDLER_HEADER \
\
	res = xmmsc_##name (xmms->real); \
	METHOD_HANDLER_FOOTER

#define METHOD_ADD_HANDLER_STR(name, arg1) \
	METHOD_HANDLER_HEADER \
\
	res = xmmsc_##name (xmms->real, StringValuePtr (arg1)); \
	METHOD_HANDLER_FOOTER

#define METHOD_ADD_HANDLER_STR_STR(name, arg1, arg2) \
	METHOD_HANDLER_HEADER \
\
	res = xmmsc_##name (xmms->real, \
	                    StringValuePtr (arg1), StringValuePtr (arg2)); \
	METHOD_HANDLER_FOOTER

#define METHOD_ADD_HANDLER_INT(name, arg1) \
	METHOD_HANDLER_HEADER \
\
	res = xmmsc_##name (xmms->real, check_int32 (arg1)); \
	METHOD_HANDLER_FOOTER

#define METHOD_ADD_HANDLER_INT_INT(name, arg1, arg2) \
	METHOD_HANDLER_HEADER \
\
	res = xmmsc_##name (xmms->real, check_int32 (arg1), check_int32 (arg2)); \
	METHOD_HANDLER_FOOTER

#define METHOD_ADD_HANDLER_BIN(name, arg1) \
	METHOD_HANDLER_HEADER \
\
	StringValue (arg1); \
\
	res = xmmsc_##name (xmms->real, \
	                    (unsigned char *) RSTRING_PTR (arg1), \
	                    RSTRING_LEN (arg1)); \
	METHOD_HANDLER_FOOTER

#define rb_check_hash_type(x) rb_check_convert_type(x, T_HASH, "Hash", "to_hash")

static VALUE cPlaylist;
static VALUE eClientError, eDisconnectedError;
static ID id_lt, id_gt;

void Init_Client (VALUE);

static void
c_mark (RbXmmsClient *xmms)
{
	rb_gc_mark (xmms->result_callbacks);

	if (!NIL_P (xmms->disconnect_cb))
		rb_gc_mark (xmms->disconnect_cb);

	if (!NIL_P (xmms->io_need_out_cb))
		rb_gc_mark (xmms->io_need_out_cb);
}

static void
c_free (RbXmmsClient *xmms)
{
	if (xmms->real && !xmms->deleted)
		xmmsc_unref (xmms->real);

	free (xmms);
}

static VALUE
c_alloc (VALUE klass)
{
	RbXmmsClient *xmms;

	return Data_Make_Struct (klass, RbXmmsClient,
	                         c_mark, c_free, xmms);
}

/*
 * call-seq:
 *  Xmms::Client.new(name) -> xc
 *
 * Creates an Xmms::Client object.
 */
static VALUE
c_init (VALUE self, VALUE name)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	if (!(xmms->real = xmmsc_init (StringValuePtr (name)))) {
		rb_raise (rb_eNoMemError, "failed to allocate memory");
		return Qnil;
	}

	xmms->deleted = false;
	xmms->result_callbacks = rb_ary_new ();
	xmms->disconnect_cb = Qnil;
	xmms->io_need_out_cb = Qnil;

	return self;
}

/*
 * call-seq:
 *  xc.connect([path]) -> self
 *
 * Connects _xc_ to the XMMS2 daemon listening at _path_.
 * If _path_ isn't given, the default path is used.
 */
static VALUE
c_connect (int argc, VALUE *argv, VALUE self)
{
	VALUE path;
	RbXmmsClient *xmms = NULL;
	char *p = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	rb_scan_args (argc, argv, "01", &path);

	if (!NIL_P (path))
		p = StringValuePtr (path);

	if (!xmmsc_connect (xmms->real, p))
		rb_raise (eClientError,
		          "cannot connect to daemon (%s)",
		          xmmsc_get_last_error (xmms->real));

	return self;
}

static VALUE
c_delete (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmmsc_unref (xmms->real);
	xmms->deleted = true;

	return Qnil;
}

static void
on_disconnect (void *data)
{
	VALUE self = (VALUE) data;
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	rb_funcall (xmms->disconnect_cb, rb_intern ("call"), 0);
}

/*
 * call-seq:
 *  xc.on_disconnect { } -> self
 *
 * Sets the block that's executed when _xc_ is disconnected from the
 * XMMS2 daemon.
 */
static VALUE
c_on_disconnect (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	if (!rb_block_given_p ())
		return Qnil;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmms->disconnect_cb = rb_block_proc ();

	xmmsc_disconnect_callback_set (xmms->real,
	                               on_disconnect, (void *) self);

	return self;
}

/*
 * call-seq:
 *  xc.last_error -> string or nil
 *
 * Returns the last error that occured in _xc_ or +nil+, if no error
 * occured yet.
 */
static VALUE
c_last_error_get (VALUE self)
{
	RbXmmsClient *xmms = NULL;
	const char *s;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	s = xmmsc_get_last_error (xmms->real);

	return s ? rb_str_new2 (s) : Qnil;
}

/*
 * call-seq:
 *  xc.io_fd -> integer
 *
 * Returns the file descriptor of the Xmms::Client IPC socket.
 */
static VALUE
c_io_fd (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	return INT2NUM (xmmsc_io_fd_get (xmms->real));
}

/*
 * call-seq:
 *  xc.io_want_out -> true or false
 *
 * Returns +true+ if an outgoing (to server) clientlib command is pending,
 * +false+ otherwise.
 */
static VALUE
c_io_want_out (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	return xmmsc_io_want_out (xmms->real) ? Qtrue : Qfalse;
}

static void
on_io_need_out (int flag, void *data)
{
	VALUE self = (VALUE) data;
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	rb_funcall (xmms->io_need_out_cb, rb_intern ("call"), 1, INT2NUM (flag));
}

/*
 * call-seq:
 *  xc.io_on_need_out { |flag| }
 *
 * Sets the block that's called when the output socket state changes.
 */
static VALUE
c_io_on_need_out (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	if (!rb_block_given_p ())
		return Qnil;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmms->io_need_out_cb = rb_block_proc ();

	xmmsc_io_need_out_callback_set (xmms->real,
	                                on_io_need_out, (void *) self);

	return Qnil;
}

/*
 * call-seq:
 *  xc.io_in_handle -> nil
 *
 * Retrieves one incoming (from server) clientlib command if there are any in
 * the buffer.
 */
static VALUE
c_io_in_handle (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmmsc_io_in_handle (xmms->real);

	return Qnil;
}

/*
 * call-seq:
 *  xc.io_out_handle -> nil
 *
 * Sends one outgoing (to server) clientlib command. You should check
 * #io_want_out before calling this method.
 */
static VALUE
c_io_out_handle (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmmsc_io_out_handle (xmms->real);

	return Qnil;
}

/*
 * call-seq:
 *  xc.io_disconnect -> nil
 *
 * Disconnects the IPC socket. This should only be used by event loop
 * implementations.
 */
static VALUE
c_io_disconnect (VALUE self)
{
	RbXmmsClient *xmms = NULL;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	xmmsc_io_disconnect (xmms->real);

	return Qnil;
}

/*
 * call-seq:
 *  xc.quit -> result
 *
 * Shuts down the XMMS2 daemon.
 */
static VALUE
c_quit (VALUE self)
{
	METHOD_ADD_HANDLER (quit);
}

/*
 * call-seq:
 *  xc.broadcast_quit -> result
 *
 * Will be called when the server is terminating.
 */
static VALUE
c_broadcast_quit (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_quit);
}

/*
 * call-seq:
 *  xc.playback_start -> result
 *
 * Starts playback.
 */
static VALUE
c_playback_start (VALUE self)
{
	METHOD_ADD_HANDLER (playback_start);
}

/*
 * call-seq:
 *  xc.playback_pause -> result
 *
 * Pauses playback.
 */
static VALUE
c_playback_pause (VALUE self)
{
	METHOD_ADD_HANDLER (playback_pause);
}

/*
 * call-seq:
 *  xc.playback_stop -> result
 *
 * Stops playback.
 */
static VALUE
c_playback_stop (VALUE self)
{
	METHOD_ADD_HANDLER (playback_stop);
}

/*
 * call-seq:
 *  xc.playback_tickle -> result
 *
 * Advances to the next playlist entry.
 */
static VALUE
c_playback_tickle (VALUE self)
{
	METHOD_ADD_HANDLER (playback_tickle);
}

/*
 * call-seq:
 *  xc.playback_status -> result
 *
 * Retrieves the playback status.
 */
static VALUE
c_playback_status (VALUE self)
{
	METHOD_ADD_HANDLER (playback_status);
}

/*
 * call-seq:
 *  xc.broadcast_playback_status -> result
 *
 * Retrieves the playback status as a broadcast.
 */
static VALUE
c_broadcast_playback_status (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_playback_status);
}

/*
 * call-seq:
 *  xc.playback_playtime -> result
 *
 * Retrieves the playtime.
 */
static VALUE
c_playback_playtime (VALUE self)
{
	METHOD_ADD_HANDLER (playback_playtime);
}

/*
 * call-seq:
 *  xc.signal_playback_playtime -> result
 *
 * Retrieves the playtime as a signal.
 */
static VALUE
c_signal_playback_playtime (VALUE self)
{
	METHOD_ADD_HANDLER (signal_playback_playtime);
}

/*
 * call-seq:
 *  xc.playback_current_id -> result
 *
 * Retrieves the media id of the currently played track.
 */
static VALUE
c_playback_current_id (VALUE self)
{
	METHOD_ADD_HANDLER (playback_current_id);
}

/*
 * call-seq:
 *  xc.broadcast_playback_current_id -> result
 *
 * Retrieves the media id of the currently played track as a broadcast.
 */
static VALUE
c_broadcast_playback_current_id (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_playback_current_id);
}

/*
 * call-seq:
 *  xc.broadcast_config_value_changed -> result
 *
 * Retrieves configuration properties as a broadcast.
 */
static VALUE
c_broadcast_config_value_changed (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_config_value_changed);
}

/*
 * call-seq:
 *  xc.playback_seek_ms(ms) -> result
 *
 * Seek to the song position given in _ms_.
 */
static VALUE
c_playback_seek_ms (VALUE self, VALUE ms)
{
	METHOD_HANDLER_HEADER

	res = xmmsc_playback_seek_ms (xmms->real,
	                              check_int32 (ms),
	                              XMMS_PLAYBACK_SEEK_SET);

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 *  xc.playback_seek_ms_rel(ms) -> result
 *
 * Seek in the song by the offset given in ms.
 */
static VALUE
c_playback_seek_ms_rel (VALUE self, VALUE ms)
{
	METHOD_HANDLER_HEADER

	res = xmmsc_playback_seek_ms (xmms->real,
	                              check_int32 (ms),
	                              XMMS_PLAYBACK_SEEK_CUR);

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 *  xc.playback_seek_samples(samples) -> result
 *
 * Seek to the song position given in _samples_.
 */
static VALUE
c_playback_seek_samples (VALUE self, VALUE samples)
{
	METHOD_HANDLER_HEADER

	res = xmmsc_playback_seek_samples (xmms->real,
	                                   check_int32 (samples),
	                                   XMMS_PLAYBACK_SEEK_SET);

	METHOD_HANDLER_FOOTER
}

 /*
  * call-seq:
  *  xc.playback_seek_samples_rel(samples) -> result
  *
  * Seek in the song position by the offset given in samples.
  */
static VALUE
c_playback_seek_samples_rel (VALUE self, VALUE samples)
{
	METHOD_HANDLER_HEADER

	res = xmmsc_playback_seek_samples (xmms->real,
	                                   check_int32 (samples),
	                                   XMMS_PLAYBACK_SEEK_CUR);

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 *  xc.playback_volume_set(channel, volume) -> result
 *
 * Sets playback volume for _channel_ to _volume_.
 */
static VALUE
c_playback_volume_set (VALUE self, VALUE channel, VALUE volume)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	Check_Type (channel, T_SYMBOL);
	Check_Type (volume, T_FIXNUM);

	res = xmmsc_playback_volume_set (xmms->real,
	                                 rb_id2name (SYM2ID (channel)),
	                                 NUM2INT (volume));

	return TO_XMMS_CLIENT_RESULT (self, res);
}

/*
 * call-seq:
 *  xc.playback_volume_get -> result
 *
 * Gets the current playback volume.
 */
static VALUE
c_playback_volume_get (VALUE self)
{
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	res = xmmsc_playback_volume_get (xmms->real);

	return TO_XMMS_CLIENT_RESULT (self, res);
}

/*
 * call-seq:
 *  xc.broadcast_volume_changed -> result
 *
 * Registers a broadcast handler that's evoked when the playback volume
 * changes.
 */
static VALUE
c_broadcast_playback_volume_changed (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_playback_volume_changed);
}

/*
 * call-seq:
 *  xc.broadcast_playlist_changed -> result
 *
 * Retrieves a hash describing the change to the playlist as a broadcast.
 */
static VALUE
c_broadcast_playlist_changed (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_playlist_changed);
}

/*
 * call-seq:
 *  xc.broadcast_playlist_current_pos -> result
 *
 * Retrieves the current playlist position as a broadcast. See
 * _playlist_current_pos_.
 */
static VALUE
c_broadcast_playlist_current_pos (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_playlist_current_pos);
}

/*
 * call-seq:
 *  xc.broadcast_playlist_loaded -> result
 *
 * Will be called when a playlist has been loaded.
 */
static VALUE
c_broadcast_playlist_loaded (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_playlist_loaded);
}

/*
 * call-seq:
 *  xc.broadcast_medialib_entry_changed -> result
 *
 * Retrieves the id of a changed medialib entry as a broadcast.
 */
static VALUE
c_broadcast_medialib_entry_changed (VALUE self)
{
	RB_XMMS_DEPRECATED (broadcast_medialib_entry_changed, broadcast_medialib_entry_updated);
	METHOD_ADD_HANDLER (broadcast_medialib_entry_updated);
}

/*
 * call-seq:
 *  xc.broadcast_medialib_entry_changed -> result
 *
 * Retrieves the id of a changed medialib entry as a broadcast.
 */
static VALUE
c_broadcast_medialib_entry_updated (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_medialib_entry_updated);
}

/*
 * call-seq:
 *  xc.broadcast_medialib_entry_added -> result
 *
 * Retrieves the id of an added medialib entry as a broadcast.
 */
static VALUE
c_broadcast_medialib_entry_added (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_medialib_entry_added);
}

/*
 * call-seq:
 *  xc.broadcast_medialib_entry_removed -> result
 *
 * Retrieves the id of an removed medialib entry as a broadcast.
 */
static VALUE
c_broadcast_medialib_entry_removed (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_medialib_entry_removed);
}

/*
 * call-seq:
 *  xc.playlist_set_next(pos) -> result
 *
 * Sets the next song to be played to _pos_ (an absolute position).
 */
static VALUE
c_playlist_set_next (VALUE self, VALUE pos)
{
	METHOD_ADD_HANDLER_INT (playlist_set_next, pos);
}

/*
 * call-seq:
 *  xc.playlist_set_next_rel(pos) -> result
 *
 * Sets the next song to be played based on the current position where
 * _pos_ is a value relative to the current position.
 */
static VALUE
c_playlist_set_next_rel (VALUE self, VALUE pos)
{
	METHOD_ADD_HANDLER_INT (playlist_set_next_rel, pos);
}

/* call-seq:
 * xc.playlist([name]) -> p
 *
 * Shortcut for Xmms::Playlist.new. Creates a new Xmms::Playlist
 * object tied to the current Xmms::Client instance. _name_ is is the name of
 * the playlist and the active playlist will be used if it is not specified.
 * Raises PlaylistError if the playlist name is invalid.
 */
static VALUE
c_playlist (int argc, VALUE *argv, VALUE self)
{
	VALUE args[2] = {self, Qnil};

	rb_scan_args (argc, argv, "01", &args[1]);

	return rb_class_new_instance (2, args, cPlaylist);
}

/*
 * call-seq:
 *  xc.medialib_add_entry(url) -> result
 *
 * Adds _url_ to the medialib.
 */
static VALUE
c_medialib_add_entry (VALUE self, VALUE url)
{
	METHOD_ADD_HANDLER_STR (medialib_add_entry, url);
}

/*
 * call-seq:
 *	xc.medialib_get_id(url) -> result
 *
 * Retrieves the id corresponding to _url_.
 */
static VALUE
c_medialib_get_id (VALUE self, VALUE url)
{
	METHOD_ADD_HANDLER_STR (medialib_get_id, url);
}

/*
 * call-seq:
 *  xc.medialib_get_info(id) -> result
 *
 * Retrieves medialib info for _id_.
 */
static VALUE
c_medialib_get_info (VALUE self, VALUE id)
{
	METHOD_ADD_HANDLER_INT (medialib_get_info, id);
}

/*
 * call-seq:
 *  xc.medialib_entry_property_set(id, key, value, *source) -> result
 *
 * Write info to the medialib at _id_. _source_ is an optional argument that
 * describes where to write the mediainfo. If _source_ is omitted, the
 * mediainfo is written to "client/<yourclient>" where <yourclient> is the
 * name you specified in ::new.
 */
static VALUE
c_medialib_entry_property_set (int argc, VALUE *argv, VALUE self)
{
	VALUE tmp, key, value, src = Qnil;
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;
	const char *ckey;
	bool is_str = false;
	int32_t id, ivalue;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	rb_scan_args (argc, argv, "31", &tmp, &key, &value, &src);

	id = check_int32 (tmp);
	Check_Type (key, T_SYMBOL);

	if (!NIL_P (rb_check_string_type (value)))
		is_str = true;
	else
		ivalue = check_int32 (value);

	ckey = rb_id2name (SYM2ID (key));

	if (NIL_P (src) && is_str)
		res = xmmsc_medialib_entry_property_set_str (xmms->real, id,
		                                             ckey,
		                                             StringValuePtr (value));
	else if (NIL_P (src))
		res = xmmsc_medialib_entry_property_set_int (xmms->real, id,
		                                             ckey, ivalue);
	else if (is_str)
		res = xmmsc_medialib_entry_property_set_str_with_source (
			xmms->real, id,
			StringValuePtr (src),
			ckey,
			StringValuePtr (value));
	else
		res = xmmsc_medialib_entry_property_set_int_with_source (
			xmms->real, id,
			StringValuePtr (src),
			ckey, ivalue);

	return TO_XMMS_CLIENT_RESULT (self, res);
}

/*
 * call-seq:
 *  xc.medialib_entry_property_remove(id, key, *source) -> result
 *
 * Remove a custom field in the medialib associated with the entry _id_.
 * _source_ is an optional argument that describes where to write the
 * mediainfo. If _source_ is omitted, "client/<yourclient>" is used,
 * where <yourclient> is the name you specified in ::new.
 */
static VALUE
c_medialib_entry_property_remove (int argc, VALUE *argv, VALUE self)
{
	VALUE tmp, key, src = Qnil;
	RbXmmsClient *xmms = NULL;
	xmmsc_result_t *res;
	const char *ckey;
	int32_t id;

	Data_Get_Struct (self, RbXmmsClient, xmms);

	CHECK_DELETED (xmms);

	rb_scan_args (argc, argv, "21", &tmp, &key, &src);

	id = check_int32 (tmp);
	Check_Type (key, T_SYMBOL);

	ckey = rb_id2name (SYM2ID (key));

	if (NIL_P (src))
		res = xmmsc_medialib_entry_property_remove (xmms->real, id,
		                                            ckey);
	else
		res = xmmsc_medialib_entry_property_remove_with_source (
			xmms->real, id,
			StringValuePtr (src),
			ckey);

	return TO_XMMS_CLIENT_RESULT (self, res);
}

/*
 * call-seq:
 *  xc.medialib_entry_move(id, url) -> result
 *
 * Moves the entry specified by _id_ to a new URL without changing mediainfo.
 */
static VALUE
c_medialib_entry_move (VALUE self, VALUE id, VALUE url)
{
	METHOD_HANDLER_HEADER

	res = xmmsc_medialib_move_entry (xmms->real, check_int32 (id),
	                                 StringValuePtr (url));

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 *  xc.medialib_entry_remove(id) -> result
 *
 * Removes the entry specified by _id_ from the medialib.
 */
static VALUE
c_medialib_entry_remove (VALUE self, VALUE id)
{
	METHOD_ADD_HANDLER_INT (medialib_remove_entry, id)
}

/*
 * call-seq:
 *  xc.playlist_list -> result
 *
 * Retrieves a list of all saved playlists from the medialib.
 * Note that clients should treat internally used playlists
 * (marked with a leading underscore) carefully.
 */
static VALUE
c_playlist_list (VALUE self)
{
	METHOD_ADD_HANDLER (playlist_list);
}

/*
 * call-seq:
 *  xc.playlist_current_active -> result
 *
 * Retrieves the name of the active playlist.
 */
static VALUE
c_playlist_current_active (VALUE self)
{
	METHOD_ADD_HANDLER (playlist_current_active);
}

/*
 * call-seq:
 *  xc.medialib_path_import(path) -> result
 *
 * Recursively imports all media files under _path_ to the medialib.
 */
static VALUE
c_medialib_path_import (VALUE self, VALUE path)
{
	METHOD_ADD_HANDLER_STR (medialib_import_path, path);
}

/*
 * call-seq:
 *  xc.medialib_path_import(path) -> result
 *
 * Recursively imports all media files under the url encoded _path_ to the medialib.
 */
static VALUE
c_medialib_path_import_encoded (VALUE self, VALUE path)
{
	METHOD_ADD_HANDLER_STR (medialib_import_path_encoded, path);
}

/*
 * call-seq:
 *  xc.medialib_rehash(id) -> result
 *
 * Rehashes the medialib entry at _id_ or the whole medialib if _id_ == 0.
 */
static VALUE
c_medialib_rehash (VALUE self, VALUE id)
{
	METHOD_ADD_HANDLER_INT (medialib_rehash, id);
}

/*
 * call-seq:
 *  xc.xform_media_browse(url) -> result
 *
 * returns a list of files from the server
 */
static VALUE
c_xform_media_browse (VALUE self, VALUE url)
{
	METHOD_ADD_HANDLER_STR (xform_media_browse, url);
}

/*
 * call-seq:
 *  xc.broadcast_mediainfo_reader_status -> result
 *
 * Requests the status of the mediainfo reader.
 */
static VALUE
c_broadcast_mediainfo_reader_status (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_mediainfo_reader_status);
}

/*
 * call-seq:
 *  xc.signal_mediainfo_reader_unindexed -> result
 *
 * Requests the number of unindexed entries in the medialib.
 */
static VALUE
c_signal_mediainfo_reader_unindexed (VALUE self)
{
	METHOD_ADD_HANDLER (signal_mediainfo_reader_unindexed);
}

/*
 * call-seq:
 *  xc.plugin_list -> result
 *
 * Retrieves an array containing a hash of information for each plugin.
 */
static VALUE
c_plugin_list (int argc, VALUE *argv, VALUE self)
{
	VALUE type = Qnil;

	rb_scan_args (argc, argv, "01", &type);

	if (NIL_P (type))
		type = INT2FIX (XMMS_PLUGIN_TYPE_ALL);

	METHOD_ADD_HANDLER_INT (main_list_plugins, type);
}

/*
 * call-seq:
 *	xc.main_stats -> result
 *
 * Retrieves a hash containing statistics about the daemon.
 */
static VALUE
c_main_stats (VALUE self)
{
	METHOD_ADD_HANDLER (main_stats);
}

/*
 * call-seq:
 *  xc.config_list_values -> result
 *
 * Retrieves a list of all config values.
 */
static VALUE
c_config_list_values (VALUE self)
{
	METHOD_ADD_HANDLER (config_list_values);
}

/*
 * call-seq:
 *  xc.config_get_value(key) -> result
 *
 * Retrieves the value of the configuration property at _key_.
 */
static VALUE
c_config_get_value (VALUE self, VALUE key)
{
	METHOD_ADD_HANDLER_STR (config_get_value, key);
}

/*
 * call-seq:
 *  xc.config_set_value(key, value) -> result
 *
 * Sets the value of the configuration property at _key_ to _value_.
 */
static VALUE
c_config_set_value (VALUE self, VALUE key, VALUE val)
{
	METHOD_ADD_HANDLER_STR_STR (config_set_value, key, val);
}

/*
 * call-seq:
 *  xc.config_register_value(key, default_value) -> result
 *
 * Registers a configuration property at _key_ with the given default value.
 */
static VALUE
c_config_register_value (VALUE self, VALUE key, VALUE defval)
{
	METHOD_ADD_HANDLER_STR_STR (config_register_value, key, defval);
}

/*
 * call-seq:
 *  xc.bindata_add(str) -> result
 *
 * Stores binary data on the server.
 */
static VALUE
c_bindata_add (VALUE self, VALUE data)
{
	METHOD_ADD_HANDLER_BIN (bindata_add, data);
}

/*
 * call-seq:
 *  xc.bindata_retrieve(hash) -> result
 *
 * Retrieves the bindata entry specified by hash (hex string)
 * from the server.
 */
static VALUE
c_bindata_retrieve (VALUE self, VALUE hash)
{
	METHOD_ADD_HANDLER_STR (bindata_retrieve, hash);
}

/*
 * call-seq:
 *  xc.bindata_remove(hash) -> result
 *
 * Remove a datafile from the server.
 */
static VALUE
c_bindata_remove (VALUE self, VALUE hash)
{
	METHOD_ADD_HANDLER_STR (bindata_remove, hash);
}

/*
 * call-seq:
 *  xc.bindata_list -> result
 *
 * List all bindata hashes stored on the server.
 */
static VALUE
c_bindata_list (VALUE self)
{
	METHOD_ADD_HANDLER (bindata_list);
}

/* call-seq:
 * xc.coll_get(name, [ns])
 *
 * Returns a result containing an Xmms::Collection object referencing the
 * collection named _name_. The namespace _ns_ is searched or all namespaces if
 * unspecified.
 */
static VALUE
c_coll_get (int argc, VALUE *argv, VALUE self)
{
	VALUE name, ns = Qnil;
	METHOD_HANDLER_HEADER

	rb_scan_args (argc, argv, "11", &name, &ns);

	if (NIL_P (ns))
		res = xmmsc_coll_get (xmms->real, StringValuePtr (name),
		                      XMMS_COLLECTION_NS_ALL);
	else
		res = xmmsc_coll_get (xmms->real, StringValuePtr (name),
		                      StringValuePtr (ns));

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 * xc.coll_list([ns]) -> result
 *
 * Retrieves an array of the names of all the collections stored in the
 * medialib under the namespace _ns_. If _ns_ is not specified, it defaults to
 * Xmms::Collection::NS_ALL.
 */
static VALUE
c_coll_list (int argc, VALUE *argv, VALUE self)
{
	VALUE ns = Qnil;
	METHOD_HANDLER_HEADER

	rb_scan_args (argc, argv, "01", &ns);

	if (NIL_P (ns))
		ns = rb_str_new2 (XMMS_COLLECTION_NS_ALL);

	res = xmmsc_coll_list (xmms->real, StringValuePtr (ns));

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 * xc.coll_save(coll, name, ns) -> result
 *
 * Save the collection _coll_ named _name_ under the namespace _ns_ to the
 * media library.
 */
static VALUE
c_coll_save (VALUE self, VALUE coll, VALUE name, VALUE ns)
{
	METHOD_HANDLER_HEADER

	/* FIXME: Check that we actually have a Collection object */

	res = xmmsc_coll_save (xmms->real,
	                       FROM_XMMS_CLIENT_COLLECTION (coll),
	                       StringValuePtr (name),
	                       StringValuePtr (ns));

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 * xc.coll_remove(name, [ns]) -> result
 *
 * Remove the collection named _name_ from the media library. The collection is
 * removed from the namespace _ns_ or all namespaces if unspecified.
 */
static VALUE
c_coll_remove (int argc, VALUE *argv, VALUE self)
{
	VALUE name, ns = Qnil;
	METHOD_HANDLER_HEADER

	rb_scan_args (argc, argv, "11", &name, &ns);

	if (NIL_P (ns))
		res = xmmsc_coll_remove (xmms->real, StringValuePtr (name),
		                         XMMS_COLLECTION_NS_ALL);
	else
		res = xmmsc_coll_remove (xmms->real, StringValuePtr (name),
		                         StringValuePtr (ns));

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 * xc.coll_find(id, ns) -> result
 *
 * Find all collections in the given namespace _ns_ which contain _id_.
 */
static VALUE
c_coll_find (VALUE self, VALUE id, VALUE ns)
{
	METHOD_HANDLER_HEADER

	res = xmmsc_coll_find (xmms->real, check_int32 (id),
	                       StringValuePtr (ns));

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 * xc.coll_rename(old, new, [ns]) -> result
 *
 * Rename the saved collection from _old_ to _new_ within the namespace _ns_ or
 * all namespaces.
 */
static VALUE
c_coll_rename (int argc, VALUE *argv, VALUE self)
{
	VALUE old, new, ns = Qnil;
	METHOD_HANDLER_HEADER

	rb_scan_args (argc, argv, "21", &old, &new, &ns);

	if (NIL_P (ns))
		res = xmmsc_coll_rename (xmms->real,
		                         StringValuePtr (old),
		                         StringValuePtr (new),
		                         XMMS_COLLECTION_NS_ALL);
	else
		res = xmmsc_coll_rename (xmms->real,
		                         StringValuePtr (old),
		                         StringValuePtr (new),
		                         StringValuePtr (ns));

	METHOD_HANDLER_FOOTER
}

static int
parse_fetch_spec_foreach (VALUE key, VALUE value, VALUE udata)
{
	xmmsv_t *spec = (xmmsv_t *) udata;
	xmmsv_t *elem;
	int i;

	if (NIL_P (rb_check_string_type (key))) {
		rb_raise (rb_eArgError, "Key must be string");
	}

	if (!NIL_P (rb_check_string_type (value))) {
		xmmsv_dict_set_string (spec,
		                       StringValuePtr (key),
		                       StringValuePtr (value));
	} else if (!NIL_P (rb_check_hash_type (value))) {
		elem = xmmsv_new_dict ();
		xmmsv_dict_set (spec, StringValuePtr (key), elem);
		xmmsv_unref (elem);
		rb_hash_foreach (value, parse_fetch_spec_foreach, (VALUE) elem);
	} else if (!NIL_P (rb_check_array_type (value))) {
		elem = xmmsv_new_list ();
		xmmsv_dict_set (spec, StringValuePtr (key), elem);
		xmmsv_unref (elem);

		for (i = 0; i < RARRAY_LEN (value); i++) {
			VALUE entry = RARRAY_PTR (value)[i];
			if (NIL_P (rb_check_string_type (entry)))
				rb_raise (rb_eArgError, "Value must be hash, string, or array of strings.");
			xmmsv_list_append_string (elem, StringValuePtr (entry));
		}
	} else {
		rb_raise (rb_eArgError, "Value must be hash, string, or array of strings.");
	}

	return 0;
}

static VALUE
c_coll_query_fragile (VALUE args)
{
	VALUE xmms, coll, spec, cspec;

	Check_Type(args, T_ARRAY);
	rb_scan_args(RARRAY_LEN (args), RARRAY_PTR (args), "4",
	             &xmms, &coll, &spec, &cspec);

	rb_hash_foreach (spec, parse_fetch_spec_foreach, cspec);

	return (VALUE) xmmsc_coll_query (((RbXmmsClient *) xmms)->real,
	                                 FROM_XMMS_CLIENT_COLLECTION (coll),
	                                 (xmmsv_t *) cspec);
}

static VALUE
c_coll_query_cleanup (VALUE args)
{
	xmmsv_unref ((xmmsv_t *) args);
	return Qnil;
}

/*
 * call-seq:
 * xc.coll_query(coll, spec) -> result
 *
 */
static VALUE
c_coll_query (int argc, VALUE *argv, VALUE self)
{
	VALUE coll, spec;
	xmmsv_t *cspec;
	METHOD_HANDLER_HEADER

	rb_scan_args (argc, argv, "2", &coll, &spec);

	cspec = xmmsv_new_dict ();

	VALUE args = rb_ary_new3(4, xmms, coll, spec, (VALUE) cspec);

	res = (xmmsc_result_t *) rb_ensure (c_coll_query_fragile, args,
	                                    c_coll_query_cleanup, (VALUE) cspec);

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 * xc.coll_query_ids(coll, [order], [start], [len]) -> result
 *
 * Retrieves a list of all the ids of media matched by the collection. _order_
 * specifies a list of properties to order by or no order if omitted. _start_
 * and _len_ determine the offset at which to start retrieving ids and the
 * maximum number of ids to retrieve, respectively.
 */
static VALUE
c_coll_query_ids (int argc, VALUE *argv, VALUE self)
{
	VALUE coll, order = Qnil, start, len = Qnil;
	xmmsv_t *corder = NULL;
	METHOD_HANDLER_HEADER

	rb_scan_args (argc, argv, "13", &coll, &order, &start, &len);

	if (!NIL_P (order))
		corder = parse_string_array2 (order);

	res = xmmsc_coll_query_ids (xmms->real,
	                            FROM_XMMS_CLIENT_COLLECTION (coll),
	                            corder,
	                            NIL_P (start) ? 0 : NUM2INT (start),
	                            NIL_P (start) ? 0 : NUM2INT (len));

	if (corder)
		xmmsv_unref (corder);

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 * xc.coll_query_info(coll, fetch, [order], [start], [len], [group]) -> result
 *
 * Retrieves media info of media matched by the collection. _fetch_ should
 * contain an array of properties to retrieve from the collection. _order_
 * specifies a list of properties to order by or no order if omitted. _start_
 * and _len_ determine the offset at which to start retrieving info and the
 * maximum number of ids to retrieve, respectively. _group_ defines a list
 * of properties to group by or no grouping if omitted.
 */
static VALUE
c_coll_query_info (int argc, VALUE *argv, VALUE self)
{
	VALUE coll, order = Qnil, start, len, fetch, group = Qnil;
	xmmsv_t *cfetch = NULL, *corder = NULL, *cgroup = NULL;
	METHOD_HANDLER_HEADER

	rb_scan_args (argc, argv, "24", &coll, &fetch, &order, &start, &len,
	              &group);

	cfetch = parse_string_array2 (fetch);

	if (!NIL_P (order))
		corder = parse_string_array2 (order);

	if (!NIL_P (group))
		cgroup = parse_string_array2 (group);

	res = xmmsc_coll_query_infos (xmms->real,
	                            FROM_XMMS_CLIENT_COLLECTION (coll),
	                            corder,
	                            NIL_P (start) ? 0 : NUM2INT (start),
	                            NIL_P (start) ? 0 : NUM2INT (len),
	                            cfetch,
	                            cgroup);
	xmmsv_unref (cfetch);
	if (corder) {
		xmmsv_unref (corder);
	}
	if (cgroup) {
		xmmsv_unref (cgroup);
	}

	METHOD_HANDLER_FOOTER
}

/*
 * call-seq:
 * xc.coll_idlist_from_playlist_file(path) -> result
 *
 * Returns a collection of the idlist type from the playlist file at _path_.
 * _path_ must be an unencoded string.
 */
static VALUE
c_coll_idlist_from_playlist_file (VALUE self, VALUE path)
{
	METHOD_ADD_HANDLER_STR (coll_idlist_from_playlist_file, path)
}

/*
 * call-seq:
 * xc.broadcast_coll_changed -> result
 *
 * The collection_changed broadcast, if requested, is called anytime a
 * collection is altered.
 */
static VALUE
c_broadcast_coll_changed (VALUE self)
{
	METHOD_ADD_HANDLER (broadcast_collection_changed)
}

VALUE
check_int32 (VALUE arg)
{
	VALUE int32_max = INT2NUM (INT_MAX);
	VALUE int32_min = INT2NUM (INT_MIN);

	if (!rb_obj_is_kind_of (arg, rb_cInteger))
		rb_raise (rb_eTypeError,
		          "wrong argument type %s (expected Integer)",
		          rb_obj_classname (arg));

	if (rb_funcall2 (arg, id_lt, 1, &int32_min) ||
	    rb_funcall2 (arg, id_gt, 1, &int32_max))
		rb_raise (rb_eTypeError,
		          "wrong argument type (expected 32 bit signed int)");

	return NUM2INT (arg);
}

const char **
parse_string_array (VALUE value)
{
	const char **ret;
	int i;

	if (!NIL_P (rb_check_array_type (value))) {
		VALUE *ary = RARRAY_PTR (value);
		int ary_len = RARRAY_LEN (value);

		ret = malloc (sizeof (char *) * (ary_len + 1));

		for (i = 0; i < ary_len; i++)
			ret[i] = StringValuePtr (ary[i]);

		ret[i] = NULL;
	} else {
		/* if it's not an array, it must be a string */
		StringValue (value);

		ret = malloc (sizeof (char *) * 2);
		ret[0] = StringValuePtr (value);
		ret[1] = NULL;
	}

	return ret;
}

xmmsv_t *
parse_string_array2 (VALUE value)
{
	xmmsv_t *list;


	list = xmmsv_new_list ();

	if (!NIL_P (rb_check_array_type (value))) {
		VALUE *ary = RARRAY_PTR (value);
		int i, ary_len = RARRAY_LEN (value);

		for (i = 0; i < ary_len; i++) {
			xmmsv_t *elem;

			elem = xmmsv_new_string (StringValuePtr (ary[i]));
			xmmsv_list_append (list, elem);
			xmmsv_unref (elem);
		}
	} else {
		xmmsv_t *elem;

		elem = xmmsv_new_string (StringValuePtr (value));
		xmmsv_list_append (list, elem);
		xmmsv_unref (elem);
	}

	return list;
}

void
Init_Client (VALUE mXmms)
{
#if RDOC_CAN_PARSE_DOCUMENTATION
	mXmms= rb_define_module("Xmms");
#endif

	VALUE c;

	c = rb_define_class_under (mXmms, "Client", rb_cObject);

	rb_define_alloc_func (c, c_alloc);
	rb_define_method (c, "initialize", c_init, 1);
	rb_define_method (c, "connect", c_connect, -1);
	rb_define_method (c, "delete!", c_delete, 0);
	rb_define_method (c, "on_disconnect", c_on_disconnect, 0);
	rb_define_method (c, "last_error", c_last_error_get, 0);

	rb_define_method (c, "io_fd", c_io_fd, 0);
	rb_define_method (c, "io_want_out", c_io_want_out, 0);
	rb_define_method (c, "io_on_need_out", c_io_on_need_out, 0);
	rb_define_method (c, "io_in_handle", c_io_in_handle, 0);
	rb_define_method (c, "io_out_handle", c_io_out_handle, 0);
	rb_define_method (c, "io_disconnect", c_io_disconnect, 0);

	rb_define_method (c, "quit", c_quit, 0);
	rb_define_method (c, "broadcast_quit", c_broadcast_quit, 0);

	rb_define_method (c, "playback_start", c_playback_start, 0);
	rb_define_method (c, "playback_pause", c_playback_pause, 0);
	rb_define_method (c, "playback_stop", c_playback_stop, 0);
	rb_define_method (c, "playback_tickle", c_playback_tickle, 0);
	rb_define_method (c, "broadcast_playback_status",
	                  c_broadcast_playback_status, 0);
	rb_define_method (c, "playback_status", c_playback_status, 0);
	rb_define_method (c, "playback_playtime", c_playback_playtime, 0);
	rb_define_method (c, "signal_playback_playtime",
	                  c_signal_playback_playtime, 0);
	rb_define_method (c, "playback_current_id",
	                  c_playback_current_id, 0);
	rb_define_method (c, "broadcast_playback_current_id",
	                  c_broadcast_playback_current_id, 0);
	rb_define_method (c, "playback_seek_ms", c_playback_seek_ms, 1);
	rb_define_method (c, "playback_seek_ms_rel", c_playback_seek_ms_rel, 1);
	rb_define_method (c, "playback_seek_samples",
	                  c_playback_seek_samples, 1);
	rb_define_method (c, "playback_seek_samples_rel",
	                  c_playback_seek_samples_rel, 1);
	rb_define_method (c, "playback_volume_set",
	                  c_playback_volume_set, 2);
	rb_define_method (c, "playback_volume_get",
	                  c_playback_volume_get, 0);
	rb_define_method (c, "broadcast_playback_volume_changed",
	                  c_broadcast_playback_volume_changed, 0);

	rb_define_method (c, "broadcast_playlist_changed",
	                  c_broadcast_playlist_changed, 0);
	rb_define_method (c, "broadcast_playlist_current_pos",
	                  c_broadcast_playlist_current_pos, 0);
	rb_define_method (c, "broadcast_playlist_loaded",
	                  c_broadcast_playlist_loaded, 0);
	rb_define_method (c, "broadcast_medialib_entry_changed",
	                  c_broadcast_medialib_entry_changed, 0);
	rb_define_method (c, "broadcast_medialib_entry_updated",
	                  c_broadcast_medialib_entry_updated, 0);
	rb_define_method (c, "broadcast_medialib_entry_added",
	                  c_broadcast_medialib_entry_added, 0);
	rb_define_method (c, "broadcast_medialib_entry_removed",
	                  c_broadcast_medialib_entry_removed, 0);

	rb_define_method (c, "playlist", c_playlist, -1);
	rb_define_method (c, "playlist_list", c_playlist_list, 0);
	rb_define_method (c, "playlist_current_active",
	                  c_playlist_current_active, 0);
	rb_define_method (c, "playlist_set_next", c_playlist_set_next, 1);
	rb_define_method (c, "playlist_set_next_rel", c_playlist_set_next_rel, 1);

	rb_define_method (c, "coll_list", c_coll_list, -1);
	rb_define_method (c, "coll_get", c_coll_get, -1);
	rb_define_method (c, "coll_save", c_coll_save, 3);
	rb_define_method (c, "coll_remove", c_coll_remove, -1);
	rb_define_method (c, "coll_find", c_coll_find, 2);
	rb_define_method (c, "coll_rename", c_coll_rename, -1);
	rb_define_method (c, "coll_query", c_coll_query, -1);
	rb_define_method (c, "coll_query_ids", c_coll_query_ids, -1);
	rb_define_method (c, "coll_query_info", c_coll_query_info, -1);
	rb_define_method (c, "coll_idlist_from_playlist_file",
	                  c_coll_idlist_from_playlist_file, 1);
	rb_define_method (c, "broadcast_coll_changed", c_broadcast_coll_changed, 0);

	rb_define_method (c, "medialib_add_entry", c_medialib_add_entry, 1);
	rb_define_method (c, "medialib_get_id", c_medialib_get_id, 1);
	rb_define_method (c, "medialib_get_info", c_medialib_get_info, 1);
	rb_define_method (c, "medialib_entry_property_set",
	                  c_medialib_entry_property_set, -1);
	rb_define_method (c, "medialib_entry_property_remove",
	                  c_medialib_entry_property_remove, -1);
	rb_define_method (c, "medialib_entry_remove", c_medialib_entry_remove, 1);
	rb_define_method (c, "medialib_entry_move", c_medialib_entry_move, 2);
	rb_define_method (c, "medialib_path_import", c_medialib_path_import, 1);
	rb_define_method (c, "medialib_path_import_encoded", c_medialib_path_import_encoded, 1);
	rb_define_method (c, "medialib_rehash", c_medialib_rehash, 1);

	rb_define_method (c, "xform_media_browse", c_xform_media_browse, 1);

	rb_define_method (c, "broadcast_mediainfo_reader_status",
	                  c_broadcast_mediainfo_reader_status, 0);
	rb_define_method (c, "signal_mediainfo_reader_unindexed",
	                  c_signal_mediainfo_reader_unindexed, 0);

	rb_define_method (c, "plugin_list", c_plugin_list, -1);
	rb_define_method (c, "main_stats", c_main_stats, 0);

	rb_define_method (c, "config_list_values", c_config_list_values, 0);
	rb_define_method (c, "config_get_value", c_config_get_value, 1);
	rb_define_method (c, "config_set_value", c_config_set_value, 2);
	rb_define_method (c, "config_register_value", c_config_register_value, 2);
	rb_define_method (c, "broadcast_config_value_changed",
	                  c_broadcast_config_value_changed, 0);

	rb_define_method (c, "bindata_add", c_bindata_add, 1);
	rb_define_method (c, "bindata_retrieve", c_bindata_retrieve, 1);
	rb_define_method (c, "bindata_remove", c_bindata_remove, 1);
	rb_define_method (c, "bindata_list", c_bindata_list, 0);

	rb_define_const (c, "PLAY",
	                 INT2FIX (XMMS_PLAYBACK_STATUS_PLAY));
	rb_define_const (c, "STOP",
	                 INT2FIX (XMMS_PLAYBACK_STATUS_STOP));
	rb_define_const (c, "PAUSE",
	                 INT2FIX (XMMS_PLAYBACK_STATUS_PAUSE));

	rb_define_const (c, "IDLE",
	                 INT2FIX (XMMS_MEDIAINFO_READER_STATUS_IDLE));
	rb_define_const (c, "RUNNING",
	                 INT2FIX (XMMS_MEDIAINFO_READER_STATUS_RUNNING));

	rb_define_const (c, "ALL_PLUGINS", INT2FIX (XMMS_PLUGIN_TYPE_ALL));
	rb_define_const (c, "XFORM", INT2FIX (XMMS_PLUGIN_TYPE_XFORM));
	rb_define_const (c, "OUTPUT", INT2FIX (XMMS_PLUGIN_TYPE_OUTPUT));

	rb_define_const (c, "ENTRY_STATUS_NEW", INT2FIX (XMMS_MEDIALIB_ENTRY_STATUS_NEW));
	rb_define_const (c, "ENTRY_STATUS_OK", INT2FIX (XMMS_MEDIALIB_ENTRY_STATUS_OK));
	rb_define_const (c, "ENTRY_STATUS_RESOLVING", INT2FIX (XMMS_MEDIALIB_ENTRY_STATUS_RESOLVING));
	rb_define_const (c, "ENTRY_STATUS_NOT_AVAILABLE", INT2FIX (XMMS_MEDIALIB_ENTRY_STATUS_NOT_AVAILABLE));
	rb_define_const (c, "ENTRY_STATUS_REHASH", INT2FIX (XMMS_MEDIALIB_ENTRY_STATUS_REHASH));

	eClientError = rb_define_class_under (c, "ClientError",
	                                      rb_eStandardError);
	eDisconnectedError = rb_define_class_under (c, "DisconnectedError",
	                                            eClientError);

	id_lt = rb_intern ("<");
	id_gt = rb_intern (">");

	Init_Result (mXmms);
	cPlaylist = Init_Playlist (mXmms);
	Init_Collection (mXmms);
}
