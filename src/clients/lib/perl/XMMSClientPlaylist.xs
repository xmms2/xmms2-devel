#include "perl_xmmsclient.h"

MODULE = Audio::XMMSClient::Playlist	PACKAGE = Audio::XMMSClient::Playlist	PREFIX = xmmsc_playlist_

=head1 NAME

Audio::XMMSClient::Playlist - Playlists for Audio::XMMSClient

=head1 SYNOPSIS

  use Audio::XMMSClient;

  my $conn = Audio::XMMSClient->new($client_name);
  $conn->connect or die $conn->get_last_error;

  my $playlist = $conn->playlist($playlist_name);
  $playlist->shuffle;

=head1 DESCRIPTION

This module provides an abstraction for L<Audio::XMMSClient>'s playlists.

=head1 METHODS

=head2 list_entries

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $playlist->list_entries;

List playlists content.

=cut

xmmsc_result_t *
xmmsc_playlist_list_entries (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

=head2 create

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $playlist->create;

Creates a new empty playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_create (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

=head2 current_pos

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $playlist->current_pos;

Retrives the current position in the playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_current_pos (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

=head2 shuffle

=over 4

=item Arguments; none

=item Return Value: $result

=back

  my $result = $playlist->shuffle;

Shuffles the playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_shuffle (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

=head2 sort

=over 4

=item Arguments: \@properties

=item Return Value: $result

=back

  my $result = $playlist->sort([qw/artist album tracknr/]);

Sorts the playlist according to the list of C<\@properties>.

=cut

xmmsc_result_t *
xmmsc_playlist_sort (p, properties)
		perl_xmmsclient_playlist_t *p
		xmmsv_t *properties = ($type)perl_xmmsclient_pack_stringlist ($arg);
	C_ARGS:
		p->conn, p->name, properties
	CLEANUP:
		xmmsv_unref (properties);

=head2 clear

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $playlist->clear;

Clears the playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_clear (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

=head2 insert_id

=over 4

=item Arguments: $position, $id

=item Return Value: $result

=back

  my $result = $playlist->insert_id(2, 1337);

Insert a medialib C<$id> at given C<$position>.

=cut

xmmsc_result_t *
xmmsc_playlist_insert_id (p, pos, id)
		perl_xmmsclient_playlist_t *p
		int pos
		unsigned int id
	C_ARGS:
		p->conn, p->name, pos, id

=head2 insert_args

=over 4

=item Arguments: $position, $url, @args?

=item Return Value: $result

=back

  my $result = $playlist->insert_args(1, 'file://path/to/media/file.sid', 'subtune=2');

Insert a C<$url> at C<$position> with C<@args>.

=cut

xmmsc_result_t *
xmmsc_playlist_insert_args (p, pos, url, ...)
		perl_xmmsclient_playlist_t *p
		int pos
		const char *url
	PREINIT:
		int i, nargs;
		const char **args = NULL;
	INIT:
		nargs = items - 2;
		args = (const char **)malloc (sizeof (char *) * nargs);

		for (i = 0; i < nargs; i++) {
			args[i] = SvPV_nolen (ST (i+2));
		}
	C_ARGS:
		p->conn, p->name, pos, url, nargs, args
	CLEANUP:
		free (args);

=head2 insert_url

=over 4

=item Arguments: $position, $url

=item Return Value: $result

=back

  my $result = $playlist->insert_url(0, 'file://path/to/media/file.flac');

Insert C<$url> at C<$position>.

=cut

xmmsc_result_t *
xmmsc_playlist_insert_url (p, pos, url)
		perl_xmmsclient_playlist_t *p
		int pos
		const char *url
	C_ARGS:
		p->conn, p->name, pos, url

=head2 insert_encoded

=over 4

=item Arguments: $position, $url

=item Return Value: $result

=back

  my $result = $playlist->insert_encoded(5, $url);

Like L</insert_url>, except it expects C<$url> to be encoded already.

=cut

xmmsc_result_t *
xmmsc_playlist_insert_encoded (p, pos, url)
		perl_xmmsclient_playlist_t *p
		int pos
		const char *url
	C_ARGS:
		p->conn, p->name, pos, url

=head2 insert_collection

=over 4

=item Arguments: $position, $collection, \@order

=item Return Value: $result

=back

  my $result = $playlist->insert_collection(5, $coll, [qw/artist album tracknr/]);

Queries the medialib for entries matching C<$collection>, orders the results
by C<\@order> and inserts them at C<$position>.

=cut

xmmsc_result_t *
xmmsc_playlist_insert_collection (p, pos, collection, order)
		perl_xmmsclient_playlist_t *p
		int pos
		xmmsv_coll_t *collection
		xmmsv_t *order = ($type)perl_xmmsclient_pack_stringlist ($arg);
	C_ARGS:
		p->conn, p->name, pos, collection, order
	CLEANUP:
		free (order);

=head2 add_id

=over 4

=item Arguments: $id

=item Return Value: $result

=back

  my $result = $playlist->add_id(9667);

Add a medialib C<$id> to the end of the playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_add_id (p, id)
		perl_xmmsclient_playlist_t *p
		unsigned int id
	C_ARGS:
		p->conn, p->name, id

=head2 add_args

=over 4

=item Arguments: $url, @args?

=item Return Value: $result

=back

  my $result = $playlist->add_args('file:///path/to/media/file.sid', 'subtune=7');

Add a C<$url> with the given C<@args> to the playlists end.

=cut

xmmsc_result_t *
xmmsc_playlist_add_args (p, url, ...)
		perl_xmmsclient_playlist_t *p
		const char *url
	PREINIT:
		int i, nargs;
		const char **args = NULL;
	INIT:
		nargs = items - 1;
		args = (const char **)malloc (sizeof (char *) * nargs);

		for (i = 0; i < nargs; i++) {
			args[i] = SvPV_nolen (ST (i+1));
		}
	C_ARGS:
		p->conn, p->name, url, nargs, args
	CLEANUP:
		free (args);

=head2 add_url

=over 4

=item Arguments: $url

=item Return Value: $result

=back

  my $result = $playlist->add_url('file:///path/to/media/file.flac');

Add a C<$url> to the end of the playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_add_url (p, url)
		perl_xmmsclient_playlist_t *p
		const char *url
	C_ARGS:
		p->conn, p->name, url

=head2 add_encoded

=over 4

=item Arguments: $url

=item Return Value: $result

=back

  my $result = $playlist->add_encoded($url);

Like L</add_url>, except it expects C<$url> to be encoded already.

=cut

xmmsc_result_t *
xmmsc_playlist_add_encoded (p, url)
		perl_xmmsclient_playlist_t *p
		const char *url
	C_ARGS:
		p->conn, p->name, url

=head2 add_collection

=over 4

=item Arguments: $collection, \@order

=back

  my $result = $playlist->add_collection($coll, [qw/artist album/]);

Query the medialib for entries matching C<$coll>, sort the results by
C<\@order> and add the results to the end of the playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_add_collection (p, collection, order)
		perl_xmmsclient_playlist_t *p
		xmmsv_coll_t *collection
		xmmsv_t *order = ($type)perl_xmmsclient_pack_stringlist ($arg);
	C_ARGS:
		p->conn, p->name, collection, order
	CLEANUP:
		free (order);

=head2 move_entry

=over 4

=item Arguments: $current_position, $new_position

=item Return Value: $result

=back

  my $result = $playlist->move_entry(3, 0);

Moves a playlist entry from C<$current_position> to C<$new_position>.

=cut

xmmsc_result_t *
xmmsc_playlist_move_entry (p, cur_pos, new_pos)
		perl_xmmsclient_playlist_t *p
		uint32_t cur_pos
		uint32_t new_pos
	C_ARGS:
		p->conn, p->name, cur_pos, new_pos

=head2 remove_entry

=over 4

=item Arguments: $position

=item Return Value: $result

=back

  my $result = $playlist->remove_entry(6);

Removes the playlist entry at C<$position>.

=cut

xmmsc_result_t *
xmmsc_playlist_remove_entry (p, pos)
		perl_xmmsclient_playlist_t *p
		unsigned int pos
	C_ARGS:
		p->conn, p->name, pos

=head2 remove

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $playlist->remove;

Remove the playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_remove (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

=head2 load

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $result = $playlist->load;

Load a playlist as the current active playlist.

=cut

xmmsc_result_t *
xmmsc_playlist_load (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

=head2 radd

=over 4

=item Arguments: $url

=item Return Value: $result

=back

  my $result = $playlist->radd($url);

Adds a C<$url> recursivly to the playlist. The C<$url> should be absolute to the
server-side.

=cut

xmmsc_result_t *
xmmsc_playlist_radd (p, url)
		perl_xmmsclient_playlist_t *p
		const char *url
	C_ARGS:
		p->conn, p->name, url

=head2 radd_encoded

=over 4

=item Arguments: $url

=item Return Value: $result

=back

  my $result = $playlist->radd_encoded($url);

Same as L</radd>, except it expects C<$url> to be encoded already.

=cut

xmmsc_result_t *
xmmsc_playlist_radd_encoded (p, url)
		perl_xmmsclient_playlist_t *p
		const char *url
	C_ARGS:
		p->conn, p->name, url

=head2 rinsert

=over 4

=item Arguments: $position, $url

=item Return Value: $result

=back

  my $result = $playlist->rinsert(42, $url);

Inserts a C<$url> recursivly at a given position in the playlist. The
C<$url> should be absolute to the server-side.

=cut

xmmsc_result_t *
xmmsc_playlist_rinsert (p, pos, url)
		perl_xmmsclient_playlist_t *p
		int pos
		const char *url
	C_ARGS:
		p->conn, p->name, pos, url

=head2 rinsert_encoded

=over 4

=item Arguments: $position, $url

=item Return Value: $result

=back

  my $result = $playlist->rinsert_encoded(5, $url);

Same as L</rinsert>, except it expects C<$url> to be encoded already.

=cut

xmmsc_result_t *
xmmsc_playlist_rinsert_encoded (p, pos, url)
		perl_xmmsclient_playlist_t *p
		int pos
		const char *url
	C_ARGS:
		p->conn, p->name, pos, url

void
DESTROY (p)
		perl_xmmsclient_playlist_t *p
	CODE:
		perl_xmmsclient_playlist_destroy (p);

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 SEE ALSO

L<Audio::XMMSClient>, L<Audio::XMMSClient::Result>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2008, Florian Ragwitz

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself, either Perl version 5.8.8 or, at your option,
any later version of Perl 5 you may have available.

=cut

BOOT:
	PERL_UNUSED_VAR (items);
