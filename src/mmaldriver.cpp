/**
 * INDI driver for Raspberry Pi 12Mp High Quality camera.
 */
#include <algorithm>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include "mmaldriver.h"

MMALDriver::MMALDriver() : INDI::CCD()
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

bool MMALDriver::saveConfigItems(FILE * fp)
{
    INDI::CCD::saveConfigItems(fp);

#ifdef USE_ISO
    // ISO Settings
    if (mIsoSP.nsp > 0) {
        IUSaveConfigSwitch(fp, &mIsoSP);
    }
#endif

    // Gain Settings
    IUSaveConfigNumber(fp, &mGainNP);

    return true;
}

void MMALDriver::addFITSKeywords(fitsfile * fptr, INDI::CCDChip * targetChip)
{
    INDI::CCD::addFITSKeywords(fptr, targetChip);

#ifdef USE_ISO // FIXME
    int status = 0;
    if (mIsoSP.nsp > 0)
    {
        ISwitch * onISO = IUFindOnSwitch(&mIsoSP);
        if (onISO)
        {
            int isoSpeed = atoi(onISO->label);
            if (isoSpeed > 0) {
                fits_update_key_s(fptr, TUINT, "ISOSPEED", &isoSpeed, "ISO Speed", &status);
            }
        }
    }
#endif

}

/**************************************************************************************
 * Client is asking us to establish connection to the device
 **************************************************************************************/
bool MMALDriver::Connect()
{
    DEBUG(INDI::Logger::DBG_SESSION, "MMAL device connected successfully!");

    camera.reset(new MMALCamera());

    camera->add_listener(this);

    SetTimer(POLLMS);

    float pixel_size_x = 0, pixel_size_y = 0;

    if (!strcmp(camera->get_name(), "imx477")) {
        pixel_size_x = pixel_size_y = 1.55F;
    }
    else {
        LOGF_WARN("%s: Unknown camera name: %s\n", __FUNCTION__, camera->get_name());
        return false;
    }
    SetCCDParams(static_cast<int>(camera->get_width()), static_cast<int>(camera->get_height()), 16, pixel_size_x, pixel_size_y);
    // Should really not be called by the client.
    UpdateCCDFrame(0, 0, static_cast<int>(camera->get_width()), static_cast<int>(camera->get_height()));

    return true;
}

/**************************************************************************************
 * Client is asking us to terminate connection to the device
 **************************************************************************************/
bool MMALDriver::Disconnect()
{
    DEBUG(INDI::Logger::DBG_SESSION, "MMAL device disconnected successfully!");

    camera.reset(nullptr);

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
    INDI::CCD::initProperties();

    // ISO switches
#ifdef USE_ISO
    IUFillSwitch(&mIsoS[0], "ISO_100", "100", ISS_OFF);
    IUFillSwitch(&mIsoS[1], "ISO_200", "200", ISS_OFF);
    IUFillSwitch(&mIsoS[2], "ISO_400", "400", ISS_ON);
    IUFillSwitch(&mIsoS[3], "ISO_800", "800", ISS_OFF);
    IUFillSwitchVector(&mIsoSP, mIsoS, 4, getDeviceName(), "CCD_ISO", "ISO", IMAGE_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
#endif

    // CCD Gain
    IUFillNumber(&mGainN[0], "GAIN", "Gain", "%.f", 1, 16.0, 1, 1);
    IUFillNumberVector(&mGainNP, mGainN, 1, getDeviceName(), "CCD_GAIN", "Gain", IMAGE_SETTINGS_TAB, IP_RW, 60, IPS_IDLE);

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

    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 1000, .0001, false);

    return true;
}

bool MMALDriver::updateProperties()
{
	// We must ALWAYS call the parent class updateProperties() first
    INDI::CCD::updateProperties();

    IUSaveText(&BayerT[2], "BGGR");

    LOGF_DEBUG("%s: updateProperties()", __FUNCTION__);

    if (isConnected())  {
#ifdef USE_ISO
        if (mIsoSP.nsp > 0) {
            defineSwitch(&mIsoSP);
        }
#endif
        defineNumber(&mGainNP);
    }
    else {
#ifdef USE_ISO
        if (mIsoSP.nsp > 0) {
            deleteProperty(mIsoSP.name);
        }
#endif

        deleteProperty(mGainNP.name);
    }

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
fprintf(stderr, "UpdateCCDFrame(%d, %d, %d, %d", x, y, w, h);

    // FIXME: handle cropping
    if (x + y != 0) {
        LOGF_ERROR("%s: origin offset not supported.", __FUNCTION__);
    }

    // Let's calculate how much memory we need for the primary CCD buffer
    int xRes = PrimaryCCD.getXRes();
    int yRes = PrimaryCCD.getYRes();
    int bpp = PrimaryCCD.getBPP();
    int nbuf = (xRes * yRes * (bpp / 8));
    // If I422, we need double that for UV data. FIXME
    nbuf *= 2;

    LOGF_DEBUG("%s: frame buffer size set to %d", __FUNCTION__, nbuf);

    PrimaryCCD.setFrameBufferSize(nbuf);

    return true;
}

