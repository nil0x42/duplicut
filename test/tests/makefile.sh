#!/bin/bash

# test suite: makefile.sh
#
# This test ensures that all Makefile rules
# works as expected.

set -ve

#### ensure invalid rule make makefile fail
! make foobar

#### make re
#### make with debug infos
make debug
test -d objects/
which ctags 2>/dev/null && test -f tags
test -f duplicut -a -x duplicut
./duplicut &> /dev/null
# check there is debug symbols AND NO profiling info
nm --debug-syms duplicut | grep -Eq '\s+N\s+\.debug_[a-z]+'
! test -e gmon.out


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
#### profiling infos for gprof)
make distclean
make profile
! test -e gmon.out
./duplicut &> /dev/null
# ensure there is 
test -e gmon.out


#### make release:
#### create an optimized binary file, without debug infos
make release
./duplicut &> /dev/null
# ensure there no profile information nor debug symbol
! test -e gmon.out
! nm --debug-syms duplicut | grep -Eq '\s+N\s+\.debug_[a-z]+'

# ensure /proc/meminfo is there
cat /proc/meminfo
