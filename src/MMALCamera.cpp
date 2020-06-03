//#include "RaspiStill-fixed.c"
#include <stdio.h>
#include <mmal_logging.h>
#include <mmal_default_components.h>
#include <bcm_host.h>

#include "MMALCamera.h"

void c_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    MMALCamera *p = dynamic_cast<MMALCamera *>(port->userdata);
    p->callback(port, buffer);
}

MMALCamera::MMALCamera()
{

}

MMALCamera::~MMALCamera()
{

}

/**
 *  Buffer header callback function for camera
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
void MMALCamera::callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    int complete = 0;

    uint32_t bytes_written = buffer->length;

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

/**
 * Create the camera component, set up its ports
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
void MMALCamera::create_camera_component()
{
    MMAL_ES_FORMAT_T *format = nullptr;
    MMAL_PORT_T *still_port = nullptr;
    MMAL_STATUS_T status = MMAL_EINVAL;
    MMAL_POOL_T *pool = nullptr;

    /* Create the component */
    try
    {
        status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
        throw_if(status != MMAL_SUCCESS, "Failed to create camera component");

        MMAL_PARAMETER_INT32_T camera_num = {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->common_settings.cameraNum};
        status = mmal_port_parameter_set(camera->control, &camera_num.hdr);
        throw_if(status != MMAL_SUCCESS, "Could not select camera : error %d", status);
        throw_if(camera->output_num == 0, "Camera doesn't have output ports");

        status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state->common_settings.sensor_mode);
        throw_if(status != MMAL_SUCCESS, "Could not set sensor mode : error %d", status);

        still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

        // Enable the camera, and tell it its control callback function
        status = mmal_port_enable(camera->control, default_camera_control_callback);
        throw_if(status != MMAL_SUCCESS, "Unable to enable control port : error %d", status);

        //  set up the camera configuration
        {
            MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
                { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
                .max_stills_w = state->common_settings.width,
                .max_stills_h = state->common_settings.height,
                .stills_yuv422 = 1,
                .one_shot_stills = 1,
                .max_preview_video_w = state->preview_parameters.previewWindow.width,
                .max_preview_video_h = state->preview_parameters.previewWindow.height,
                .num_preview_video_frames = 3,
                .stills_capture_circular_buffer_height = 0,
                .fast_preview_resume = 0,
                .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
            };

            if (state->fullResPreview)
            {
                cam_config.max_preview_video_w = state->common_settings.width;
                cam_config.max_preview_video_h = state->common_settings.height;
            }

            mmal_port_parameter_set(camera->control, &cam_config.hdr);
    fprintf(stderr, "Size set to %dx%d\n", cam_config.max_stills_w, cam_config.max_stills_h);
        }

        raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

        // Now set up the port formats

        format = still_port->format;

        if(state->camera_parameters.shutter_speed > 6000000)
        {
            MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                    { 5, 1000 }, {166, 1000}
                                                   };
            mmal_port_parameter_set(still_port, &fps_range.hdr);
        }
        else if(state->camera_parameters.shutter_speed > 1000000)
        {
            MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                    { 167, 1000 }, {999, 1000}
                                                   };
            mmal_port_parameter_set(still_port, &fps_range.hdr);
        }

        // Set our stills format on the stills (for encoder) port
        format->encoding = MMAL_ENCODING_OPAQUE;
        format->encoding_variant = MMAL_ENCODING_I420;
        format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
        format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = state->common_settings.width;
        format->es->video.crop.height = state->common_settings.height;
        format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;

        status = mmal_port_format_commit(still_port);

        if (status != MMAL_SUCCESS) {
            vcos_log_error("camera still format couldn't be set");
            goto error;
        }

        still_port->buffer_size = still_port->buffer_size_recommended;

        /* Ensure there are enough buffers to avoid dropping frames */
        if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM) {
            still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
        }

        /* Create pool of buffer headers for the output port to consume */
        pool = mmal_port_pool_create(still_port, still_port->buffer_num, still_port->buffer_size);
        if (!pool)
        {
           vcos_log_error("Failed to create buffer header pool for camera output port %s", still_port->name);
        }
    }
    catch(Exception e)
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
 * @param exposure Shutter time in us.
 * @param iso ISO value.
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
int MMALCamera::exposure(long exposure, int iso)
{
    // Our main data storage vessel..
    char filename[] = "/dev/shm/indi_raspistill_capture.jpg";
    int exit_code = EX_OK;

fprintf(stderr, "raspi_exposure called\n");

    MMAL_STATUS_T status = MMAL_SUCCESS;
    MMAL_PORT_T *camera_still_port = nullptr;

    bcm_host_init();
fprintf(stderr, "bcm_host_init done\n");

    // Register our application with the logging system
    vcos_log_register("RaspiStill", VCOS_LOG_CATEGORY);
fprintf(stderr, "vcos_log_register done\n");

    signal(SIGINT, default_signal_handler);

    // Disable USR1 and USR2 for the moment - may be reenabled if go in to signal capture mode
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);


    default_status(&state);
