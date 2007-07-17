package Audio::XMMSClient::Sync;

use strict;
use warnings;
use Audio::XMMSClient;
use Scalar::Util qw/blessed/;

sub new {
    my $base = shift;

    my $c = Audio::XMMSClient->new( @_ );

    return $base->new_from( $c );
}

sub new_from {
    my ($base, $c) = @_;

    my $self = bless \$c, $base;

    return $self;
}

sub can {
    my $self = shift;

    if (blessed $self) {
        my $code = $$self->can(@_);
        return $code if $code;
    }

    return $self->UNIVERSAL::can(@_);
}

our $AUTOLOAD;
sub AUTOLOAD {
    my ($self) = @_;

    (my $func = $AUTOLOAD) =~ s/.*:://;
    return if $func eq 'DESTROY';

    unless ($$self->can($func)) {
        require Carp;
        Carp::croak (qq{Can't locate object method "$func" via package Audio::XMMSClient::Sync});
    }

    {
        no strict 'refs';
        *$AUTOLOAD = sub { shift->sync_request( $func, @_ ) };

        &$AUTOLOAD;
    }
}

sub sync_request {
    my $self = shift;
    my $request = shift;

    my $resp = $$self->$request(@_);

    return $resp unless blessed $resp;

    if ($resp->isa('Audio::XMMSClient::Result')) {
        $resp->wait;

        if ($resp->iserror) {
            require Carp;
            Carp::croak ($resp->get_error);
        }

        return $resp->value;
    }

    if ($resp->isa('Audio::XMMSClient::Playlist')) {
        return Audio::XMMSClient::Playlist::Sync->new_from($resp);
    }

    return $resp;
}

package Audio::XMMSClient::Playlist::Sync;

use strict;
use warnings;
use base qw/Audio::XMMSClient::Sync/;

1;
