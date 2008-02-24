#!/bin/sh

perls=~/Works/Perl/versions/*/bin/perl
[ -x /usr/bin/perl ] && perls="$perls /usr/bin/perl"
[ -x /alt/local/bin/perl ] && perls="$perls /alt/local/bin/perl"

for perl in $perls
do
    echo === Testing with $perl
    pv=`$perl -v | perl -ne 'if (/(\d+\.\d+(?:[._]\d+)?)/) { print $1; last }'`
    [ -f Makefile ] && make distclean

    if {
        $perl Makefile.PL && make && make test
    } ; then
        echo "PASS $pv" | beeb
    else
        echo "FAIL $pv" | beeb
    fi
done

