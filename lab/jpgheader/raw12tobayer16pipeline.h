#ifndef RAW12TOBAYER16PIPELINE_H
#define RAW12TOBAYER16PIPELINE_H
#include <cstddef>
#include "pipeline.h"

struct BroadcomHeader;

/**
 * @brief The Raw12ToBayer16Pipeline class
 * Accepts bytes in raw12 format and writes 16 bits bayer image.
 * RAW12 format is like | {R11,R10,R09,R08,R07,R06,R05,R04} | {R03,R02,R01,R00,G11,G10,G09,G08} | {G07,G06,G05,G04,G03,G02,G01} |
 *                                   b1                                      b2                               b3
 * Odd lines are swapped R->G, G-B
 */
class Raw12ToBayer16Pipeline : public Pipeline
{
public:
    Raw12ToBayer16Pipeline(const BroadcomHeader *bcm_header, uint16_t *framebuffer, size_t fb_size, size_t fb_width, size_t fb_height) :
        Pipeline(),
        bcm_header(bcm_header),
        framebuffer(framebuffer),
        fb_size(fb_size),
        fb_width(fb_width),
        fb_height(fb_height) {}

    virtual void acceptByte(uint8_t byte) override;

private:
    const BroadcomHeader *bcm_header;
    uint16_t *framebuffer;
    size_t fb_size;
    size_t fb_width;
    size_t fb_height;
    int x {0};
    int y {0};
    int pos {-1};
    int raw_x {-1};  // Position in the raw-data comming in.
    enum {b1, b2, b3} state = b1; // Which byte in the RAW12 format (see above).
};

#endif // RAW12TOBAYER16PIPELINE_H
