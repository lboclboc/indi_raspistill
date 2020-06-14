#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include "jpegpipeline.h"
#include "broadcompipeline.h"
#include "raw12tobayer16pipeline.h"

int main(int argc, char **argv)
{
    uint8_t byte;
    std::ifstream fin;
    fin.open("raw.jpg");
    if (!fin) {
        std::cerr << "Failed to open file\n";
        exit(1);
    }
    size_t fb_size = 2 * 4056 * 3040;
    uint16_t *fb = static_cast<uint16_t *>(malloc(fb_size));
    JpegPipeline receiver;
    BroadcomPipeline brcm;
    receiver.daisyChain(&brcm);

    Raw12ToBayer16Pipeline raw12(&brcm.header, fb, fb_size, 4056, 3040);
    receiver.daisyChain(&raw12);

    try
    {
        while(fin.read(reinterpret_cast<char *>(&byte), 1))
        {
            receiver.acceptByte(byte);
        }
    }
    catch(JpegPipeline::Exception e)
    {
        fprintf(stderr, "Caught exception %s\n", e.what());
    }

    FILE *fp = fopen("/dev/shm/capture", "w");
    fwrite(fb, fb_size, 1, fp);
    fclose(fp);

    return 0;
}
