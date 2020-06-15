#include <stdio.h>

#include "pipetee.h"

PipeTee::PipeTee(const char *filename)
{
    fp = fopen(filename, "w");
}

PipeTee::~PipeTee()
{
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }
}

void PipeTee::acceptByte(uint8_t byte)
{
    fwrite(&byte, 1, 1, fp);
    forward(byte);
}

