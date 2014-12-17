#!/bin/bash

set -ve

echo "CC=$CC"
make fclean
make
make fclean
make release
