#include "perl_xmmsclient.h"

STATIC void
disconnect_callback_set_cb (void *userdata)
{
	PerlXMMSClientCallback *cb = (PerlXMMSClientCallback *)userdata;

	perl_xmmsclient_callback_invoke (cb, NULL);
}

STATIC void
perl_xmmsclient_xmmsc_io_need_out_callback_set_cb (int flag, void *userdata)
{
	PerlXMMSClientCallback *cb = (PerlXMMSClientCallback *)userdata;

	perl_xmmsclient_callback_invoke (cb, NULL);
}

MODULE = Audio::XMMSClient	PACKAGE = Audio::XMMSClient	PREFIX = xmmsc_

=head1 NAME

Audio::XMMSClient - Interface to the xmms2 music player

=head1 SYNOPSIS

  use Audio::XMMSClient;

  my $conn = Audio::XMMSClient->new($client_name);
  $conn->connect or die $c->get_last_error;

  my $result = $c->playback_status;
  $result->wait;
  print $result->value;

=head1 DESCRIPTION

This module provides a perl interface to the xmms2 client library.

=head1 METHODS

=cut

## General

=head2 new

=over 4

=item Arguments: $client_name?

=item Return Value: $connection

=back

  my $conn = Audio::XMMSClient->new('foo');

Creates a new Audio::XMMSClient instance. If C<$client_name> is omitted it will
use the name of your application (see C<$0>/C<$PROGRAM_NAME> in L<perlvar>).

=cut

SV *
new (class, clientname=NULL)
		const char *class
		const char *clientname
	PREINIT:
		xmmsc_connection_t *con;
	INIT:
		if (!clientname) {
			clientname = SvPV_nolen (get_sv ("0", 0));
		}
	CODE:
		con = xmmsc_init (clientname);

		if (con == NULL) {
			RETVAL = &PL_sv_undef;
		}
		else {
			RETVAL = perl_xmmsclient_new_sv_from_ptr (con, class);
		}
	OUTPUT:
		RETVAL

=head2 connect

=over 4

=item Arguments: $ipcpath?

=item Return Value: $success

=back

  my $success = $conn->connect;

Tries to establish a connection to the xmms2 server. If C<$ipcpath> is omitted
it will fall back to C<$ENV{XMMS_PATH}> and, if that's unset as well, the
default ipcpath of libxmmsclient. If an error occurs a false value is returned
and a message describing the error can be obtained using L</get_last_error>.

=cut

int
xmmsc_connect (c, ipcpath=NULL)
		xmmsc_connection_t *c
		const char *ipcpath
	OUTPUT:
		RETVAL


=head2 disconnect_callback_set

=over 4

=item Arguments: \&func, $data?

=item Return Value: none

=back

  $conn->disconnect_callback_set(sub { die 'got disconnected' });

Registers a function which will be called if the connection to the xmms2 server
gets abnormally terminated. C<\&func> will be called with either one or two
arguments. The first one will be a reference to the connection. C<$data>, if
passed, will be the second argument of the callback.

=cut

void
xmmsc_disconnect_callback_set (c, func, data=NULL)
		xmmsc_connection_t *c
		SV *func
		SV *data
	PREINIT:
		PerlXMMSClientCallback *cb = NULL;
		PerlXMMSClientCallbackParamType param_types[1];
	CODE:
		param_types[0] = PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_CONNECTION;

		cb = perl_xmmsclient_callback_new (func, data, ST(0), 1, param_types,
		                                   PERL_XMMSCLIENT_CALLBACK_RETURN_TYPE_NONE);

		xmmsc_disconnect_callback_set_full (c, disconnect_callback_set_cb, cb,
		                                    (xmmsc_user_data_free_func_t)perl_xmmsclient_callback_destroy);

=head2 io_disconnect

=over 4

=item Arguments: none

=item Return Value: none

=back

  $conn->io_disconnect;

Flags the connection as disconected. This is to be called when the mainloop
signals disconnection of the connection. This is optional, any call to
L</io_out_handle> or L</io_in_handle> will notice the disconnection and handle
it accordingly.

=cut

void
xmmsc_io_disconnect (c)
		xmmsc_connection_t *c

=head2 get_last_error

=over 4

=item Arguments: none

=item Return Value: $error_message

=back

  my $message = $conn->get_last_error;

Returns a string that descibes the last error (if any).

=cut

char *
xmmsc_get_last_error (c)
		xmmsc_connection_t *c

=head2 main_list_plugins

=over 4

=item Arguments: $type?

=item Return Value: $result

=back

  my $result = $conn->main_list_plugins;

Get a list of loaded plugins from the server. C<$type>, which may be used to
only get a list of plugins of a specific type, can be any of the following
strings:

=over 4

=item * output

=item * playlist

=item * effect

=item * xform

=item * all

=back

If C<$type> is omitted "all" is assumed.

=cut

xmmsc_result_t *
xmmsc_main_list_plugins (c, type=XMMS_PLUGIN_TYPE_ALL)
		xmmsc_connection_t *c
		xmms_plugin_type_t type

