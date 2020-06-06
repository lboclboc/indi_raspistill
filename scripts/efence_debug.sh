#!/bin/bash
PROG=$HOME/git/build-indi_raspistill-Desktop-Debug/indi_raspistill 
export EF_PROTECT_FREE=1
export EF_FILL=66
ulimit -c unlimited

rm core
LD_PRELOAD=/usr/lib/libefence.so.0.0 $PROG

echo "If it crached, then run: "
echo "gdb --tui $PROG core"
echo "info stack"
