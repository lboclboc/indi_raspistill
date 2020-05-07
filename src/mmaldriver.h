#ifndef _INDI_MMAL_H
#define _INDI_MMAL_H

#include <indiccd.h>

class MMALDriver : public INDI::CCD
{
public:
	MMALDriver();
	virtual ~MMALDriver();
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    bool UpdateCCDFrame(int x, int y, int w, int h) override;

protected:
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    // CCD specific functions
    virtual bool StartExposure(float duration) override;
    virtual bool AbortExposure() override;
    void TimerHit();


private:

  // Utility functions
  float CalcTimeLeft();
  void setupParams();
  void grabImage();
  void updateFrameBufferSize();

  // Are we exposing?
  bool InExposure { false };
  // Struct to keep timing
  struct timeval ExpStart { 0, 0 };

  float ExposureRequest { 0 };

};

#endif /* _INDI_MMAL_H */