=head2 main_stats

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->main_stats;

Get a list of statistics from the server.

=cut

xmmsc_result_t *
xmmsc_main_stats (c)
		xmmsc_connection_t *c

=head2 quit

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->quit;

Tell the server to quit. This will terminate the server. If you only want to
disconnect just destroy all references to the connection instance.

=cut

xmmsc_result_t *
xmmsc_quit (c)
		xmmsc_connection_t *c

=head2 broadcast_quit

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_quit;

Request the quit broadcast. Will be called when the server is terminating.

=cut

xmmsc_result_t *
xmmsc_broadcast_quit (c)
		xmmsc_connection_t *c


## Medialib

=head2 medialib_get_id

=over 4

=item Arguments: $url

=item Return Value: $result

=back

  my $result = $conn->medialib_get_id('file:///home/user/music/artist/album/track.flac');

Search for a entry (URL) in the medialib db and return its ID number.

=cut

xmmsc_result_t *
xmmsc_medialib_get_id (c, url)
		xmmsc_connection_t *c
		const char *url

=head2 medialib_move_entry

=over 4

=item Arguments: $id, $url

=item Return Value: $result

  my $result = $conn->medialib_move_entry(42, 'file:///new/path/to/file.flac');

Change the url property of an entry in the media library. Note that you need to
handle the actual file move yourself.

=back

=cut

xmmsc_result_t *
xmmsc_medialib_move_entry (c, id, url)
		xmmsc_connection_t *c
		uint32_t id
		const char *url

=head2 medialib_remove_entry

=over 4

=item Arguments: $entry

=item Return Value: $result

=back

  my $result = $conn->medialib_remove_entry(1337);

Remove a entry with a given ID from the medialib.

=cut

xmmsc_result_t *
xmmsc_medialib_remove_entry (c, entry)
		xmmsc_connection_t *c
		int entry

=head2 medialib_add_entry

=over 4

=item Arguments: $url

=item Return Value: $result

=back

  my $result = $conn->medialib_add_entry;

Add a C<$url> to the medialib. If you want to add mutiple files you should call
L</medialib_path_import>.

=cut

xmmsc_result_t *
xmmsc_medialib_add_entry (c, url)
		xmmsc_connection_t *c
		const char *url

=head2 medialib_add_entry_args

=over 4

=item Arguments: $url, @args?

=item Return Value: $result

=back

  my $result = $conn->medialib_add_entry_args(
          "file:///data/HVSC/C64Music/Hubbard_Rob/Commando.sid",
          "subtune=2",
  );

Add a C<$url> with arguments to the medialib.

=cut

xmmsc_result_t *
xmmsc_medialib_add_entry_args (c, url, ...)
		xmmsc_connection_t *c
		const char *url
	PREINIT:
		int i;
		int nargs;
		const char **args;
	INIT:
		nargs = items - 2;
		args = (const char **)malloc (sizeof (char *) * nargs);

		for (i = 2; i < items; i++) {
			args[i] = SvPV_nolen (ST (i));
		}
	C_ARGS:
		c, url, nargs, args
	CLEANUP:
		free (args);

=head2 medialib_add_entry_encoded

=over 4

=item Arguments: $url

=item Return Value: $result

=back

  my $result = $conn->medialib_add_entry_encoded($url);

Same as L</medialib_add_entry>, except it expects an encoded C<$url>.

=cut

xmmsc_result_t *
xmmsc_medialib_add_entry_encoded (c, url)
		xmmsc_connection_t *c
		const char *url

=head2 playlist

=over 4

=item Arguments: $playlist_name?

=item Return Value: $playlist

=back

  my $playlist = $conn->playlist('jrock');

Returns an L<Audio::XMMSClient::Playlist> instance representing a playlist
specified by C<$playlist_name>. If no C<$playlist_name> is given "_active" will
be used.

=cut

perl_xmmsclient_playlist_t *
playlist(c, playlist=XMMS_ACTIVE_PLAYLIST)
		xmmsc_connection_t *c
		const char *playlist
	CODE:
		RETVAL = perl_xmmsclient_playlist_new (c, playlist);
	OUTPUT:
		RETVAL

=head2 medialib_import_path

=over 4

=item Arguments: $path

=item Return Value: $result

=back

  my $result = $conn->medialib_import_path('file:///media/music/');

Import a all files recursivly from C<$path>. C<$path> must include the
protocol, i.e. file://.

=cut

xmmsc_result_t *
xmmsc_medialib_import_path (c, path)
		xmmsc_connection_t *c
		const char *path
	ALIAS:
		Audio::XMMSClient::medialib_path_import = 1
	INIT:
		if (ix == 1)
			warn ("Audio::XMMSClient::medialib_path_import is deprecated, use Audio::XMMSClient::medialib_import_path instead.");

=head2 medialib_import_path_encoded

=over 4

=item Arguments: $path

=item Return Value: $result

=back

  my $result = $conn->medialib_import_path_encoded($path);

