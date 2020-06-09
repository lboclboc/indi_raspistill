//#include "RaspiStill-fixed.c"
#define MMAL_COMPONENT_USERDATA_T MMALCamera // NOt quite C++ but thats how its supposed to be used I think.

#include <stdio.h>
#include <mmal_logging.h>
#include <mmal_default_components.h>
#include <util/mmal_util.h>
#include <util/mmal_util_params.h>
#include <bcm_host.h>

#include "mmalcamera.h"
#include "mmallistener.h"

MMALCamera::MMALException::MMALException(const char *text) : std::runtime_error(text)
{
}

void MMALCamera::c_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    MMALCamera *p = dynamic_cast<MMALCamera *>(port->component->userdata);
    p->callback(port, buffer);
}

void  MMALCamera::MMALException::throw_if(bool status, const char *text)
{
    if(status) {
        throw MMALCamera::MMALException(text);
    }
}

MMALCamera::MMALCamera(int n) : cameraNum(n)
{
    // Register our application with the logging system
    vcos_log_register("RaspiStill", VCOS_LOG_CATEGORY);

    create_camera_component();
}

MMALCamera::~MMALCamera()
{
    if (camera) {
        fprintf(stderr, "%s(%s): destroying camera object.\n", __FILE__, __func__);
        mmal_component_disable(camera);
        mmal_component_destroy(camera);
        camera = nullptr;
    }
}

