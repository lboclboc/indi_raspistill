/**
 * INDI driver for Raspberry Pi 12Mp High Quality camera.
 */
#include <string.h>

#include "mmaldriver.h"

MMALDriver::MMALDriver()
{
    setVersion(1, 0);
}

MMALDriver::~MMALDriver()
{
}

/**************************************************************************************
 * Client is asking us to establish connection to the device
 **************************************************************************************/
bool MMALDriver::Connect()
{
    DEBUG(INDI::Logger::DBG_SESSION, "MMAL device connected successfully!");
    SetTimer(POLLMS);
    return true;
}

/**************************************************************************************
 * Client is asking us to terminate connection to the device
 **************************************************************************************/
bool MMALDriver::Disconnect()
{
    DEBUG(INDI::Logger::DBG_SESSION, "MMAL device disconnected successfully!");
    return true;
}

/**************************************************************************************
 * INDI is asking us for our default device name
 **************************************************************************************/
const char * MMALDriver::getDefaultName()
{
    return "MMAL Device";
}

bool MMALDriver::initProperties()
{
    // We must ALWAYS init the properties of the parent class first
    CCD::initProperties();

    SetCCDCapability(0
		| CCD_CAN_BIN			// Does the CCD support binning?
		| CCD_CAN_SUBFRAME		// Does the CCD support setting ROI?
	//	| CCD_CAN_ABORT			// Can the CCD exposure be aborted?
	//	| CCD_HAS_GUIDE_HEAD	// Does the CCD have a guide head?
	//	| CCD_HAS_ST4_PORT 		// Does the CCD have an ST4 port?
	//	| CCD_HAS_SHUTTER 		// Does the CCD have a mechanical shutter?
	//	| CCD_HAS_COOLER 		// Does the CCD have a cooler and temperature control?
		| CCD_HAS_BAYER 		// Does the CCD send color data in bayer format?
	//	| CCD_HAS_STREAMING 	// Does the CCD support live video streaming?
	//	| CCD_HAS_WEB_SOCKET 	// Does the CCD support web socket transfers?
	);


    setDefaultPollingPeriod(500);

    return true;
}

bool MMALDriver::updateProperties()
{
	// We must ALWAYS call the parent class updateProperties() first
	DefaultDevice::updateProperties();

	// If we are connected, we define the property to the client.
	if (isConnected())
	{
		// Let's get parameters now from CCD
	    // Our CCD is an 12 bit CCD, 4054x3040 resolution, with 1.55um square pixels.
	    SetCCDParams(4054, 3040, 12, 1.55, 1.55);

		// Start the timer
		SetTimer(POLLMS);
	}

	return true;
}

/**************************************************************************************
 * Calculate memory need for new frame size and updates PrimaryCCD.
 **************************************************************************************/
void MMALDriver::updateFrameBufferSize()
{
    LOGF_DEBUG("%s: frame bytes %d", __FUNCTION__, PrimaryCCD.getFrameBufferSize());
	ulong frameBytes;

	frameBytes = PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * ((PrimaryCCD.getBPP() + 7) / 8);

    PrimaryCCD.setFrameBufferSize(frameBytes);
}

/**************************************************************************************
 * CCD calls this function when CCD Frame dimension needs to be updated in the hardware.
 * Derived classes should implement this function.
 **************************************************************************************/
bool MMALDriver::UpdateCCDFrame(int x, int y, int w, int h)
{
	LOGF_DEBUG("UpdateCCDFrame(%d, %d, %d, %d", x, y, w, h);

    // Let's calculate how much memory we need for the primary CCD buffer
    int nbuf = (PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() + 7) / 8;

    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

/**************************************************************************************
 * Client is asking us to start an exposure
 **************************************************************************************/
bool MMALDriver::StartExposure(float duration)
{
    ExposureRequest = duration;

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setExposureDuration(duration);

    gettimeofday(&ExpStart, nullptr);

    InExposure = true;

    // We're done
    return true;
}


/**************************************************************************************
 * Client is asking us to abort an exposure
 **************************************************************************************/
bool MMALDriver::AbortExposure()
{
    InExposure = false;
    // FIXME: Needs to be handled.
    return true;
}

/**************************************************************************************
 * How much longer until exposure is done?
 **************************************************************************************/
float MMALDriver::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now { 0, 0 };
    gettimeofday(&now, nullptr);

    timesince = (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = ExposureRequest - timesince;
    return timeleft;
}

/**************************************************************************************
 * Main device loop. We check for exposure
 **************************************************************************************/
void MMALDriver::TimerHit()
{
    long timeleft;

    if (!isConnected())
        return; //  No need to reset timer if we are not connected anymore

    if (InExposure)
    {
        timeleft = CalcTimeLeft();

        // Less than a 0.1 second away from exposure completion
        // This is an over simplified timing method, check CCDSimulator and simpleCCD for better timing checks
        if (timeleft < 0.1)
        {
            /* We're done exposing */
            IDMessage(getDeviceName(), "Exposure done, downloading image...");

            // Set exposure left to zero
            PrimaryCCD.setExposureLeft(0);

            // We're no longer exposing...
            InExposure = false;

            /* grab and save image */
            grabImage();
        }
        else
            // Just update time left in client
            PrimaryCCD.setExposureLeft(timeleft);
    }

    SetTimer(POLLMS);
}

/**************************************************************************************
 * Create a random image and return it to client
 **************************************************************************************/
void MMALDriver::grabImage()
{
    // Let's get a pointer to the frame buffer
    uint8_t *image = PrimaryCCD.getFrameBuffer();

    // Get width and height
    int width  = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
    int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    // Fill buffer with random pattern
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            image[i * width + j] = rand() % 255;

    IDMessage(getDeviceName(), "Download complete.");

    // Let INDI::CCD know we're done filling the image buffer
    ExposureComplete(&PrimaryCCD);
}

bool MMALDriver::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
	return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}
