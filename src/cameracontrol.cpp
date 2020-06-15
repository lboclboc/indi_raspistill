#include <stdio.h>
#include <bcm_host.h>
#include <stdexcept>

#include <mmal_logging.h>
#include "cameracontrol.h"
#include "mmalexception.h"
#include "pixellistener.h"

CameraControl::CameraControl()
{
    camera.reset(new MMALCamera(0));
    encoder.reset(new MMALEncoder());
    encoder->add_port_listener(this);

    camera->connect(2, encoder.get(), 0); // Connected the capture port to the encoder.

    encoder->activate();
}

CameraControl::~CameraControl()
{
    fprintf(stderr, "CameraControl deleted\n");
}

/**
 * @brief CameraControl::buffer_received Buffer received to what ever we are listened to.
 * This method is responsible to feed image data to the indi-driver code.
 * @param port
 * @param buffer
 */
void CameraControl::buffer_received(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    if (port->type == MMAL_PORT_TYPE_OUTPUT)
    {
        int complete = 0;

        assert(buffer->type->video.planes == 1);

        if (buffer->length) {
            for(auto l : pixel_listeners) {
                l->pixels_received(buffer->data + buffer->offset, buffer->length);
            }
        }

        // Now flag if we have completed
        if (buffer->flags & (MMAL_BUFFER_HEADER_FLAG_EOS | MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED)) {
            complete = 1;
        }

        if (complete) {
            vcos_semaphore_post(&(complete_semaphore));
        }
    }
    else  {
        fprintf(stderr, "%s(%s): unsupported port type: %d\n", __FILE__, __func__, port->type);
    }
}

/**
 * @brief CameraControl::capture Perform the actual capture.
 */
void CameraControl::capture()
{
    VCOS_STATUS_T vcos_status;
    // Signalling semaphore used in the callback function.
    vcos_status = vcos_semaphore_create(&complete_semaphore, "RaspiStill-sem", 0);
    MMALException::throw_if(vcos_status, "Failed to create semaphore");

    camera->capture();

    // Wait for capture to complete
    vcos_semaphore_wait(&complete_semaphore);
    vcos_semaphore_delete(&complete_semaphore);
}


#if 0
class FileWriter : public PixelListener
{
public:
    FileWriter(const char *filename);
    virtual ~FileWriter();
    virtual void pixels_received(uint8_t *buffer, size_t len, uint32_t pitch) override;

private:
    FILE *fp;
    uint32_t pitch {};
    int rows {};
};

FileWriter::FileWriter(const char *filename)
{
    unlink("/dev/shm/capture");
    fp = fopen(filename, "w");
}

FileWriter::~FileWriter()
{
    fclose(fp);
    fprintf(stderr, "%d rows with pitch %d\n", rows, pitch);
}

void FileWriter::pixels_received(uint8_t *buffer, size_t len, uint32_t pitch)
{
    if(buffer != nullptr && len != 0 && pitch != 0) {
        fwrite(buffer, len, 1, fp);
        rows += len / pitch;
        this->pitch = pitch;
    }
}

int main(int argc, char **argv)
{
    bcm_host_init();

    // Register our application with the logging system
    vcos_log_register("indi_mmal", VCOS_LOG_CATEGORY);

    (void)argc;
    (void)argv;

    CameraControl cam;
    FileWriter fout("/dev/shm/capture");

    cam.add_pixel_listener(&fout);
    cam.get_camera()->set_shutter_speed_us(300000);   // FIXME: Seconds does not work completely ok.
    cam.get_camera()->set_iso(0);
    cam.get_camera()->set_gain(1);
    cam.capture();
}
#endif
