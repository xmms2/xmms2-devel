package Audio::XMMSClient::Collection;

use strict;
use warnings;
use base qw/Exporter/;

my %functions = (
        set    => [qw/reference union intersection complement/],
        filter => [qw/has equals match smaller greater/],
        list   => [qw/idlist queue partyshuffle/],
);

my @functions = map { @{$functions{$_}} } keys %functions;

for my $type (@functions) {
    {
        no strict 'refs';
        *$type = sub {
            my (@args) = @_;

            if (ref $args[-1]) {
                shift @args if @args > 1;
            }
            elsif (@args % 2 != 0) {
                shift @args; # $args[0] eq classname
            }

            __PACKAGE__->new($type, @args);
        };
    }
}

our @EXPORT_OK = (@functions, 'universe');
our %EXPORT_TAGS = (
        all => \@EXPORT_OK,
        %functions,
);

1;
