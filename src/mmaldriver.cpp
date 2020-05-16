/**
 * INDI driver for Raspberry Pi 12Mp High Quality camera.
 */
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mmaldriver.h"

extern "C" {
    extern int raspi_exposure();
}

MMALDriver::MMALDriver()
{
    setVersion(1, 0);
}

MMALDriver::~MMALDriver()
{
}

void MMALDriver::assert_framebuffer(INDI::CCDChip *ccd)
{
    int nbuf = (ccd->getXRes() * ccd->getYRes() * (ccd->getBPP() / 8));
    int expected = 4056 * 3040 * 2;
    if (nbuf != expected) {
        LOGF_DEBUG("%s: frame buffer size set to %d", __FUNCTION__, nbuf);
        LOGF_ERROR("%s: Wrong size of framebuffer: %d, expected %d", __FUNCTION__, nbuf, expected);
        exit(1);
    }

    LOGF_DEBUG("%s: frame buffer size set to %d", __FUNCTION__, nbuf);
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

void MMALDriver::ISGetProperties(const char * dev)
{
    if (dev != nullptr && strcmp(getDeviceName(), dev) != 0)
        return;

    INDI::CCD::ISGetProperties(dev);
}

bool MMALDriver::initProperties()
{
    // We must ALWAYS init the properties of the parent class first
    CCD::initProperties();

    addDebugControl();

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

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 1, 10000, 1, false);
    PrimaryCCD.setResolution(4056, 3040);

    return true;
}

bool MMALDriver::updateProperties()
{
	// We must ALWAYS call the parent class updateProperties() first
	CCD::updateProperties();

    PrimaryCCD.setBPP(16);

	// Let's get parameters now from CCD
	// Our CCD is an 12 bit CCD, 4054x3040 resolution, with 1.55um square pixels.
    SetCCDParams(4056, 3040, 16, 1.55L, 1.55L);

    UpdateCCDFrame(0, 0, 4056, 3040);

	return true;
}

// FIXME: doc
bool MMALDriver::UpdateCCDBin(int hor, int ver)
{
	// FIXME: implement UpdateCCDBin
    LOGF_DEBUG("%s: UpdateCCDBin(%d, %d)", __FUNCTION__, hor, ver);

    return true;
}

/**************************************************************************************
 * CCD calls this function when CCD Frame dimension needs to be updated in the hardware.
 * Derived classes should implement this function.
 **************************************************************************************/
