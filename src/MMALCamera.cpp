//#include "RaspiStill-fixed.c"
#define MMAL_COMPONENT_USERDATA_T MMALCamera // NOt quite C++ but thats how its supposed to be used I think.

#include <stdio.h>
#include <mmal_logging.h>
#include <mmal_default_components.h>
#include <util/mmal_util.h>
#include <util/mmal_util_params.h>
#include <bcm_host.h>

#include "MMALCamera.h"

MMALCamera::MMALException::MMALException(const char *text) : std::runtime_error(text)
{
}


void c_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    MMALCamera *p = dynamic_cast<MMALCamera *>(port->component->userdata);
    p->callback(port, buffer);
}

void  MMALCamera::MMALException::throw_if(bool status, const char *text)
{
    if(status) {
        throw new MMALCamera::MMALException(text);
    }
}

MMALCamera::MMALCamera(int n) : cameraNum(n)
{
    bcm_host_init();

    // Register our application with the logging system
    vcos_log_register("RaspiStill", VCOS_LOG_CATEGORY);

    create_camera_component();
}

MMALCamera::~MMALCamera()
{

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
        uint32_t bytes_written = buffer->length;

#ifdef HANDLE_YUV420
buffer	@0x39588	MMAL_BUFFER_HEADER_T
    next	0x0	MMAL_BUFFER_HEADER_T*
    priv	@0x395e8	MMAL_BUFFER_HEADER_PRIVATE_T
    cmd	0	uint32_t
    data	18	uint8_t
    alloc_size	18531840	uint32_t
    length	18531840	uint32_t
    offset	0	uint32_t
    flags	4	uint32_t
    pts	3239232311	int64_t
    dts	-9223372036854775808	int64_t
    type	@0x395c0	MMAL_BUFFER_HEADER_TYPE_SPECIFIC_T
        video	@0x395c0	MMAL_BUFFER_HEADER_VIDEO_SPECIFIC_T
            planes	3	uint32_t
            offset	@0x395c4	uint32_t[4]
                [0]	0	uint32_t
                [1]	12354560	uint32_t
                [2]	15443200	uint32_t
                [3]	0	uint32_t
            pitch	@0x395d4	uint32_t[4]
                [0]	4064	uint32_t
                [1]	2032	uint32_t
                [2]	2032	uint32_t
                [3]	0	uint32_t
            flags	0	uint32_t
    user_data	0x0	void*
#endif
#ifdef HANDLE_RGB24
buffer	@0x39588	MMAL_BUFFER_HEADER_T
    next	0x0	MMAL_BUFFER_HEADER_T*
    priv	@0x395e8	MMAL_BUFFER_HEADER_PRIVATE_T
    cmd	0	uint32_t
    data	227	uint8_t
    alloc_size	24709120	uint32_t
    length	24709120	uint32_t
    offset	0	uint32_t
    flags	4	uint32_t
    pts	6237719365	int64_t
    dts	-9223372036854775808	int64_t
    type	@0x395c0	MMAL_BUFFER_HEADER_TYPE_SPECIFIC_T
        video	@0x395c0	MMAL_BUFFER_HEADER_VIDEO_SPECIFIC_T
            planes	1	uint32_t
            offset	@0x395c4	uint32_t[4]
                [0]	0	uint32_t
                [1]	0	uint32_t
                [2]	0	uint32_t
                [3]	0	uint32_t
            pitch	@0x395d4	uint32_t[4]
                [0]	8128	uint32_t
                [1]	0	uint32_t
                [2]	0	uint32_t
                [3]	0	uint32_t
            flags	0	uint32_t
    user_data	0x0	void*
#endif

        if (buffer->length && file_handle)
        {
            mmal_buffer_header_mem_lock(buffer);

            bytes_written = fwrite(buffer->data, 1, buffer->length, file_handle);

            mmal_buffer_header_mem_unlock(buffer);
        }

        // We need to check we wrote what we wanted - it's possible we have run out of storage.
        if (bytes_written != buffer->length)
        {
            vcos_log_error("Unable to write buffer to file - aborting");
            complete = 1;
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
        else {
            fprintf(stderr, "callback: port not enabled\n");
        }

        if (complete) {
            vcos_semaphore_post(&(complete_semaphore));
        }

     }
}


void MMALCamera::setup_capture_port()
{
    MMAL_STATUS_T status {MMAL_EINVAL};
    MMAL_PORT_T *port {camera->output[CAPTURE_PORT]};


    if(shutter_speed > 6000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 5, 1000 }, {166, 1000}
                                               };
        status = mmal_port_parameter_set(port, &fps_range.hdr);
        MMALException::throw_if(status != MMAL_SUCCESS, "Failed to set FPS");
    }
    else if(shutter_speed > 1000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                { 167, 1000 }, {999, 1000}
                                               };
        status = mmal_port_parameter_set(port, &fps_range.hdr);
        MMALException::throw_if(status != MMAL_SUCCESS, "Failed to set FPS");
    }

    // Set our stills format on the stills (for encoder) port
    MMAL_ES_FORMAT_T *format {port->format};
