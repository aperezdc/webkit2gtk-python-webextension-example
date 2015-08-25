#! /bin/sh
#
# run.sh
# Copyright (C) 2015 Adrian Perez <aperez@igalia.com>
#
# Distributed under terms of the MIT license.
#
make -s
export PYTHONPATH=$(pwd)
exec ./browse-with-extension "$@"