/**
 *  Buffer header callback function for camera still port
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
void MMALCamera::callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    if (port->type == MMAL_PORT_TYPE_OUTPUT)
    {
        int complete = 0;

// FIXME        assert(buffer->type->video.planes == 1);

        if (buffer->length)
        {
            mmal_buffer_header_mem_lock(buffer);

            for(auto *l : listeners)
            {
                l->buffer_received(buffer->data + buffer->offset, buffer->length, buffer->type->video.pitch[0]);
            }

            mmal_buffer_header_mem_unlock(buffer);
        }

        // Now flag if we have completed
        if (buffer->flags & (MMAL_BUFFER_HEADER_FLAG_EOS | MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED)) {
            complete = 1;
        }

        // release buffer back to the pool
        mmal_buffer_header_release(buffer);

        // and send one back to the port (if still open)
        if (port->is_enabled)
        {
            MMAL_STATUS_T status = MMAL_SUCCESS;
            MMAL_BUFFER_HEADER_T *new_buffer;

            new_buffer = mmal_queue_get(pool->queue);

            if (new_buffer)
            {
                status = mmal_port_send_buffer(port, new_buffer);
            }
            if (!new_buffer || status != MMAL_SUCCESS) {
                vcos_log_error("Unable to return a buffer to the camera port");
            }
        }

        if (complete) {
            vcos_semaphore_post(&(complete_semaphore));
        }
    }
    else  {
        fprintf(stderr, "Other port\n");
    }
}


/**
 * Create the camera component, set up its ports.
 * @param shutter_speed Exposure time in us.
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
void MMALCamera::create_camera_component()
{

    MMAL_STATUS_T status = MMAL_EINVAL;

    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
    MMALException::throw_if(status != MMAL_SUCCESS, "Failed to create camera component");
    camera->userdata = this; // c_callback needs this to find this object.

    get_sensor_size();

    MMAL_PARAMETER_INT32_T camera_num_param = {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num_param)}, cameraNum};
    status = mmal_port_parameter_set(camera->control, &camera_num_param.hdr);
    MMALException::throw_if(status != MMAL_SUCCESS, "Could not select camera");
    MMALException::throw_if(camera->output_num == 0, "Camera doesn't have output ports");

    status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, 0);
    MMALException::throw_if(status != MMAL_SUCCESS, "Could not set sensor mode");

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, c_callback);
    MMALException::throw_if(status != MMAL_SUCCESS, "Unable to enable control port");

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config;
        cam_config.hdr.id = MMAL_PARAMETER_CAMERA_CONFIG;
        cam_config.hdr.size = sizeof cam_config;
        cam_config.max_stills_w = width;
        cam_config.max_stills_h = height;
        cam_config.stills_yuv422 = 0;
        cam_config.one_shot_stills = 1;
        cam_config.max_preview_video_w = 1024;
        cam_config.max_preview_video_h = 768;
        cam_config.num_preview_video_frames = 3;
        cam_config.stills_capture_circular_buffer_height = 0;
        cam_config.fast_preview_resume = 0;
        cam_config.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;

        status = mmal_port_parameter_set(camera->control, &cam_config.hdr);
        MMALException::throw_if(status, "Failed to set camera config");
        fprintf(stderr, "Size set to %dx%d\n", cam_config.max_stills_w, cam_config.max_stills_h);
    }

 //    MMALException::throw_if(mmal_port_disable(camera->port[PREVIEW_PORT]), "Failed to disable preview port");
 //   MMALException::throw_if(mmal_port_disable(camera->port[VIDEO_PORT]), "Failed to disable video port");

    // Save cameras default FPG range.
    MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)}, {0, 0}, {0, 0}};
    status = mmal_port_parameter_get(camera->output[MMAL_CAMERA_CAPTURE_PORT], &fps_range.hdr);
    MMALException::throw_if(status != MMAL_SUCCESS, "Failed to get FPS range");

    fps_low = fps_range.fps_low;
    fps_high = fps_range.fps_high;
}

/**
 * Main exposure method.
 *
 * @param speed Shutter speed time in us.
 * @param iso ISO value.
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
int MMALCamera::capture()
{
    int exit_code = 0;
    MMAL_STATUS_T status = MMAL_SUCCESS;
    VCOS_STATUS_T vcos_status;

    // Set all necessary camera parameters (control-port) and enable camera.
    set_camera_parameters();

    camera->port[MMAL_CAMERA_CAPTURE_PORT]->buffer_size = camera->port[MMAL_CAMERA_CAPTURE_PORT]->buffer_size_recommended;

    MMALException::throw_if(mmal_port_parameter_set_boolean(camera->output[MMAL_CAMERA_VIDEO_PORT], MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE), "Failed to turn on zero-copy for video port");

    set_capture_port_format();

    // Enable the camera.
    status = mmal_component_enable(camera);
    MMALException::throw_if(status != MMAL_SUCCESS, "camera component couldn't be enabled");

    // Signalling semaphore.
    vcos_status = vcos_semaphore_create(&complete_semaphore, "RaspiStill-sem", 0);
    MMALException::throw_if(vcos_status != VCOS_SUCCESS, "Failed to create semaphore");

    // Create pool of buffer headers.
    pool = mmal_port_pool_create(camera->output[MMAL_CAMERA_CAPTURE_PORT], camera->output[MMAL_CAMERA_CAPTURE_PORT]->buffer_num, camera->output[MMAL_CAMERA_CAPTURE_PORT]->buffer_size);
    MMALException::throw_if(pool == nullptr, "Failed to allocate buffer pool");

    // Enable the camera output port and tell it its callback function
    status = mmal_port_enable(camera->output[MMAL_CAMERA_CAPTURE_PORT], c_callback);
    MMALException::throw_if(status != MMAL_SUCCESS, "Failed to enable capture port");

    status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_SHUTTER_SPEED, shutter_speed);
    MMALException::throw_if(status, "Failed to set shutter speed");

    unsigned int num = mmal_queue_length(pool->queue);
    for (unsigned int q = 0; q < num; q++)
    {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);

        if (!buffer)
            vcos_log_error("Unable to get a required buffer %d from pool queue", q);

        if (mmal_port_send_buffer(camera->output[MMAL_CAMERA_CAPTURE_PORT], buffer)!= MMAL_SUCCESS)
            vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
    }


    status = mmal_port_parameter_set_boolean(camera->output[MMAL_CAMERA_CAPTURE_PORT], MMAL_PARAMETER_CAPTURE, 1);
    MMALException::throw_if(status != MMAL_SUCCESS, "Failed to start capture");

    // Wait for capture to complete
    // For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
    // even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
    vcos_semaphore_wait(&complete_semaphore);

    vcos_semaphore_delete(&complete_semaphore);

    status = mmal_port_disable(camera->output[MMAL_CAMERA_CAPTURE_PORT]);
    MMALException::throw_if(status != MMAL_SUCCESS, "Failed to disable capture port");

    status = mmal_component_disable(camera);
    MMALException::throw_if(status != MMAL_SUCCESS, "camera component couldn't be disabled");

    mmal_port_pool_destroy(camera->output[MMAL_CAMERA_CAPTURE_PORT], pool);
    pool = nullptr;

    return exit_code;
}

void MMALCamera::set_camera_parameters()
{

    MMALException::throw_if(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SATURATION, MMAL_RATIONAL_T {10, 0}), "Failed to set saturation");
    MMALException::throw_if(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_SHUTTER_SPEED, shutter_speed), "Failed to set shutter speed");
    MMALException::throw_if(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_ISO, iso), "Failed to set ISO");
    MMALException::throw_if(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_ANALOG_GAIN, MMAL_RATIONAL_T {static_cast<int32_t>(gain * 65536), 65536}), "Failed to set analog gain");
    MMALException::throw_if(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_DIGITAL_GAIN, MMAL_RATIONAL_T {1, 1}), "Failed to set digital gain");
    MMALException::throw_if(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_BRIGHTNESS, MMAL_RATIONAL_T{50, 100}), "Failed to set brightness");

    {
        MMAL_PARAMETER_AWBMODE_T param = {{MMAL_PARAMETER_AWB_MODE,sizeof param}, MMAL_PARAM_AWBMODE_AUTO};
        MMALException::throw_if(mmal_port_parameter_set(camera->control, &param.hdr), "Failed to set AWB mode");
    }
    {
        MMAL_PARAMETER_EXPOSUREMODE_T param {{MMAL_PARAMETER_EXPOSURE_MODE, sizeof param}, MMAL_PARAM_EXPOSUREMODE_OFF};
        MMALException::throw_if(mmal_port_parameter_set(camera->control, &param.hdr), "Failed to set exposure mode");
    }
    {
        MMAL_PARAMETER_INPUT_CROP_T crop = {{MMAL_PARAMETER_INPUT_CROP, sizeof(MMAL_PARAMETER_INPUT_CROP_T)}, {}};
        crop.rect.x = (0);
        crop.rect.y = (0);
        crop.rect.width = (0x10000);
        crop.rect.height = (0x10000);
        MMALException::throw_if(mmal_port_parameter_set(camera->control, &crop.hdr), "Failed to set ROI");
    }
}

void MMALCamera::set_capture_port_format()
{
    MMAL_STATUS_T status {MMAL_EINVAL};

    MMALException::throw_if(mmal_port_parameter_set_boolean(camera->output[MMAL_CAMERA_CAPTURE_PORT], MMAL_PARAMETER_ENABLE_RAW_CAPTURE, 1), "Failed to set raw capture");

    if(shutter_speed > 6000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 5, 1000 }, {166, 1000}
                                               };
        status = mmal_port_parameter_set(camera->output[MMAL_CAMERA_CAPTURE_PORT], &fps_range.hdr);
        MMALException::throw_if(status != MMAL_SUCCESS, "Failed to set FPS very low range");
    }
    else if(shutter_speed > 1000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 167, 1000 }, {999, 1000}
                                               };
        status = mmal_port_parameter_set(camera->output[MMAL_CAMERA_CAPTURE_PORT], &fps_range.hdr);
        MMALException::throw_if(status != MMAL_SUCCESS, "Failed to set FPS low range");
    }
#if 0 // FIXME
    else {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},fps_low, fps_high};
        MMALException::throw_if(status != MMAL_SUCCESS, "Failed to set FPS default range");
    }
#endif

    // Set our stills format on the stills (for encoder) port
    MMAL_ES_FORMAT_T *format {camera->output[MMAL_CAMERA_CAPTURE_PORT]->format};
// WORKS    format->encoding = MMAL_ENCODING_I420_SLICE; format->encoding_variant = MMAL_ENCODING_I420;
// BW image:   format->encoding = MMAL_ENCODING_RGB16_SLICE;
//    format->encoding = MMAL_ENCODING_RGB16;
// NOPE    format->encoding = MMAL_ENCODING_RGB24;
// WORKS: 8bit*3    format->encoding = MMAL_ENCODING_RGB24_SLICE;
// WORKS: not raw     format->encoding = MMAL_ENCODING_I420_SLICE;
// WORKLS    format->encoding = MMAL_ENCODING_I422_SLICE; format->encoding_variant = 0;
// HANGS    format->encoding = MMAL_ENCODING_OPAQUE;

    format->encoding = MMAL_ENCODING_RGB24; format->encoding_variant = 0;

    if (!mmal_util_rgb_order_fixed(camera->output[MMAL_CAMERA_CAPTURE_PORT]))
    {
       if (format->encoding == MMAL_ENCODING_RGB24)
          format->encoding = MMAL_ENCODING_BGR24;
       else if (format->encoding == MMAL_ENCODING_BGR24)
          format->encoding = MMAL_ENCODING_RGB24;
    }

    format->encoding_variant = 0;
    format->es->video.width = width;
    format->es->video.height = height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = static_cast<int32_t>(width);
    format->es->video.crop.height = static_cast<int32_t>(height);
    format->es->video.frame_rate.num = 0;
    format->es->video.frame_rate.den = 1;
    format->es->video.par.num = 1;
    format->es->video.par.den = 1;

    status = mmal_port_format_commit(camera->output[MMAL_CAMERA_CAPTURE_PORT]);
    MMALException::throw_if(status != MMAL_SUCCESS, "camera capture port format couldn't be set");
}

/**
 * @brief MMALCamera::get_sensor_size gets default size for camrea.
 * @param camera_num
 * @param camera_name
 * @param len Length of camera_name string
 * @param width
 * @param height
 */
