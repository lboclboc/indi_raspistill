#!/bin/bash
export LD_LIRARY_PATH=$(pwd)/build/userland-prefix/src/userland/build/lib
exec qtcreator "$@"
