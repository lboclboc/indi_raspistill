#!/usr/bin/env python3

import os
import sys

class BRCMo:
    size = 18711040
    raw_width = 6112

stat = os.stat(sys.argv[1])

brcmo_start = stat.st_size - BRCMo.size
print(brcmo_start)

with open(sys.argv[1] + ".rgb24", "wb") as fout:
    with open(sys.argv[1], "rb") as fin:
        while True:
            marker = fin.read(2)