Same as L</medialib_import_path> except it expects C<$path> to be url encoded.
You probably want to use L</medialib_import_path> unless you want to add a path
that comes as a result from the daemon, such as from C</xform_media_browse>.

=cut

xmmsc_result_t *
xmmsc_medialib_import_path_encoded (c, path)
		xmmsc_connection_t *c
		const char *path
	ALIAS:
		Audio::XMMSClient::medialib_path_import_encoded = 1
	INIT:
		if (ix == 1)
			warn ("Audio::XMMSClient::medialib_path_import_encoded is deprecated, use Audio::XMMSClient::medialib_import_path_encoded instead.");

=head2 medialib_rehash

=over 4

=item Arguments: $id?

=item Return Value: $result

=back

  my $result = $conn->medialib_rehash;

Rehash the medialib. This will check that data for entry C<$id> in the medialib
still is the same as in its data in files. If C<$id> is omitted or set to 0 the
full medialib will be rehashed.

=cut

xmmsc_result_t *
xmmsc_medialib_rehash (c, id=0)
		xmmsc_connection_t *c
		uint32_t id

=head2 medialib_get_info

=over 4

=item Arguments: $id

=item Return Value: $result

=back

  my $result = $conn->medialib_get_info(9667);

Retrieve information about entry C<$id> from the medialib.

=cut

xmmsc_result_t *
xmmsc_medialib_get_info (c, id)
		xmmsc_connection_t *c
		uint32_t id

=head2 broadcast_medialib_entry_added

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_medialib_entry_added;

Request the medialib_entry_added broadcast. This will be called if a new entry
is added to the medialib serverside.

=cut

xmmsc_result_t *
xmmsc_broadcast_medialib_entry_added (c)
		xmmsc_connection_t *c

=head2 broadcast_medialib_entry_updated

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_medialib_entry_changed;

Request the medialib_entry_changed broadcast. This will be called if a entry
changes on the serverside.

=cut

xmmsc_result_t *
xmmsc_broadcast_medialib_entry_updated (c)
		xmmsc_connection_t *c
	ALIAS:
		Audio::XMMSClient::broadcast_medialib_entry_changed = 1
	INIT:
		if (ix == 1)
			warn ("Audio::XMMSClient::broadcast_medialib_entry_changed is deprecated, use Audio::XMMSClient::broadcast_medialib_entry_updated instead.");

=head2 medialib_entry_property_set_int

=over 4

=item Arguments: $id, $key, $value

=item Return Value: $result

=back

  my $result = $conn->medialib_entry_property_set_int(1337, 'leet_level', 42);

Associate an integer C<$value> with a property C<$key> for medialib entry
C<$id>. Uses default source which is C<client/E<lt>clientnameE<gt>>.

=cut

xmmsc_result_t *
xmmsc_medialib_entry_property_set_int (c, id, key, value)
		xmmsc_connection_t *c
		uint32_t id
		const char *key
		int value

=head2 medialib_entry_property_set_int_with_source

=over 4

=item Arguments: $source, $id, $key, $valu

=item Return Value: $result

=back

  my $result = $conn->medialib_entry_property_set_int_with_source(9667, 'client/generic', 'rating', 3);

Same as L</medialib_entry_property_set_int>, except it also allows to set the
C<$source> for the given property.

=cut

xmmsc_result_t *
xmmsc_medialib_entry_property_set_int_with_source (c, id, source, key, value)
		xmmsc_connection_t *c
		uint32_t id
		const char *source
		const char *key
		int value

=head2 medialib_entry_property_set_str

=over 4

=item Arguments: $id, $key, $value

=item Return Value: $result

=back

  my $result = $conn->medialib_entry_property_set_str(431, 'fingerprint', '13f3ad');

Same as L</medialib_entry_property_set_int>, except it sets a string C<$value>.

=cut

xmmsc_result_t *
xmmsc_medialib_entry_property_set_str (c, id, key, value)
		xmmsc_connection_t *c
		uint32_t id
		const char *key
		const char *value

=head2 medialib_entry_property_set_str_with_source

=over 4

=item Arguments: $id, $source, $key, $value

=item Return Value: $result

=back

  my $result = $conn->medialib_entry_property_set_str_with_source(542, 'client/generic', 'lyrics', <<'EOL');
  Hey, Ho, Supergaul..
  ...
  EOL

Same as L</medialib_entry_property_set_str>, except it also allows to set the
C<$source> for the given property.

=cut

xmmsc_result_t *
xmmsc_medialib_entry_property_set_str_with_source (c, id, source, key, value)
		xmmsc_connection_t *c
		uint32_t id
		const char *source
		const char *key
		const char *value

=head2 medialib_entry_property_remove

=over 4

=item Arguments: $id, $key

=item Return Value: $result

=back

  my $result = $conn->medialib_entry_property_remove(431, 'fingerprint');

Remove a custom field specified by C<$key> in the medialib associated with the
entry C<$id>.

=cut

xmmsc_result_t *
xmmsc_medialib_entry_property_remove (c, id, key)
		xmmsc_connection_t *c
		uint32_t id
		const char *key

