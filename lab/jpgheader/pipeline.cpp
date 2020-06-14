#include <stdexcept>
#include "pipeline.h"

Pipeline::Pipeline()
{

}

void Pipeline::daisyChain(Pipeline *p)
{
    Pipeline *next = this;
    while(next->nextPipeline != nullptr) {
        next = nextPipeline;
    }
    next->nextPipeline = p;
}

void Pipeline::forward(uint8_t byte)
{
    if (nextPipeline == nullptr) {
        throw std::runtime_error("No next pipeline to forward bytes to.");
    }

    nextPipeline->acceptByte(byte);
}
