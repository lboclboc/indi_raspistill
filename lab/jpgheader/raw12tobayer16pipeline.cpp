#include <iostream>
#include "raw12tobayer16pipeline.h"
#include "broadcompipeline.h"

void Raw12ToBayer16Pipeline::acceptByte(uint8_t byte)
{
    pos++;
    if (++raw_x >= bcm_header->omx_data.raw_width) {
        y += 1;
        x = 0;
        raw_x = 0;
    }

    if ( x < fb_width && y < fb_height) {
        uint16_t *cur_row = framebuffer + y * fb_width;
        // RAW according to experiment.
        switch(state)
        {
        case b1:
            cur_row[x] = (byte & 0x0F) >> 0;
            cur_row[x+1] = (byte & 0xF0) >> 4;
            state = b2;
            break;

        case b2:
            cur_row[x] |= byte << 4;
            x++;
            state = b3;
            break;

        case b3:
            cur_row[x] |= byte << 4;
            x++;
            state = b1;
            break;
        }
    }
}