=head2 medialib_entry_property_remove_with_source

=over 4

=item Arguments: $id, $source, $key

=item Return Value: $result

=back

  my $result = $conn->medialib_entry_property_remove_with_source(542, 'client/generic', 'lyrics');

Like L</medialib_entry_property_remove>, but also allows to specify the
C<$source>.

=cut

xmmsc_result_t *
xmmsc_medialib_entry_property_remove_with_source (c, id, source, key)
		xmmsc_connection_t *c
		uint32_t id
		const char *source
		const char *key


## Collections

=head2 coll_get

=over 4

=item Arguments: $name, $namespace

=item Return Value: $collection

=back

  my $coll = $conn->coll_get('Funk', 'Collections');

Get the collection structure of the collection C<$name> saved on the server in
a given C<$namespace>.

=cut

xmmsc_result_t *
xmmsc_coll_get (c, collname, namespace)
		xmmsc_connection_t *c
		const char *collname
		xmmsv_coll_namespace_t namespace

=head2 coll_sync

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->coll_sync;

Synchronize collection data to the database.

=cut

xmmsc_result_t *
xmmsc_coll_sync (c)
		xmmsc_connection_t *c

=head2 coll_list

=over 4

=item Arguments: $namespace

=item Return Value: $result

=back

  my $result = $conn->coll_list('Collections');

List all collections saved on the server in a given C<$namespace>.

=cut

xmmsc_result_t *
xmmsc_coll_list (c, namespace)
		xmmsc_connection_t *c
		xmmsv_coll_namespace_t namespace

=head2 coll_save

=over 4

=item Arguments: $coll, $name, $namespace

=item Return Value: $result

=back

  my $result = $conn->coll_save('Funk', $collection, 'Collections');

Saves a C<$collection> on the server under a given C<$name>, in a given
C<$namespace>.

=cut

xmmsc_result_t *
xmmsc_coll_save (c, coll, name, namespace)
		xmmsc_connection_t *c
		xmmsv_t *coll
		const char *name
		xmmsv_coll_namespace_t namespace

=head2 coll_remove

=over 4

=item Arguments: $name, $namespace

=item Return Value: $result

=back

  my $result = $conn->coll_remove('Funk', 'Collections');

Remove a collection from the server.

=cut

xmmsc_result_t *
xmmsc_coll_remove (c, name, namespace)
		xmmsc_connection_t *c
		const char *name
		xmmsv_coll_namespace_t namespace

=head2 coll_find

=over 4

=item Arguments: $mediaid, $namespace

=item Return Value: $result

=back

  my $result = $conn->coll_find(542, 'Collections');

Find all collections in a given C<$namespace> which contain the medialib entry
with the given C<$mediaid>.

=cut

xmmsc_result_t *
xmmsc_coll_find (c, mediaid, namespace)
		xmmsc_connection_t *c
		unsigned int mediaid
		xmmsv_coll_namespace_t namespace

=head2 coll_rename

=over 4

=item Arguments: $from, $to, $namespace

=item Return Value: $result

=back

  my $result = $conn->coll_rename('Funk', 'Funky Music', 'Collections');

Rename a collection within a C<$namespace>.

=cut

xmmsc_result_t *
xmmsc_coll_rename (c, from, to, namespace)
		xmmsc_connection_t *c
		char *from
		char *to
		xmmsv_coll_namespace_t namespace

=head2 coll_query_ids

=over 4

=item Arguments: $collection, \%args

=item Arguments: $collection, \@order?, $limit_start?, $limit_len?

=item Return Value: $result

=back

  my $result = $conn->coll_query_ids($collection, {
          order       => [qw/artist album/],
          limit_start => 0,
          limit_len   => 10,
  });

List the ids of all media matched by a given C<$collection>. The returned list
might be refined using the following parameters:

=over 4

=item * order

The list of properties to order by. C<undef> or an empty array reference to
disable.

=item * limit_start

The offset at which to start retrieving results. C<0> to disable.

=item * limit_len

The maximum number of entries to retrieve. C<0> to disable.

=back

The above parameters might be passed either positional or within a hash
reference.

=cut

xmmsc_result_t *
xmmsc_coll_query_ids (c, coll, ...)
		xmmsc_connection_t *c
		xmmsv_t *coll
	INIT:
		xmmsv_t *order = NULL;
		unsigned int limit_start = 0;
		unsigned int limit_len = 0;
		HV *args;
		SV *val;

		if (items == 3 && SvROK (ST (2)) && (SvTYPE (SvRV (ST (2))) == SVt_PVHV)) {
			args = (HV *)SvRV (ST (2));

			if ((val = perl_xmmsclient_hv_fetch (args, "order", 5))) {
				order = perl_xmmsclient_pack_stringlist (val);
			}

			if ((val = perl_xmmsclient_hv_fetch (args, "limit_start", 11))) {
				limit_start = SvUV (val);
			}

			if ((val = perl_xmmsclient_hv_fetch (args, "limit_len", 9))) {
				limit_len = SvUV (val);
			}
		}
		else {
			order = perl_xmmsclient_pack_stringlist (ST (2));
			limit_start = SvOK (ST (3)) ? SvUV (ST (3)) : 0;
			limit_len = SvOK (ST (4)) ? SvUV (ST (4)) : 0;
		}
	C_ARGS:
		c, coll, order, limit_start, limit_len
	CLEANUP:
		xmmsv_unref (order);