void MMALCamera::get_sensor_size()
{
   MMAL_COMPONENT_T *camera_info;
   MMAL_STATUS_T status;

   // Try to get the camera name and maximum supported resolution
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);

   // Default to the OV5647 setup
   strncpy(cameraName, "OV5647", sizeof cameraName);

   MMAL_PARAMETER_CAMERA_INFO_T param;
   param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
   param.hdr.size = sizeof(param)-4;  // Deliberately undersize to check firmware version
   status = mmal_port_parameter_get(camera->control, &param.hdr);

   if (status != MMAL_SUCCESS)
   {
       // Running on newer firmware
       param.hdr.size = sizeof(param);
       status = mmal_port_parameter_get(camera_info->control, &param.hdr);
       MMALException::throw_if(status != MMAL_SUCCESS, "Failed to get camera parameters.");
       MMALException::throw_if(param.num_cameras <= static_cast<uint32_t>(cameraNum), "Camera number not found.");
      // Take the parameters from the first camera listed.
      width = param.cameras[cameraNum].max_width;
      height = param.cameras[cameraNum].max_height;
      strncpy(cameraName, param.cameras[cameraNum].camera_name, sizeof cameraName);
      cameraName[sizeof cameraName - 1] = 0;
   }
   else {
       // default to OV5647 if nothing detected..
      width = 2592;
      height = 1944;
   }

   mmal_component_destroy(camera_info);
}

#if 1
class FileWriter : public MMALListener
{
public:
    FileWriter(const char *filename);
    virtual ~FileWriter();
    virtual void buffer_received(uint8_t *buffer, size_t len, uint32_t pitch) override;

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

void FileWriter::buffer_received(uint8_t *buffer, size_t len, uint32_t pitch)
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

    MMALCamera cam(0);
    FileWriter fout("/dev/shm/capture");

    cam.add_listener(&fout);
    cam.set_shutter_speed_us(0);   // FIXME: Seconds does not work completely ok.
    cam.set_iso(0);
    cam.set_gain(1);
    cam.capture();
}
#endif
