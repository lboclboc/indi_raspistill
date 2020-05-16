#!/usr/bin/env python3

import os
import sys

class BRCMo:
    size = 18711040
    header_size = 32768
    raw_width = 6112
    pixel_row_size = raw_width - 28


def process_simple_channel_split(row, fout):
    '''
    Just split groups of three bytes into three channels.
    Observations: line 0, 2, 4... Are much brighter than line 1, 3...
    '''
    for p in range(0, BRCMo.pixel_row_size, 3):
        for ch in range(3):
            fout[ch].write(bytes([row[p + ch]]))

def process_hyp1(row, fout):
    '''
     format hypotesis:
     Input rows are ordered as:
     0   1   2     3   4   5    6       7       8
     1la 1lb 1lc   2la 2lb 2lc  1ua|2ua 1ub|2ub 1uc|2uc
           |
           V
     16 out (a-channel):
     0   1   2   3
     1la 1ua 2la 2ua

     Observations:
        The raw file row length seems to rime with this to give correct pixel width.
        6112 (-28) * 2/3 = 4056
    '''
    for p in range(0, BRCMo.pixel_row_size, 9):
        for ch in range(3):
            # Write out per channel in little endian 16 bit
            #fout[channel].write(bytes([row[p + channel]]))
            # FIXME: Currently only 8 channel
            fout[ch].write(bytes([row[p + ch], (row[p + ch + 6] & 0xF)]))
            fout[ch].write(bytes([row[p + 3 + ch], (row[p + ch + 6] & 0xF0) >> 4]))

if __name__ == '__main__':
    stat = os.stat(sys.argv[1])

    brcmo_start = stat.st_size - BRCMo.size

    with open(sys.argv[1], "rb") as fin:
        fin.seek(brcmo_start, 0)
        with open(sys.argv[1] + ".brcm", "wb") as fout:
            brcm = fin.read(BRCMo.header_size)
            fout.write(brcm)
            
        fout = [None, None, None]
        with open(sys.argv[1] + ".raw-0", "wb") as fout[0], open(sys.argv[1] + ".raw-1", "wb") as fout[1], open(sys.argv[1] + ".raw-2", "wb") as fout[2]:
            while True:
                row = fin.read(BRCMo.raw_width)
                if not row:
                    break
                #process_simple_channel_split(row, fout)
                process_hyp1(row, fout)
