package Audio::XMMSClient::Glib;

use strict;
use warnings;
use Glib qw( TRUE FALSE );
use base qw(Audio::XMMSClient);

sub new {
    my $class = shift;

    my $self = $class->SUPER::new(@_);
    bless $self, $class;

    return $self;
}

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

{
    my $loop = Glib::MainLoop->new(undef, FALSE);

    sub loop {
        $loop->run;
    }

    sub quit_loop {
        $loop->quit;
    }
}

1;
