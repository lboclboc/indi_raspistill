#!/usr/bin/env python3

import os
import sys
import ctypes

class BRCMo(ctypes.Structure):
    size = 18711040
    header_size = 32768
    row_padding = 28
    _fields_ = [
# 00000000  42 52 43 4d 6f 00 00 00  fc 7f 00 00 00 00 00 00  |BRCMo...........|
        ("header",        ctypes.c_char * 8),
        ('dummy_list1',   ctypes.c_int16 * 4),

# 00000010  74 65 73 74 63 20 76 65  72 73 69 6f 6e 20 31 2e  |testc version 1.|
# 00000020  30 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |0...............|
# 00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
# *
        ('name',          ctypes.c_char * 0x90),

# 000000a0  e0 17 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
        ('raw_width',     ctypes.c_uint16),
        ('dummy_list2',   ctypes.c_uint16 * 7),

# 000000b0  66 75 6c 6c 00 00 00 00  00 00 00 00 00 00 00 00  |full............|
        ('full',          ctypes.c_char * 0x20),

# 000000d0  d8 0f e0 0b 02 00 02 00  9b 6f 00 00 ac 0d 00 00  |.........o......|
        ('width',         ctypes.c_uint16),
        ('height',        ctypes.c_uint16),
        ('padding_right', ctypes.c_uint16),
        ('padding_down',  ctypes.c_uint16),
        ('dummy_list3',   ctypes.c_uint16 * 4),

# 000000e0  ff ff ff ff 01 00 00 00  00 0a 00 00 01 00 01 00  |................|
# 000000f0  03 00 21 00 02 04 0c 00  00 00 00 00 02 00 00 00  |..!.............|
        ('dummy',         ctypes.c_uint32 * 2),
        ('transform',     ctypes.c_uint16),
        ('format',        ctypes.c_uint16),
        ('bayer_order',   ctypes.c_uint8),
        ('bayer_format',  ctypes.c_uint8),
    ]


odd_row = False

def debug_brcm(buffer):
    data = BRCMo.from_buffer_copy(buffer)
    for d in dir(data):
      if d[0] != '_':
          v = data.__getattribute__(d)
          print(d + ": ", str(data.__getattribute__(d)))

def write_int16_le(f, val):
    ''' Write 16-bit value little-endian.'''
    f.write(bytes([val&0xFF, (val>>8)&0xFF]))

def process_simple_channel_split(row, fout, brcm_info):
    '''
    Just split groups of three bytes into three channels.
    Observations: line 0, 2, 4... Are much brighter than line 1, 3...
    '''
    for p in range(0, brcm_info.raw_with - BRCMo.row_padding, 3):
        for ch in range(3):
            fout[ch].write(bytes([row[p + ch]]))

def process_row(row, fout, brcm_info):
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
    channels = [0, 1] if odd_row else [2, 3]
    for p in range(0, brcm_info.raw_width - BRCMo.row_padding, 3):
        # Write out per channel in little endian 16 bit
        v1 = row[p] + ((row[p+2]&0xF)<<8)
        v2 = row[p+1] + ((row[p+2]&0xF0)<<4)

        write_int16_le(fout[channels[0]], v1)
        write_int16_le(fout[channels[1]], v2)


if __name__ == '__main__':
    stat = os.stat(sys.argv[1])

    brcmo_start = stat.st_size - BRCMo.size

    with open(sys.argv[1], "rb") as fin:
        fin.seek(brcmo_start, 0)

        with open(sys.argv[1] + ".brcm", "wb") as fout:
            brcm = fin.read(BRCMo.header_size)
            fout.write(brcm)
            brcm_info = BRCMo.from_buffer_copy(brcm)


        fout = [None, None, None, None]
        with open(sys.argv[1] + ".raw", "wb") as raw, \
             open(sys.argv[1] + ".raw-b", "wb") as fout[0], \
             open(sys.argv[1] + ".raw-g1", "wb") as fout[1], \
             open(sys.argv[1] + ".raw-g2", "wb") as fout[2], \
             open(sys.argv[1] + ".raw-r", "wb") as fout[3]:
            while True:
                row = fin.read(brcm_info.raw_width)
                raw.write(row[0:brcm_info.raw_width - BRCMo.row_padding])
                if not row:
                    break
                process_row(row, fout, brcm_info)
                odd_row = not odd_row

            # Remove last 16 rows of data.
            for ch in range(4):
                fout[ch].truncate(3040 * 4056 * 2 / 3)
