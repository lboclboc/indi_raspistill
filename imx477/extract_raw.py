#!/usr/bin/env python3

import os
import sys

class BRCMo:
    size = 18711040
    header_size = 32768
    raw_width = 6112

stat = os.stat(sys.argv[1])

brcmo_start = stat.st_size - BRCMo.size

with open(sys.argv[1], "rb") as fin:
    fin.seek(brcmo_start, 0)
    with open(sys.argv[1] + ".brcm", "wb") as fout:
        brcm = fin.read(BRCMo.header_size)
        fout.write(brcm)
        
    with open(sys.argv[1] + ".rgb24", "wb") as fout:
        while True:
            row = fin.read(BRCMo.raw_width)
            if not row:
                break
            #fout.write(row[0:6112-28])
            fout.write(row[0:6112])
