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

    return true;
}

bool MMALDriver::updateProperties()
{
	// We must ALWAYS call the parent class updateProperties() first
	CCD::updateProperties();


	// Let's get parameters now from CCD
	// Our CCD is an 12 bit CCD, 4054x3040 resolution, with 1.55um square pixels.
	SetCCDParams(4056, 3040, 16, 1.55, 1.55);

	updateFrameBufferSize();

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
    uint8_t *image = PrimaryCCD.getFrameBuffer();
    const char filename[] = "x.jpg";

    // Get width and height
//    int width  = (PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() + 7)/ 8;
//    int height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    // Perform the actual exposure.
    raspi_exposure();
    fprintf(stderr,"Image exposed to %s.\n", filename);

    FILE *fp = fopen(filename, "rb");
    fprintf(stderr, "File opened\n");
    fprintf(stderr, "Start pos: %ld\n", ftell(fp));

    struct stat statbuf;
    if (stat(filename, &statbuf) != 0) {
        LOGF_ERROR("%s: Failed to stat %s file: %s", __FUNCTION__, filename, strerror(errno));
        exit(1);
    }

    if (!fp) {
        LOGF_ERROR("%s: Failed to open %s file: %s", __FUNCTION__, filename, strerror(errno));
        exit(1);
    }
    fprintf(stderr, "Start pos: %ld\n", ftell(fp));

    // FIXME: remove all hardcoding for IMAX477 camera.
    int raw_file_size = 18711040;
    int brcm_header_size = 32768;
    int pixels_to_send = 4056 * 3040; // FIXME: The raw image really has some other strange size.
    fprintf(stderr,"Seeking to %ld\n", statbuf.st_size - raw_file_size);
    if (fseek(fp, statbuf.st_size - raw_file_size, SEEK_SET) != 0)  {
        LOGF_ERROR("%s: Wrong size of %s: %s", __FUNCTION__, filename, strerror(errno));
        exit(1);
    }
    fprintf(stderr, "BRCM pos: %ld\n", ftell(fp));
    if (fseek(fp, brcm_header_size, SEEK_CUR) != 0) {
        LOGF_ERROR("%s: File to small: %s", __FUNCTION__, strerror(errno));
        exit(1);
    }
    fprintf(stderr, "Current pos: %ld\n", ftell(fp));

    size_t bs = 640;
    int i = 0;
    for(i = 0; i < pixels_to_send; i += bs) {
        bool retry = true;
        while(retry)
        {
            retry = false;
            fread(image + i, 1, bs, fp);
            if (ferror(fp)) {
                if (errno != EAGAIN) {
                    LOGF_ERROR("%s: Failed to read from file: %s", __FUNCTION__, strerror(errno));
                    exit(1);
                }
                else {
                    LOGF_ERROR("%s: Failed to read from file: %s, retrying..", __FUNCTION__, strerror(errno));
                    sleep(1);
                }
                retry = true;

            }
        }
        printf("%02x ", image[i]);
    }
    LOGF_DEBUG("%s: Wrote %d bytes: %.640s", __FUNCTION__, i, image);
    char buf[1000];
    char *p = buf;
    for(i = 0; i < 64; i++) {
        p += sprintf(p, "%02x ", image[i + 1024]);
    }
    LOGF_DEBUG("%s: Wrote: %.1024s", __FUNCTION__, buf);


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
