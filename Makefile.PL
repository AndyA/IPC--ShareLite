use strict;
use warnings;
use ExtUtils::MakeMaker;

WriteMakefile(
    ( MM->can( 'signature_target' ) ? ( SIGN => 1 ) : () ),
    NAME          => 'IPC::ShareLite',
    AUTHOR        => 'Maurice Aubrey <maurice@cpan.org>',
    LICENSE       => 'perl',
    VERSION_FROM  => 'ShareLite.pm',
    ABSTRACT_FROM => 'ShareLite.pm',
    PL_FILES      => {},
    PREREQ_PM     => { 'Test::More' => 0, },
    LIBS          => [''],
    DEFINE        => '',
    INC           => '',
    OBJECT        => 'sharelite.o ShareLite.o',
    dist          => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean => { FILES => 'IPC-ShareLite-*' },
);
