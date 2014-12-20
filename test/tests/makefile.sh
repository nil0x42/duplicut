#!/bin/bash

# test suite: makefile.sh
#
# This test ensures that all Makefile rules
# works as expected.

set -ve

#### ensure invalid rule make makefile fail
! make foobar


#### run make multiple times, check for `nothing to do`
make
make all
test "$(make all | wc -l)" -eq "1"
make all | grep -iq "nothing"


#### make re
#### make with debug infos
make re
test -d objects/
test -f tags
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


#### make fclean:
#### restore state before compilation
make fclean
! test -e duplicut
! test -e objects
! test -e tags
! test -e gmon.out


#### make profile:
#### profiling infos for gprof)
make fclean
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
