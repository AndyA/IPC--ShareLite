use 5.006001;

use strict;
# use warnings;

use Config;
use ExtUtils::MakeMaker;

if ( $^O =~ /^(MS)?Win32$/ ) {
    warn "IPC::ShareLite doesn't work on Windows.\nStopping.\n";
    exit 0;    # == NA test result
}

if ( $^O =~ /^VMS$/ ) {
    warn "IPC::ShareLite doesn't work on VMS.\nStopping.\n";
    exit 0;    # == NA test result
}

my %mm_args = (
    ( MM->can( 'signature_target' ) ? ( SIGN => 1 ) : () ),
    NAME          => 'IPC::ShareLite',
    AUTHOR        => 'Maurice Aubrey <maurice@cpan.org>',
    VERSION_FROM  => 'lib/IPC/ShareLite.pm',
    ABSTRACT_FROM => 'lib/IPC/ShareLite.pm',
    PL_FILES      => {},
    PREREQ_PM     => { 'Test::More' => 0, },
    LIBS          => [''],
    DEFINE        => '',
    INC           => '',
    OBJECT        => 'sharestuff.o ShareLite.o',
    dist          => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean => { FILES => 'IPC-ShareLite-*' },
);

{
    local $^W = 0;    # Silence warning about non-numeric version
    if ( $ExtUtils::MakeMaker::VERSION >= '6.31' ) {
        $mm_args{LICENSE} = 'perl';
    }
}

WriteMakefile( %mm_args );

