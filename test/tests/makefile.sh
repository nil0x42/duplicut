#!/bin/bash

# test suite: makefile.sh
#
# This test ensures that all Makefile rules
# works as expected.

set -ve

#### ensure invalid rule make makefile fail
! make foobar

#### make debug:
#### make with debug infos
make debug
test -d objects/
which ctags 2>/dev/null && test -f tags
test -f duplicut -a -x duplicut
./duplicut &> /dev/null
# check there is debug symbols AND NO profiling info (linux only)
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    nm --debug-syms duplicut | grep -Eq '\s+N\s+\.debug_[a-z]+'
    ! test -e gmon.out
fi

#### make clean:
#### remove unneeded objects, keep duplicut executable only
make
make clean
test -f duplicut -a -x duplicut
! test -e objects
! test -e tags
! test -e gmon.out


#### make distclean:
#### restore state before compilation
make distclean
! test -e duplicut
! test -e objects
! test -e tags
! test -e gmon.out


#### make profile:
#### profiling infos for gprof, linux only
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    make distclean
    make profile
    ! test -e gmon.out
    ./duplicut > /dev/null
    # ensure there is gmon.out
    test -e gmon.out
fi


#### make (make release):
#### create an optimized binary file, without debug infos
make
./duplicut &> /dev/null
# ensure there is no profile info nor debug symbol
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    ! test -e gmon.out
    ! nm --debug-syms duplicut | grep -Eq '\s+N\s+\.debug_[a-z]+'
fi

# ensure /proc/meminfo exists (linux)
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    cat /proc/meminfo
fi