fprintf(stderr, "default_status done\n");

    state.preview_parameters.wantPreview = 0;
    state.timeout = 1;
    state.common_settings.verbose = 0;
    state.camera_parameters.shutter_speed = exposure * 1000000L;
    state.frameNextMethod = FRAME_NEXT_IMMEDIATELY;

    if (state.timeout == -1) {
        state.timeout = 5000;
    }

    // Setup for sensor specific parameters
    get_sensor_defaults(state.common_settings.cameraNum, state.common_settings.camera_name,
                        &state.common_settings.width, &state.common_settings.height);

fprintf(stderr, "get_sensor_defaults done\n");
    // FIXME: my_create_camera_component does not handle longer exposure than 1s
    if ((status = my_create_camera_component(&state)) != MMAL_SUCCESS)
    {
        vcos_log_error("%s: Failed to create camera component", __func__);
        exit_code = EX_SOFTWARE;
    }
    else
    {
        PORT_USERDATA callback_data;
        VCOS_STATUS_T vcos_status;

        // Set up our userdata - this is passed though to the callback where we need the information.
        // Null until we open our filename
        callback_data.file_handle = NULL;
        callback_data.pstate = &state;
        vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "RaspiStill-sem", 0);
        vcos_assert(vcos_status == VCOS_SUCCESS);

fprintf(stderr, "my_create_camera_component done\n");

        camera_still_port   = state.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];

        /* Enable component */
        status = mmal_component_enable(state.camera_component);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("camera component couldn't be enabled");
            goto error;
        }
        fprintf(stderr, "camera enabled\n");

        if (status == MMAL_SUCCESS)
        {
            VCOS_STATUS_T vcos_status;

            // Set up our userdata - this is passed though to the callback where we need the information.
            // Null until we open our filename
            callback_data.file_handle = NULL;
            callback_data.pstate = &state;
            vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "RaspiStill-sem", 0);
fprintf(stderr, "vcos_semaphore_create done\n");

            vcos_assert(vcos_status == VCOS_SUCCESS);

            FILE *output_file = NULL;

            // Open the file
            output_file = fopen(filename, "wb");
            if (!output_file)
            {
                // Notify user, carry on but discarding encoded output buffers
                vcos_log_error("%s: Error opening output file: %s\nNo output file will be generated\n", __func__, filename);
            }
            callback_data.file_handle = output_file;

            unsigned int num, q;

            if (mmal_port_parameter_set_boolean(camera_still_port, MMAL_PARAMETER_ENABLE_RAW_CAPTURE, 1) != MMAL_SUCCESS)
            {
                vcos_log_error("RAW was requested, but failed to enable");
            }
fprintf(stderr, "mmal_port_parameter_set_boolean done\n");

            // There is a possibility that shutter needs to be set each loop.
            if (mmal_status_to_int(mmal_port_parameter_set_uint32(camera_component->control, MMAL_PARAMETER_SHUTTER_SPEED, state.camera_parameters.shutter_speed)) != MMAL_SUCCESS)
                vcos_log_error("Unable to set shutter speed");

            // Enable the camera output port and tell it its callback function
            camera_still_port->userdata = (struct MMAL_PORT_USERDATA_T *)&this;
            status = mmal_port_enable(camera_still_port, camera_callback);
            if (status != MMAL_SUCCESS) {
                vcos_log_error("%s: Failed to enable camera port", __func__);
            }

            // Send all the buffers to the encoder output port

            // FIXME: encoder_pool is 0 here.
            num = mmal_queue_length(pool->queue);
fprintf(stderr, "mmal_queue_length done. num=%d\n", num);
            for (q=0; q<num; q++)
            {
                MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);

                if (!buffer)
                    vcos_log_error("Unable to get a required buffer %d from pool queue", q);

                if (mmal_port_send_buffer(camera_still_port, buffer)!= MMAL_SUCCESS)
                    vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
            }
fprintf(stderr, "mmal_port_send_buffer done\n");
            if (mmal_port_parameter_set_boolean(camera_still_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS)
            {
                vcos_log_error("%s: Failed to start capture", __func__);
            }
            else
            {
                fprintf(stderr, "mmal_port_parameter_set_boolean done\n");
                // Wait for capture to complete
                // For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
                // even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
                vcos_semaphore_wait(&callback_data.complete_semaphore);
fprintf(stderr, "vcos_semaphore_wait done\n");
            }

            // Ensure we don't die if get callback with no open file
            callback_data.file_handle = NULL;

            fclose(output_file);
            fprintf(stderr, "Closed output file\n");
            vcos_semaphore_delete(&callback_data.complete_semaphore);
fprintf(stderr, "vcos_semaphore_delete done\n");
        }
        else
        {
            mmal_status_to_int(status);
            vcos_log_error("%s: Failed to connect camera to preview", __func__);
        }

    error:
        if (camera_component) {
            mmal_component_disable(state.camera_component);
        }

        destroy_camera_component(&state);
    }

    return exit_code;
}