bool MMALDriver::UpdateCCDFrame(int x, int y, int w, int h)
{
	LOGF_DEBUG("UpdateCCDFrame(%d, %d, %d, %d", x, y, w, h);

    // FIXME: handle cropping
    if (x + y != 0) {
        LOGF_ERROR("%s: origin offset not supported.", __FUNCTION__);
    }

    // Let's calculate how much memory we need for the primary CCD buffer
    int nbuf = (PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * (PrimaryCCD.getBPP() / 8));

    LOGF_DEBUG("%s: frame buffer size set to %d", __FUNCTION__, nbuf);

    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

/**************************************************************************************
 * Client is asking us to start an exposure
 **************************************************************************************/
bool MMALDriver::StartExposure(float duration)
{
	LOGF_DEBUG("StartEposure(%f)", duration);

    ExposureRequest = duration;

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setExposureDuration(duration);

    gettimeofday(&ExpStart, nullptr);

    InExposure = true;

    // Return true for this will take some time.
    return true;
}


/**************************************************************************************
 * Client is asking us to abort an exposure
 **************************************************************************************/
bool MMALDriver::AbortExposure()
{
	LOGF_DEBUG("AbortEposure()", 0);
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

    if (timeleft < 0) {
        timeleft = 0;
    }

    return timeleft;
}

/**************************************************************************************
 * Main device loop. We check for exposure
 **************************************************************************************/
void MMALDriver::TimerHit()
{
    uint32_t nextTimer = POLLMS;

    if (!isConnected()) {
        return; //  No need to reset timer if we are not connected anymore
    }

    if (InExposure)
    {
        float timeleft = CalcTimeLeft();
        if (timeleft < 0)
            timeleft = 0;

        // Just update time left in client
        PrimaryCCD.setExposureLeft(timeleft);

        // Less than a 1 second away from exposure completion, use shorter timer. If less than 1m, take the image.
        if (timeleft < 1.0) {

            if (timeleft < 0.001) {
				/* We're done exposing */
				IDMessage(getDeviceName(), "Exposure done, downloading image...");

				// Set exposure left to zero
				PrimaryCCD.setExposureLeft(0);

				// We're no longer exposing...
				InExposure = false;

				/* grab and save image */
				grabImage();
            }
            else {
                nextTimer = timeleft * 1000;
            }
        }
    }

    SetTimer(nextTimer);
}

/**************************************************************************************
 * Create a random image and return it to client
 **************************************************************************************/
void MMALDriver::grabImage()
{
    // Let's get a pointer to the frame buffer
    uint16_t *image = reinterpret_cast<uint16_t *>(PrimaryCCD.getFrameBuffer());
    const char filename[] = "x.jpg";

    // Perform the actual exposure.
    // FIXME: add exposure time.
    raspi_exposure();
    fprintf(stderr,"Image exposed to %s.\n", filename);

    FILE *fp = fopen(filename, "rb");
    if (!fp)  {
        LOGF_ERROR("%s: Failed to open %s: %s", __FUNCTION__, filename, strerror(errno));
        exit(1);
    }

    struct stat statbuf;
    if (stat(filename, &statbuf) != 0) {
        LOGF_ERROR("%s: Failed to stat %s file: %s", __FUNCTION__, filename, strerror(errno));
        exit(1);
    }

    // FIXME: remove all hardcoding for IMAX477 camera. Should at least try to parse the BCRM header.
    int raw_file_size = 18711040;
    int brcm_header_size = 32768;
    int pixels_to_send = PrimaryCCD.getFrameBufferSize() / PrimaryCCD.getBPP();

    assert_framebuffer(&PrimaryCCD);

    if (fseek(fp, statbuf.st_size + brcm_header_size - raw_file_size, SEEK_SET) != 0)  {
        LOGF_ERROR("%s: Wrong size of %s: %s", __FUNCTION__, filename, strerror(errno));
        exit(1);
    }

    const int raw_row_size = 6112;
    const int trailer = 28;
    uint8_t row[raw_row_size];
    int i = 0;
    while(i < pixels_to_send)
    {
        fread(row, raw_row_size, 1, fp);
        if (ferror(fp)) {
                LOGF_ERROR("%s: Failed to read from file: %s, retrying..", __FUNCTION__, strerror(errno));
                sleep(1);
        }

        for(int p = 0; p < raw_row_size - trailer; p += 3)
        {
            uint16_t v1 = static_cast<uint16_t>(row[p] + ((row[p+2]&0xF)<<8));
            uint16_t v2 = static_cast<uint16_t>(row[p+1] + ((row[p+2]&0xF0)<<4));
            image[i++ + p] = v1;
            image[i++ + p] = v2;
        }
    }
    LOGF_DEBUG("%s: Wrote %d bytes", __FUNCTION__, i);

    // FIXME: add hw binning
 //   PrimaryCCD.binFrame();

    IDMessage(getDeviceName(), "Download complete.");

    // Let INDI::CCD know we're done filling the image buffer
    ExposureComplete(&PrimaryCCD);
}

bool MMALDriver::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    LOGF_DEBUG("%s: dev=%s, name=%s", __FUNCTION__, dev, name);

    // FIXME: When implementing variables here, make sure to call void MMALDriver::updateFrameBufferSize()

	return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool MMALDriver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    LOGF_DEBUG("%s: dev=%s, name=%s", __FUNCTION__, dev, name);

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool MMALDriver::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    LOGF_DEBUG("%s: dev=%s, name=%s", __FUNCTION__, dev, name);

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}
