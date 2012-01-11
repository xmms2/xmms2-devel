package Audio::XMMSClient;

use strict;
use warnings;
use Carp;
use IO::Handle;
use IO::Select;
use Audio::XMMSClient::Collection;

our $VERSION = '0.04';
our @ISA;

eval {
    require XSLoader;
    XSLoader::load(__PACKAGE__, $VERSION);
    1;
} or do {
    require DynaLoader;
    push @ISA, 'DynaLoader';
    bootstrap Audio::XMMSClient $VERSION;
};

sub loop {
    my ($self) = @_;

    my $fd = IO::Handle->new_from_fd( $self->io_fd_get, 'r+' );
    $self->{do_loop} = 1;

    pipe my $r, my $w;
    $self->{wakeup} = $w;

    my $rin = IO::Select->new( $fd, $r );
    my $ein = IO::Select->new( $fd     );
    my $win;

    while ($self->{do_loop}) {

        if ($self->io_want_out) {
            $win = IO::Select->new( $fd );
        }
        else {
            $win = undef;
        }

        my ($i, $o, $e) = IO::Select->select( $rin, $win, $ein );

        if (ref $i && @$i && $i->[0] == $fd) {
            $self->io_in_handle;
        }

        if (ref $o && @$o && $o->[0] == $fd) {
            $self->io_out_handle;
        }

        if (ref $e && @$e && $e->[0] == $fd) {
            $self->disconnect;
            $self->{do_loop} = 0;
        }
    }
}

sub quit_loop {
    my ($self) = @_;

    $self->{do_loop} = 0;
    $self->{wakeup}->print('42');
}

sub request {
    my $self = shift;
    my $func = shift;

    my $callback  = pop;

    if (!$self->can($func)) {
        Carp::croak( "Invalid request name `${func}' given" );
    }

    my $result = $self->$func( @_ );
    $result->notifier_set($callback);

    return $result;
}

sub plugin_list {
    my ($pkg, $file, $line) = caller;
    warn "call to deprecated sub 'plugin_list' at $file line $line";

    my ($self, $type) = @_;
    return $self->main_list_plugins($type);
}

sub playback_seek_ms_rel {
    my ($pkg, $file, $line) = caller;
    warn "call to deprecated sub 'playback_seek_ms_rel' at $file line $line";

    my ($self, $ms) = @_;
    return $self->playback_seek_ms($ms, 'cur');
}

sub playback_seek_samples_rel {
    my ($pkg, $file, $line) = caller;
    warn "call to deprecated sub 'playback_seek_samples_rel' at $file line $line";

    my ($self, $samples) = @_;
    return $self->playback_seek_samples($samples, 'cur');
}

1;

=pod

=head1 NAME

Audio::XMMSClient - Interface to the xmms2 music player

=head1 SYNOPSIS

  use Audio::XMMSClient;

  $c = Audio::XMMSClient->new( $name );
  $c->connect;

  my $r = $c->playback_status;
  $r->wait;
  print $r->value;

=head1 DESCRIPTION

This module provides a perl interface to the xmms2 client library. It currently
lacks a good documentation, but the 'turorial' directory provides some nice and
well explained examples to get you started for now.

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2008, Florian Ragwitz

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself, either Perl version 5.8.8 or, at your option,
any later version of Perl 5 you may have available.

=cut