/**************************************************************************************
 * Client is asking us to start an exposure
 * \param duration exposure time in seconds.
 **************************************************************************************/
bool MMALDriver::StartExposure(float duration)
{

    if (InExposure)
    {
        LOG_ERROR("Camera is already exposing.");
        return false;
    }

    LOGF_DEBUG("StartEposure(%f)", static_cast<double>(duration));

    ExposureRequest = static_cast<double>(duration);

    // Since we have only have one CCD with one chip, we set the exposure duration of the primary CCD
    PrimaryCCD.setExposureDuration(static_cast<double>(duration));

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
    // FIXME: AbortExposure needs to be handled.
    return true;
}

/**************************************************************************************
 * How much longer until exposure is done?
 **************************************************************************************/
double MMALDriver::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now { 0, 0 };
    gettimeofday(&now, nullptr);

    timesince = static_cast<double>(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                static_cast<double>(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
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
        double timeleft = CalcTimeLeft();
        if (timeleft < 0)
            timeleft = 0;

        // FIXME: make capturing occur in separate thread.
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
                nextTimer = static_cast<uint32_t>(timeleft * 1000);
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
    // Perform the actual exposure.
    // FIXME: Should be a separate thread, this thread should just be waiting.

    int isoSpeed = 0;

#ifdef USE_ISO
    isoSpeed = DEFAULT_ISO;
    ISwitch * onISO = IUFindOnSwitch(&mIsoSP);
    if (onISO) {
        isoSpeed = atoi(onISO->label);
    }
#endif
    double gain = mGainN[0].value;

    image_buffer_pointer = PrimaryCCD.getFrameBuffer();
    try {
        camera->set_iso(isoSpeed);
        camera->set_gain(gain);
        camera->set_shutter_speed_us(static_cast<long>(ExposureTime * 1E6F));
        camera->capture();
    } catch (MMALCamera::MMALException e) {
        fprintf(stderr, "Caugh camera exception: %s\n", e.what());
        return;
    }

    IDMessage(getDeviceName(), "Download complete.");

    // Let INDI::CCD know we're done filling the image buffer
    ExposureComplete(&PrimaryCCD);
}

/**
 * @brief MMALDriver::buffer_received Gets called when a new buffer is received from the camera.
 * This method convers the pixels if needed and stores the recived pixel into the primary
 * frame buffer.
 * Assumes that it will be called with complete rows.
 * @param buffer
 */
void MMALDriver::buffer_received(uint8_t *buffer, size_t length, uint32_t pitch)
{
    std::unique_lock<std::mutex> guard(ccdBufferLock);

    uint8_t *end_of_image = PrimaryCCD.getFrameBuffer() + PrimaryCCD.getFrameBufferSize();

    uint32_t max_x = std::min(static_cast<uint32_t>(PrimaryCCD.getXRes()), pitch);

    for(uint8_t *row = buffer; row < buffer + length - pitch; row += pitch)
    {
        for(uint32_t x = 0; x <  max_x && image_buffer_pointer < end_of_image; x++)
        {
            *image_buffer_pointer++ = 0;        // LSB
            *image_buffer_pointer++ = row[x];   // MSB
        }
    }

    guard.unlock();
}

bool MMALDriver::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    LOGF_DEBUG("%s: dev=%s, name=%s", __FUNCTION__, dev, name);

    // ignore if not ours
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return false;

    if (INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n))
        return true;

    ISwitchVectorProperty *svp = getSwitch(name);
    if (!isConnected()) {
         svp->s = IPS_ALERT;
         IDSetSwitch(svp, "Cannot change property while device is disconnected.");
         return false;
    }

    // FIXME: When implementing variables here, make sure to call void MMALDriver::updateFrameBufferSize()
#ifdef USE_ISO
    if (!strcmp(name, mIsoSP.name))
    {
        if (IUUpdateSwitch(&mIsoSP, states, names, n) < 0) {
            return false;
        }

        IDSetSwitch(&mIsoSP, nullptr);
        return true;
    }
#endif

    return false;
}

bool MMALDriver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    LOGF_DEBUG("%s: dev=%s, name=%s", __FUNCTION__, dev, name);

    // ignore if not ours
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return false;

    if (INDI::CCD::ISNewNumber(dev, name, values, names, n)) {
        return true;
    }

    if (!strcmp(name, mGainNP.name))
    {
        IUUpdateNumber(&mGainNP, values, names, n);
        mGainNP.s = IPS_OK;
        IDSetNumber(&mGainNP, nullptr);
        return true;
    }

    return false;
}

bool MMALDriver::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    LOGF_DEBUG("%s: dev=%s, name=%s", __FUNCTION__, dev, name);

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

