use strict;
use warnings;
use ExtUtils::MakeMaker;
use Config;

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

# #include <sys/types.h>
# #include <sys/ipc.h>
# #include <sys/sem.h>
# int main () { union semun semun; semun.buf = 0; }
