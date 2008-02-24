use 5.006001;

use strict;
use warnings;

use Config;
use ExtUtils::MakeMaker;
use File::Temp qw( tempfile );

my @define = ();

# push @define, '-DHAS_UNION_SEMUN' if try(
#     <<EOS
# #include <sys/types.h>
# #include <sys/ipc.h>
# #include <sys/sem.h>
# int main () { union semun semun; semun.buf = 0; }
# EOS
# );

WriteMakefile(
    ( MM->can( 'signature_target' ) ? ( SIGN => 1 ) : () ),
    NAME          => 'IPC::ShareLite',
    AUTHOR        => 'Maurice Aubrey <maurice@cpan.org>',
    LICENSE       => 'perl',
    VERSION_FROM  => 'lib/IPC/ShareLite.pm',
    ABSTRACT_FROM => 'lib/IPC/ShareLite.pm',
    PL_FILES      => {},
    PREREQ_PM     => { 'Test::More' => 0, },
    LIBS          => [''],
    DEFINE        => join( ' ', @define ),
    INC           => '',
    OBJECT        => 'sharelite.o ShareLite.o',
    dist          => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean => { FILES => 'IPC-ShareLite-*' },
);

sub try {
    my $src = shift;
    my ( $fh, $fn ) = tempfile( UNLINK => 1, SUFFIX => '.c' );
    my ( $oh, $on ) = tempfile( UNLINK => 1, SUFFIX => '.o' );
    print $fh $src;
    my $cmd
      = join( ' ', $Config{cc}, $Config{ccflags}, '-o', $on, $fn );
    return 0 == system( $cmd);
}