//    format->encoding = MMAL_ENCODING_I420;
//    format->encoding_variant = MMAL_ENCODING_I420;
    format->encoding = MMAL_ENCODING_RGB16_SLICE; //MMAL_ENCODING_RGB16; // Same as RAW12 format?
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

    status = mmal_port_format_commit(port);
    MMALException::throw_if(status != MMAL_SUCCESS, "camera still format couldn't be set");
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
    MMAL_POOL_T *pool = nullptr;

    try
    {
        /* Create the component */
        status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
        MMALException::throw_if(status != MMAL_SUCCESS, "Failed to create camera component");
        camera->userdata = this; // c_callback needs this to find this object.

        get_sensor_defaults();

        for(int i = 0; i < 3; i++) {
            MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo = { {MMAL_PARAMETER_STEREOSCOPIC_MODE, sizeof(stereo)},
               MMAL_STEREOSCOPIC_MODE_NONE, MMAL_FALSE, MMAL_FALSE };

            status = mmal_port_parameter_set(camera->output[i], &stereo.hdr);
            MMALException::throw_if(status != MMAL_SUCCESS, "Failed to set stereoscopic mode");
        }

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
            cam_config.stills_yuv422 = 1;
            cam_config.one_shot_stills = 1;
            cam_config.max_preview_video_w = 1024;
            cam_config.max_preview_video_h = 768;
            cam_config.num_preview_video_frames = 3;
            cam_config.stills_capture_circular_buffer_height = 0;
            cam_config.fast_preview_resume = 0;
            cam_config.use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC;

            mmal_port_parameter_set(camera->control, &cam_config.hdr);
            fprintf(stderr, "Size set to %dx%d\n", cam_config.max_stills_w, cam_config.max_stills_h);
        }

        setup_capture_port();

        MMAL_PORT_T *port { camera->port[CAPTURE_PORT] };
        port->buffer_size = port->buffer_size_recommended;

        /* Ensure there are enough buffers to avoid dropping frames */
        if (port->buffer_num < 3) {
            port->buffer_num = 3;
        }

        /* Create pool of buffer headers for the output port to consume */
        pool = mmal_port_pool_create(port, port->buffer_num, port->buffer_size);
        MMALException::throw_if(!pool, "Failed to create buffer header pool for camera output port");

        // Close un-used ports.
//        mmal_port_disable(camera->output[VIDEO_PORT]);
//        mmal_port_disable(camera->output[PREVIEW_PORT]);
    }
    catch(MMALException &e)
    {
        if (camera) {
            mmal_component_destroy(camera);
            camera = nullptr;
        }
        throw(e);
    }
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
    char filename[] = "/dev/shm/indi_raspistill_capture.jpg";
    int exit_code = 0;
    MMAL_STATUS_T status = MMAL_SUCCESS;
    VCOS_STATUS_T vcos_status;

    set_camera_parameters();

    // Signalling semaphore.
    file_handle = nullptr;
    vcos_status = vcos_semaphore_create(&complete_semaphore, "RaspiStill-sem", 0);
    MMALException::throw_if(vcos_status != VCOS_SUCCESS, "Failed to create semaphore");

    // Enable camera component.
    status = mmal_component_enable(camera);
    MMALException::throw_if(status != MMAL_SUCCESS, "camera component couldn't be enabled");

    // Create pool of buffer headers.
    pool = mmal_port_pool_create(camera->output[CAPTURE_PORT], camera->output[CAPTURE_PORT]->buffer_num, camera->output[CAPTURE_PORT]->buffer_size);
    MMALException::throw_if(pool == nullptr, "Failed to allocate buffer pool");

    // Open the file
    file_handle = fopen(filename, "wb");
    MMALException::throw_if(file_handle == nullptr, "Failed to open the capture file");

    // Enable the camera output port and tell it its callback function
    status = mmal_port_enable(camera->output[MMAL_CAMERA_CAPTURE_PORT], c_callback);
    MMALException::throw_if(status != MMAL_SUCCESS, "Failed to enable camera port");

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

    // Ensure we don't die if get callback with no open file
    fclose(file_handle);

    file_handle = nullptr;

    vcos_semaphore_delete(&complete_semaphore);

    if (camera) {
        mmal_component_disable(camera);
    }

    return exit_code;
}

void MMALCamera::set_camera_parameters()
{
    MMALException::throw_if(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_SATURATION, MMAL_RATIONAL_T {10, 0}), "Failed to set saturation");
    MMALException::throw_if(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_SHUTTER_SPEED, shutter_speed), "Failed to set shutter speed");
    MMALException::throw_if(mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_ISO, iso), "Failed to set ISO");
    MMALException::throw_if(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_ANALOG_GAIN, MMAL_RATIONAL_T {static_cast<int32_t>(gain) * 65536, 65536}), "Failed to set analog gain");
    MMALException::throw_if(mmal_port_parameter_set_rational(camera->control, MMAL_PARAMETER_DIGITAL_GAIN, MMAL_RATIONAL_T {1, 1}), "Failed to set digital gain");
    MMALException::throw_if(mmal_port_parameter_set_boolean(camera->output[MMAL_CAMERA_CAPTURE_PORT], MMAL_PARAMETER_ENABLE_RAW_CAPTURE, 1), "Failed to set raw capture");

    {
        MMAL_PARAMETER_AWBMODE_T param = {{MMAL_PARAMETER_AWB_MODE,sizeof param}, MMAL_PARAM_AWBMODE_AUTO};
        MMALException::throw_if(mmal_port_parameter_set(camera->control, &param.hdr), "Failed to set AWB off");
    }
    {
        MMAL_PARAMETER_EXPOSUREMODE_T param {{MMAL_PARAMETER_EXPOSURE_MODE, sizeof param}, MMAL_PARAM_EXPOSUREMODE_OFF};
        MMALException::throw_if(mmal_port_parameter_set(camera->control, &param.hdr), "Failed to set exposure mode");
    }
}

/**
 * @brief MMALCamera::get_sensor_defaults gets default parameters for camrea.
 * @param camera_num
 * @param camera_name
 * @param len Length of camera_name string
 * @param width
 * @param height
 */
void MMALCamera::get_sensor_defaults()
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

int main(int argc, char **argv)
{
    MMALCamera cam(0);
    cam.set_shutter_speed_us(300000);
    cam.set_iso(0);
    cam.set_gain(1);
    cam.capture();
}
