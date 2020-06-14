#include "broadcompipeline.h"

void BroadcomPipeline::acceptByte(uint8_t byte)
{
    pos++;

    if (pos < 9) {
        header.BRCM[pos] = byte;
    }
    else if (pos < (9 + sizeof header.omx_data)) {
        reinterpret_cast<uint8_t *>(&header.omx_data)[pos - 9] = byte;
    }
    else if (pos >= 32768) {
        forward(byte);
    }
}

