#ifndef PIXELLISTENER_H
#define PIXELLISTENER_H


class PixelListener
{
public:
    virtual void pixels_received(uint8_t *buffer, size_t length, uint32_t pitch) = 0;
};

#endif // PIXELLISTENER_H
