#ifndef MMALLISTENER_H
#define MMALLISTENER_H

#include <stddef.h>

class MMALListener
{
public:
    virtual void buffer_received(uint8_t *buffer, size_t len, uint32_t pitch) = 0;
};

#endif // MMALLISTENER_H
