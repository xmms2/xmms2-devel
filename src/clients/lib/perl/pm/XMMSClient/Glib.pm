package Audio::XMMSClient::Glib;

use strict;
use warnings;
use Glib qw( TRUE FALSE );
use base qw(Audio::XMMSClient);

=head1 NAME

Audio::XMMSClient::Glib - Use Audio::XMMSClient with a Glib mainloop

=head1 SYNOPSIS

  use Audio::XMMSClient::Glib;

  my $conn = Audio::XMMSClient::Glib->new('my-glib-client');
  $conn->connect or die $conn->get_last_error;

  my $result = $conn->broadcast_playback_current_id;
  $result->notifier_set(sub { $conn->quit_loop });

  $conn->loop;

=head1 DESCRIPTION

This module subclasses L<Audio::XMMSClient> and overrides L</loop> and
L</quit_loop> with something that uses a L<Glib> based mainloop instead of the
standard select-mainloop of L<Audio::XMMSClient>.

=head1 METHODS

=cut

sub connect {
    my $self = shift;
    my $res = $self->SUPER::connect(@_);

    $self->io_need_out_callback_set(\&need_out);
    Glib::IO->add_watch( $self->io_fd_get, 'in', sub { $self->handle_in(@_) } );
    $self->{has_out_watch} = 0;

    return $res;
}

sub need_out {
    my ($self, $flag) = @_;

    if ($self->io_want_out && !$self->{has_out_watch}) {
        Glib::IO->add_watch( $self->io_fd_get, 'out', sub { $self->handle_out(@_) } );
        $self->{has_out_watch} = 1;
    }
}

sub handle_in {
    my ($self, $source, $cond) = @_;

    if ($cond eq 'in') {
        return $self->io_in_handle;
    }

    return TRUE;
}

sub handle_out {
    my ($self, $source, $cond) = @_;

    if ($cond eq 'out') {
        $self->io_out_handle;
    }

    return $self->{has_out_watch} = $self->io_want_out;
}

sub get_loop {
}

{
    my $loop = Glib::MainLoop->new(undef, FALSE);

=head2 loop

=over 4

=item Arguments: none

=item Return Value: none

=back

  $conn->loop;

Starts a L<Glib::MainLoop> for the given connection.

=cut

    sub loop {
        $loop->run;
    }

=head2 quit_loop

=over 4

=item Arguments: none

=item Return Value: none

=back

  $conn->quit_loop;

Terminates the mainloop started by L</loop>.

=cut

    sub quit_loop {
        $loop->quit;
    }
}

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 SEE ALSO

L<Audio::XMMSClient>, L<Audio::XMMSClient::Result::PropDict>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2008, Florian Ragwitz

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself, either Perl version 5.8.8 or, at your option,
any later version of Perl 5 you may have available.

=cut

1;