=head2 coll_query_infos

=over 4

=item Arguments: $collection, \%args

=item Arguments: $collection, \@order?, $limit_start?, $limit_len?, \@fetch?, \@group?

=item Return Value: $result

=back

  my $result = $conn->coll_query_infos($collection, {
          order       => [qw/artist/],
          limit_start => 0,
          limit_len   => 10,
          fetch       => [qw/artist/],
          group       => [qw/artist/],
  });

List the properties of all media matched by the given collection. The returned
list might be refined using the following parameters:

=over 4

=item * order

The list of properties to order by. C<undef> or an empty array reference to
disable.

=item * limit_start

The offset at which to start retrieving results. C<0> to disable.

=item * limit_len

The maximum number of entries to retrieve. C<0> to disable.

=item * fetch

The list of properties to retrieve (at least one property required).

=item * group

The list of properties to group by. C<undef> or an empty array reference to
disable.

=back

The above parameters might be passed either positional or within a hash
reference.

=cut

xmmsc_result_t *
xmmsc_coll_query_infos (c, coll, ...)
		xmmsc_connection_t *c
		xmmsv_t *coll
	INIT:
		xmmsv_t *order = NULL;
		unsigned int limit_start = 0;
		unsigned int limit_len = 0;
		xmmsv_t *fetch = NULL;
		xmmsv_t *group = NULL;
		HV *args;
		SV *val;

		if (items == 3 && SvROK (ST (2)) && (SvTYPE (SvRV (ST (2))) == SVt_PVHV)) {
			args = (HV *)SvRV (ST (2));

			if ((val = perl_xmmsclient_hv_fetch (args, "order", 5))) {
				order = perl_xmmsclient_pack_stringlist (val);
			}

			if ((val = perl_xmmsclient_hv_fetch (args, "fetch", 5))) {
				fetch = perl_xmmsclient_pack_stringlist (val);
			}

			if ((val = perl_xmmsclient_hv_fetch (args, "group", 5))) {
				group = perl_xmmsclient_pack_stringlist (val);
			}

			if ((val = perl_xmmsclient_hv_fetch (args, "limit_start", 11))) {
				limit_start = SvUV (val);
			}

			if ((val = perl_xmmsclient_hv_fetch (args, "limit_len", 9))) {
				limit_len = SvUV (val);
			}
		}
		else {
			order = perl_xmmsclient_pack_stringlist (ST (2));
			limit_start = SvOK (ST (3)) ? SvUV (ST (3)) : 0;
			limit_len = SvOK (ST (4)) ? SvUV (ST (4)) : 0;
			fetch = perl_xmmsclient_pack_stringlist (ST (5));
			group = perl_xmmsclient_pack_stringlist (ST (6));
		}
	C_ARGS:
		c, coll, order, limit_start, limit_len, fetch, group
	CLEANUP:
		if (order != NULL)
			xmmsv_unref (order);
		if (fetch != NULL)
			xmmsv_unref (fetch);
		if (group != NULL)
			xmmsv_unref (group);

=head2 coll_query

=over 4

=item Arguments: $collection, \@spec

=item Return Value: $result

=back

my $result = $conn->coll_query($collection, \@spec);

List the properties of all media matched by the given collection. The structure
of the return value is determined by the fetch specification.

=over 4

=item * spec

A collections fetch specification.

=back

The above parameters might be passed either positional or within a hash
reference.

=cut

xmmsc_result_t *
xmmsc_coll_query (c, coll, spec)
		xmmsc_connection_t *c
		xmmsv_t *coll
	INIT:
		xmmsv_t *spec = perl_xmmsclient_pack_fetchspec (ST (2));
	C_ARGS:
		c, coll, spec
	CLEANUP:
		xmmsv_unref (spec);

=head2 broadcast_collection_changed

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_collection_changed;

Request the collection changed broadcast from the server. Everytime someone
manipulates a collection this will be emitted.

=cut

xmmsc_result_t *
xmmsc_broadcast_collection_changed (c)
		xmmsc_connection_t *c

## XForm

=head2 xform_media_browse

=over 4

=item Arguments: $url

=item Return Value: $result

=back

  my $result = $conn->xform_media_browse('file:///media/music/');

Browse available media in a C<$url>.

=cut

xmmsc_result_t *
xmmsc_xform_media_browse (c, url)
		xmmsc_connection_t *c
		const char *url

=head2 xform_media_browse_encoded

=over 4

=item Arguments: $url

=item Return Value: $result

=back

  my $result = $conn->xform_media_browse_encoded($url);

Same as L</xform_media_browse>, except it expects C<url> to be url encoded
already.

=cut

