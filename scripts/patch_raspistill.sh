#!/bin/bash
src="$1"
dst="$2"

if [ ! -f "$dst" ]; then
    sed -e "s/main(/xmain(/" "$src" > "$dst"
    echo "Patched $src -> $dst"
fi
