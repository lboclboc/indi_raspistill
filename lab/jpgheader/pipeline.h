#ifndef PIPELINE_H
#define PIPELINE_H

#include <cstdint>

class Pipeline
{
public:
    Pipeline();
    virtual void acceptByte(uint8_t byte) = 0;
    void daisyChain(Pipeline *p);

protected:
    void forward(uint8_t);

private:
    Pipeline *nextPipeline {};
};

#endif // PIPELINE_H
