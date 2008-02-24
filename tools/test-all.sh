#!/bin/sh

perls=~/Works/Perl/versions/*/bin/perl
[ -x /usr/bin/perl ] && perls="$perls /usr/bin/perl"
[ -x /alt/local/bin/perl ] && perls="$perls /alt/local/bin/perl"

for perl in $perls
do
    echo === Testing with $perl
    [ -f Makefile ] && make distclean
    if {
        $perl Makefile.PL && make && make test
    } ; then
        echo PASS | beeb
    else
        echo FAIL | beeb
    fi
done

