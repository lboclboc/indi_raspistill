#!/usr/bin/env python3

import os
import sys

class BRCMo:
    size = 18711040
    header_size = 32768
    raw_width = 6112
    pixel_row_size = raw_width - 28

odd_row = False

def write_int16_le(f, val):
    ''' Write 16-bit value little-endian.'''
    f.write(bytes([val&0xFF, (val>>8)&0xFF]))

def process_simple_channel_split(row, fout):
    '''
    Just split groups of three bytes into three channels.
    Observations: line 0, 2, 4... Are much brighter than line 1, 3...
    '''
    for p in range(0, BRCMo.pixel_row_size, 3):
        for ch in range(3):
            fout[ch].write(bytes([row[p + ch]]))


def process_hyp2(row, fout):
    '''
     format hypotesis:
     Input rows are ordered as:
         0   1    2        3   4   5    6       7       8
     0   1ua 1ub  1la|1lb  2la ...
     1   1uc 1ud  1lc|1ld  2lc ...
           |
           V
     16 out (channel 0 = a from above):
     0   1   2   3
     1la 1ua 2la 2ua
     
     u = Upper bits
     l = lower bits
     | = Joined byte of two nibbles

     Observations:
        The raw file row length seems to rime with this to give correct pixel width.
        6112 (-28) * 2/3 = 4056
    '''
    if not row:
        # End of file.
        for ch in range(4):
            fout[ch].truncate(3040 * 4056 * 2 / 3)

    else:
        channels = [0, 1] if odd_row else [2, 3]
        for p in range(0, BRCMo.pixel_row_size, 3):
            # Write out per channel in little endian 16 bit
            v1 = (row[p]<<4) + (row[p+2]&0xF)
            v2 = (row[p+1]<<4) + ((row[p+2]>>4)&0xF)

            write_int16_le(fout[channels[0]], v1)
            write_int16_le(fout[channels[1]], v2)


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
    # End of file.
    if not row:
        for ch in range(3):
            fout[ch].truncate(3040 * 4056 * 2 / 3)

    else:
        for p in range(0, BRCMo.pixel_row_size, 9):
            for ch in range(3):
                # Write out per channel in little endian 16 bit
                fout[ch].write(bytes([row[p + ch], (row[p + ch + 6] & 0xF)]))
                fout[ch].write(bytes([row[p + 3 + ch], (row[p + ch + 6] & 0xF0) >> 4]))

if __name__ == '__main__':
    stat = os.stat(sys.argv[1])

    brcmo_start = stat.st_size - BRCMo.size

    #processor = process_simple_channel_split
    processor = process_hyp2

    with open(sys.argv[1], "rb") as fin:
        fin.seek(brcmo_start, 0)
        with open(sys.argv[1] + ".brcm", "wb") as fout:
            brcm = fin.read(BRCMo.header_size)
            fout.write(brcm)
            
        fout = [None, None, None, None]
        with open(sys.argv[1] + ".raw", "wb") as raw, \
             open(sys.argv[1] + ".raw-0", "wb") as fout[0], \
             open(sys.argv[1] + ".raw-1", "wb") as fout[1], \
             open(sys.argv[1] + ".raw-2", "wb") as fout[2], \
             open(sys.argv[1] + ".raw-3", "wb") as fout[3]:
            while True:
                row = fin.read(BRCMo.raw_width)
                raw.write(row[0:BRCMo.pixel_row_size])
                if not row:
                    break
                processor(row, fout)
                odd_row = not odd_row

            # Indicate eof
            processor(row, fout)


        
