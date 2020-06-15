#ifndef PIPETEE_H
#define PIPETEE_H

#include "pipeline.h"

class PipeTee : public Pipeline
{
public:
    PipeTee(const char *filename);
    virtual ~PipeTee();
    virtual void acceptByte(uint8_t byte) override;

private:
    FILE *fp {};
};

#endif // PIPETEE_H
