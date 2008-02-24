use 5.006001;

use strict;
use warnings;

use Config;
use ExtUtils::MakeMaker;

if ( $^O =~ /^(MS)?Win32$/ ) {
    warn "IPC::ShareLite doesn't work on Windows.\nStopping.\n";
    exit 0;    # == NA test result
}

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
    DEFINE        => '',
    INC           => '',
    OBJECT        => 'sharelite.o ShareLite.o',
    dist          => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean => { FILES => 'IPC-ShareLite-*' },
);
