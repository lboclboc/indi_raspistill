#ifndef _INDI_MMAL_H
#define _INDI_MMAL_H

#include <indiccd.h>

class MMALDriver : public INDI::CCD
{
public:
	MMALDriver();
	virtual ~MMALDriver();
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

protected:
    bool Connect() override;
    bool Disconnect() override;
    const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE * fp) override;
    virtual void addFITSKeywords(fitsfile * fptr, INDI::CCDChip * targetChip) override;


    // CCD specific functions
    virtual bool StartExposure(float duration) override;
    virtual bool AbortExposure() override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int hor, int ver) override;
    void TimerHit();

private:
  // Utility functions
  double CalcTimeLeft();
  void setupParams();
  void grabImage();
  void updateFrameBufferSize();
  void assert_framebuffer(INDI::CCDChip *ccd);

  // Are we exposing?
  bool InExposure { false };
  // Struct to keep timing
  struct timeval ExpStart { 0, 0 };

  double ExposureRequest { 0 };
#ifdef USE_ISO
  ISwitch mIsoS[4];
  ISwitchVectorProperty mIsoSP;
#endif
#ifdef USE_GAIN
  INumber mGainN[1];
  INumberVectorProperty mGainNP;
#endif
};

#endif /* _INDI_MMAL_H */
