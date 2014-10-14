#!/bin/bash

set -ve

echo "CC=$CC"
make
make fclean
make release