xmmsc_result_t *
xmmsc_xform_media_browse_encoded (c, url)
		xmmsc_connection_t *c
		const char *url


## Bindata

=head2 bindata_add

=over 4

=item Arguments: $data

=item Return Value: $result

=back

  my $result = $conn->bindata_add($data);

Add binary data to the servers bindata directory.

=cut

xmmsc_result_t *
xmmsc_bindata_add (c, data)
		xmmsc_connection_t *c
	PREINIT:
		STRLEN len = 0;
	INPUT:
		const unsigned char *data = ($type)SvPVbyte ($arg, len);
	C_ARGS:
		c, data, len

=head2 bindata_retrieve

=over 4

=item Arguments: $hash

=item Return Value: $result

=back

  my $result = $conn->bindata_retrieve($hash);

Retrieve a file from the servers bindata directory, based on the C<$hash>.

=cut

xmmsc_result_t *
xmmsc_bindata_retrieve (c, hash)
		xmmsc_connection_t *c
		const char *hash

=head2 bindata_remove

=over 4

=item Arguments: $hash

=item Return Value: $result

=back

  my $result = $conn->bindata_remove($hash);

Remove a file from the servers bindata directory, based on the C<$hash>.

=cut

xmmsc_result_t *
xmmsc_bindata_remove (c, hash)
		xmmsc_connection_t *c
		const char *hash

=head2 bindata_list

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->bindata_list;

List all bindata hashes stored on the server.

=cut

xmmsc_result_t *
xmmsc_bindata_list (c)
		xmmsc_connection_t *c


## Other

=head2 config_register_value

=over 4

=item Arguments: $key, $default_value

=item Return Value: $result

=back

  my $result = $conn->config_register_value('myclient.stop_playback_on_quit', 0);

Registers a configvalue called C<$key> with a C<$default_value> in the server.

=cut

xmmsc_result_t *
xmmsc_config_register_value (c, valuename, defaultvalue)
		xmmsc_connection_t *c
		const char *valuename
		const char *defaultvalue

=head2 config_set_value

=over 4

=item Arguments: $key, $value

=item Return Value: $result

=back

  my $result = $conn->config_set_value('myclient.stop_playback_on_quit', 1);

Sets a configvalue called C<$key> to C<$value> in the server.

=cut

xmmsc_result_t *
xmmsc_config_set_value (c, key, val)
		xmmsc_connection_t *c
		const char *key
		const char *val

=head2 config_get_value

=over 4

=item Arguments: $key

=item Return Value: $result

=back

  my $result = $conn->config_get_value('myclient.stop_playback_on_quit');

Retrieves the value of a configvalue called C<$key> from the server.

=cut

xmmsc_result_t *
xmmsc_config_get_value (c, key)
		xmmsc_connection_t *c
		const char *key

=head2 config_list_values

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->config_list_values;

Lists all configuration values.

=cut

xmmsc_result_t *
xmmsc_config_list_values (c)
		xmmsc_connection_t *c

=head2 broadcast_config_value_changed

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_config_value_changed;

Requests the config_value_changed broadcast. This will be called when a
configvalue has been updated.

=cut

xmmsc_result_t *
xmmsc_broadcast_config_value_changed (c)
		xmmsc_connection_t *c

=head2 broadcast_mediainfo_reader_status

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_mediainfo_reader_status;

Request status for the mediainfo reader. It can be idle or working.

=cut

xmmsc_result_t *
xmmsc_broadcast_mediainfo_reader_status (c)
		xmmsc_connection_t *c

=head2 signal_mediainfo_reader_unindexed

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->signal_mediainfo_reader_unindexed;

Request number of unindexed entries in medialib.

=cut

xmmsc_result_t *
xmmsc_signal_mediainfo_reader_unindexed (c)
		xmmsc_connection_t *c

=head2 userconfdir_get

=over 4

=item Arguments: none

=item Return Value: $path

=back

Get the absolute path to the user config dir.

=cut

const char *
xmmsc_userconfdir_get (class)
	PREINIT:
		char path[XMMS_PATH_MAX];
	CODE:
		RETVAL = xmmsc_userconfdir_get (path, XMMS_PATH_MAX);
	OUTPUT:
		RETVAL


## Playback

=head2 playback_tickle

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playback_tickle;

Stop decoding of current song. This will start decoding of the song set with
xmmsc_playlist_set_next, or the current song again if no
xmmsc_playlist_set_next was executed.

=cut

xmmsc_result_t *
xmmsc_playback_tickle (c)
		xmmsc_connection_t *c

=head2 playback_stop

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playback_stop;

Stops the current playback. This will make the server idle.

=cut

xmmsc_result_t *
xmmsc_playback_stop (c)
		xmmsc_connection_t *c

=head2 playback_pause

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playback_pause;

Pause the current playback, will tell the output to not read nor write.

=cut

xmmsc_result_t *
xmmsc_playback_pause (c)
		xmmsc_connection_t *c

=head2 playback_start

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playback_start;

Starts playback if server is idle.

=cut

