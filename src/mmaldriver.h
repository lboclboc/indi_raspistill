#ifndef _INDI_MMAL_H
#define _INDI_MMAL_H

#include <indiccd.h>
#include "cameracontrol.h"
#include "pixellistener.h"

#undef USE_ISO
#define DEFAULT_ISO 400

class JpegPipeline;
class BroadcomPipeline;
class Raw12ToBayer16Pipeline;

class MMALDriver : public INDI::CCD, PixelListener
{
public:
	MMALDriver();
	virtual ~MMALDriver();

    // INDI::CCD Overrides
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual void pixels_received(uint8_t *buffer, size_t length) override;


protected:
    // INDI::CCD overrides.
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool saveConfigItems(FILE * fp) override;
    virtual void addFITSKeywords(fitsfile * fptr, INDI::CCDChip * targetChip) override;
    virtual bool StartExposure(float duration) override;
    virtual bool AbortExposure() override;
    virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
    virtual bool UpdateCCDBin(int hor, int ver) override;
    virtual void TimerHit() override;

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
  uint8_t *image_buffer_pointer {0};

#ifdef USE_ISO
  ISwitch mIsoS[4];
  ISwitchVectorProperty mIsoSP;
#endif
  INumber mGainN[1];
  INumberVectorProperty mGainNP;

  std::unique_ptr<CameraControl> camera_control; // Controller object for the camera communication.
  std::unique_ptr<JpegPipeline> receiver; // Start of pipeline that recieved raw data from camera.
  std::unique_ptr<BroadcomPipeline> brcm; // Second in pipe.
  std::unique_ptr<Raw12ToBayer16Pipeline> raw12; // Final in pipe, converting RAW12 to Bayer 16 bits.
};

extern std::unique_ptr<MMALDriver> mmalDevice;
#endif /* _INDI_MMAL_H */