xmmsc_result_t *
xmmsc_playback_start (c)
		xmmsc_connection_t *c

=head2 playback_seek_ms

=over 4

=item Arguments: $milliseconds

=item Arguments: $whence?

=item Return Value: $result

=back

  my $result = $conn->playback_seek_ms(1000);

Seek in the current playback. The time is specified in C<$milliseconds>.
The whence parameter specifies whether the time is absolute (seek mode 'set')
or relative to the current point in the song (seek mode 'cur'). The default is
to seek using an absolute time.

=cut

xmmsc_result_t *
xmmsc_playback_seek_ms (c, milliseconds, whence = XMMS_PLAYBACK_SEEK_SET)
		xmmsc_connection_t *c
		int32_t milliseconds
		xmms_playback_seek_mode_t whence

=head2 playback_seek_samples

=over 4

=item Arguments: $samples

=item Return Value: $result

=back

  my $result = $conn->playback_seek_samples(5000);

Seek in the current playback. The time is specified in C<$samples>.
The whence parameter specifies whether the time is absolute (seek mode 'set')
or relative to the current point in the song (seek mode 'cur'). The default is
to seek using an absolute time.

=cut

xmmsc_result_t *
xmmsc_playback_seek_samples (c, samples, whence = XMMS_PLAYBACK_SEEK_SET)
		xmmsc_connection_t *c
		int32_t samples
		xmms_playback_seek_mode_t whence

=head2 broadcast_playback_status

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_playback_status;

Requests the playback status broadcast. This will be called when events like
play, stop and pause is triggered.

=cut

xmmsc_result_t *
xmmsc_broadcast_playback_status (c)
		xmmsc_connection_t *c

=head2 playback_status

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playback_status;

Request the playback status.

=cut

xmmsc_result_t *
xmmsc_playback_status (c)
		xmmsc_connection_t *c

=head2 broadcast_playback_current_id

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_playback_current_id;

Request the current id signal. This will be called then the current playing id
is changed. New song for example.

=cut

xmmsc_result_t *
xmmsc_broadcast_playback_current_id (c)
		xmmsc_connection_t *c

=head2 playback_current_id

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playback_current_id;

Request the current playback id.

=cut

xmmsc_result_t *
xmmsc_playback_current_id (c)
		xmmsc_connection_t *c

=head2 signal_playback_playtime

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->signal_playback_playtime;

Request the playback_playtime signal. Will update the time we have played the
current entry.

=cut

xmmsc_result_t *
xmmsc_signal_playback_playtime (c)
		xmmsc_connection_t *c

=head2 playback_playtime

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playback_playtime;

Request the current playtime.

=cut

xmmsc_result_t *
xmmsc_playback_playtime (c)
		xmmsc_connection_t *c

=head2 playback_volume_set

=over 4

=item Arguments: $channel, $volume

=item Return Value: $result

=back

  my $result = $conn->playback_volume_set('left', 75);

Set the C<$volume> for a given C<$channel>.

=cut

xmmsc_result_t *
xmmsc_playback_volume_set (c, channel, volume)
		xmmsc_connection_t *c
		const char *channel
		uint32_t volume

=head2 playback_volume_get

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playback_volume_get;

Get the current playback volume.

=cut

xmmsc_result_t *
xmmsc_playback_volume_get (c)
		xmmsc_connection_t *c

=head2 broadcast_playback_volume_changed

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_playback_volume_changed;

Request the playback_volume_changed broadcast. This will be called when the
playback volume changed.

=cut

xmmsc_result_t *
xmmsc_broadcast_playback_volume_changed (c)
		xmmsc_connection_t *c


## Playlist

=head2 playlist_list

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playlist_list;

List the existing playlists.

=cut

xmmsc_result_t *
xmmsc_playlist_list (c)
		xmmsc_connection_t *c

=head2 broadcast_playlist_changed

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_playlist_changed;

Request the playlist changed broadcast from the server. Everytime someone
manipulate the playlist this will be emitted.

=cut

xmmsc_result_t *
xmmsc_broadcast_playlist_changed (c)
		xmmsc_connection_t *c

=head2 broadcast_playlist_current_pos

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_playlist_current_pos;

Request the playlist current pos broadcast. When the position in the playlist
is changed this will be called.

=cut

xmmsc_result_t *
xmmsc_broadcast_playlist_current_pos (c)
		xmmsc_connection_t *c

=head2 broadcast_playlist_loaded

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->broadcast_playlist_loaded;

Request the playlist_loaded broadcast. This will be called if a playlist is
loaded server-side.

=cut

xmmsc_result_t *
xmmsc_broadcast_playlist_loaded (c)
		xmmsc_connection_t *c

=head2 playlist_current_active

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $conn->playlist_current_active;

Retrive the name of the active playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_current_active (c)
		xmmsc_connection_t *c

=head2 playlist_set_next

=over 4

=item Arguments: $pos

=item Return Value: $result

=back

  my $result = $conn->playlist_set_next(7);

Set next entry in the playlist to the absolute position C<$pos>.

=cut

xmmsc_result_t *
xmmsc_playlist_set_next (c, pos)
		xmmsc_connection_t *c
		uint32_t pos

=head2 playlist_set_next_rel

=over 4

=item Arguments: $pos

=item Return Value: $result

=back

  my $result = $conn->playlist_set_next_rel(-1);

Same as L</playlist_set_next> but relative to the current position.

=cut

xmmsc_result_t *
xmmsc_playlist_set_next_rel (c, pos)
		xmmsc_connection_t *c
		int32_t pos

=head2 coll_idlist_from_playlist_file

=over 4

=item Arguments: $path

=item Return Value: $result

=back

  my $result = $conn->coll_idlist_from_playlist_file('file:///path/to/some/playlist.m3u');

Create a new collections structure with type idlist from a playlist file at
C<$path>.

=cut

xmmsc_result_t *
xmmsc_coll_idlist_from_playlist_file (c, path)
		xmmsc_connection_t *c
		const char *path


## IO

=head2 io_want_out

=over 4

=item Arguments: none

=item Return Value: 1 | 0

=back

  my $has_pending_output = $conn->io_want_out;

Check for pending output.

=cut

int
xmmsc_io_want_out (c)
		xmmsc_connection_t *c

=head2 io_out_handle

=over 4

=item Arguments: none

=item Return Value: $success

=back

  my $success = $conn->io_out_handle;

Write pending data. Should be called when the mainloop flags that writing is
available on the socket.

=cut

int
xmmsc_io_out_handle (c)
		xmmsc_connection_t *c

=head2 io_in_handle

=over 4

=item Arguments: none

=item Return Value: $success

=back

  my $success = $conn->io_in_handle;

Read available data. Should be called when the mainloop flags that reading is
available on the socket.

=cut

int
xmmsc_io_in_handle (c)
		xmmsc_connection_t *c

=head2 io_fd_get

=over 4

=item Arguments: none

=item Return Value: $fd | -1

=back

  my $fd = $conn->io_fd_get;

Retrieve filedescriptor for the connection. Returns -1 on error. This is to be
used in a mainloop to do poll/select on. Reading and writing should B<NOT> be
done on this fd. L</io_in_handle> and L</io_out_handle> B<MUST> be used to
handle reading and writing.

=cut

int
xmmsc_io_fd_get (c)
		xmmsc_connection_t *c

=head2 io_need_out_callback_set

=over 4

=item Arguments: \&func, $data?

=item Return Value: none

=back

  $conn->io_need_out_callback_set(sub { ... });

Set callback for enabling/disabling writing.

If the mainloop doesn't provide a mechanism to run code before each iteration
this function allows registration of a callback to be called when output is
needed or not needed any more. The arguments to the callback are the
connection, flags and C<$data>, if specified; flag is true if output is wanted,
false if not.

=cut

void
xmmsc_io_need_out_callback_set (c, func, data=NULL)
		xmmsc_connection_t *c
		SV *func
		SV *data
	PREINIT:
		PerlXMMSClientCallback *cb = NULL;
		PerlXMMSClientCallbackParamType param_types[2];
	CODE:
		param_types[0] = PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_CONNECTION;
		param_types[1] = PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_FLAG;

		cb = perl_xmmsclient_callback_new (func, data, ST(0), 2, param_types,
		                                   PERL_XMMSCLIENT_CALLBACK_RETURN_TYPE_NONE);

		xmmsc_io_need_out_callback_set_full (c,
		                                     perl_xmmsclient_xmmsc_io_need_out_callback_set_cb, cb,
		                                     (xmmsc_user_data_free_func_t)perl_xmmsclient_callback_destroy);

=head2 loop

=over 4

=item Arguments: none

=item Return Value: none

=back

  $conn->loop;

Starts a select-based mainloop which may be terminated by calling
C</quit_loop>.

=head2 quit_loop

=over 4

=item Arguments: none

=item Return Value: none

=back

  $conn->quit_loop;

Terminates the mainloop started with C</loop>.

=cut

void
DESTROY (c)
		xmmsc_connection_t *c
	CODE:
		xmmsc_unref (c);

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

  perldoc Audio::XMMSClient

You can also look for information at:

=over 4

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/Audio-XMMSClient>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/Audio-XMMSClient>

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Audio-XMMSClient>

=item * Search CPAN

L<http://search.cpan.org/dist/Audio-XMMSClient>

=back

=head1 SEE ALSO

L<Audio::XMMSClient::Result>, L<Audio::XMMSClient::Playlist>, L<Audio::XMMSClient::Collection>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2008, Florian Ragwitz

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself, either Perl version 5.8.8 or, at your option,
any later version of Perl 5 you may have available.

=cut

BOOT:
	PERL_UNUSED_VAR (items);
	PERL_XMMSCLIENT_CALL_BOOT (boot_Audio__XMMSClient__Playlist);
	PERL_XMMSCLIENT_CALL_BOOT (boot_Audio__XMMSClient__Collection);
	PERL_XMMSCLIENT_CALL_BOOT (boot_Audio__XMMSClient__Result);
