#include "RaspiStill-fixed.c"

RASPISTILL_STATE state;

/**
 *  buffer header callback function for encoder
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void my_encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    int complete = 0;

    // We pass our file handle and other stuff in via the userdata field.

    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

    if (pData)
    {
        int bytes_written = buffer->length;

        if (buffer->length && pData->file_handle)
        {
            mmal_buffer_header_mem_lock(buffer);

            bytes_written = fwrite(buffer->data, 1, buffer->length, pData->file_handle);

            mmal_buffer_header_mem_unlock(buffer);
        }

        // We need to check we wrote what we wanted - it's possible we have run out of storage.
        if (bytes_written != buffer->length)
        {
            vcos_log_error("Unable to write buffer to file - aborting");
            complete = 1;
        }

        // Now flag if we have completed
        if (buffer->flags & (MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED))
            complete = 1;
    }
    else
    {
        vcos_log_error("Received a encoder buffer callback with no state");
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled)
    {
        MMAL_STATUS_T status = MMAL_SUCCESS;
        MMAL_BUFFER_HEADER_T *new_buffer;

        new_buffer = mmal_queue_get(pData->pstate->encoder_pool->queue);

        if (new_buffer)
        {
            status = mmal_port_send_buffer(port, new_buffer);
        }
        if (!new_buffer || status != MMAL_SUCCESS)
            vcos_log_error("Unable to return a buffer to the encoder port");
    }

    if (complete)
        vcos_semaphore_post(&(pData->complete_semaphore));
}

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct. camera_component member set to the created camera_component if successful.
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
static MMAL_STATUS_T my_create_camera_component(RASPISTILL_STATE *state)
{
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *still_port = NULL;
    MMAL_STATUS_T status;

    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Failed to create camera component");
        goto error;
    }

    MMAL_PARAMETER_INT32_T camera_num = {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->common_settings.cameraNum};
    status = mmal_port_parameter_set(camera->control, &camera_num.hdr);
    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Could not select camera : error %d", status);
        goto error;
    }
    if (!camera->output_num)
    {
        status = MMAL_ENOSYS;
        vcos_log_error("Camera doesn't have output ports");
        goto error;
    }

    status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state->common_settings.sensor_mode);
    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Could not set sensor mode : error %d", status);
        goto error;
    }

    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, default_camera_control_callback);
    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
            .max_stills_w = state->common_settings.width,
            .max_stills_h = state->common_settings.height,
            .stills_yuv422 = 0,
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
    format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->common_settings.width;
    format->es->video.crop.height = state->common_settings.height;
    format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;

    status = mmal_port_format_commit(still_port);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera still format couldn't be set");
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    /* Enable component */
    status = mmal_component_enable(camera);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera component couldn't be enabled");
        goto error;
    }

    state->camera_component = camera;

    if (state->common_settings.verbose)
        fprintf(stderr, "Camera component done\n");

    return status;

error:

    if (camera)
        mmal_component_destroy(camera);

    return status;
}

int raspi_exposure(long exposure)
{
    // Our main data storage vessel..
    char filename[] = "/dev/shm/indi_raspistill_capture.jpg";

    int exit_code = EX_OK;

    MMAL_STATUS_T status = MMAL_SUCCESS;

    MMAL_PORT_T *camera_still_port = NULL;

    MMAL_PORT_T *encoder_input_port = NULL;
    MMAL_PORT_T *encoder_output_port = NULL;

    bcm_host_init();

    // Register our application with the logging system
    vcos_log_register("RaspiStill", VCOS_LOG_CATEGORY);

    signal(SIGINT, default_signal_handler);

    // Disable USR1 and USR2 for the moment - may be reenabled if go in to signal capture mode
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);

    default_status(&state);
    state.wantRAW = true;
    state.preview_parameters.wantPreview = 0;
    state.timeout = 1;
    state.common_settings.verbose = 1;
    state.camera_parameters.shutter_speed = exposure * 1000000L;

    if (state.timeout == -1)
        state.timeout = 5000;

    // Setup for sensor specific parameters
    get_sensor_defaults(state.common_settings.cameraNum, state.common_settings.camera_name,
                        &state.common_settings.width, &state.common_settings.height);

    // OK, we have a nice set of parameters. Now set up our components
    // We have three components. Camera, Preview and encoder.
    // Camera and encoder are different in stills/video, but preview
    // is the same so handed off to a separate module

    // FIXME: my_create_camera_component does not handle longer exposure than 1s
    if ((status = create_camera_component(&state)) != MMAL_SUCCESS)
    {
        vcos_log_error("%s: Failed to create camera component", __func__);
        exit_code = EX_SOFTWARE;
    }
    else if ((!state.useGL) && (status = raspipreview_create(&state.preview_parameters)) != MMAL_SUCCESS)
    {
        vcos_log_error("%s: Failed to create preview component", __func__);
        destroy_camera_component(&state);
        exit_code = EX_SOFTWARE;
    }
    else if ((status = create_encoder_component(&state)) != MMAL_SUCCESS)
    {
        vcos_log_error("%s: Failed to create encode component", __func__);
        raspipreview_destroy(&state.preview_parameters);
        destroy_camera_component(&state);
        exit_code = EX_SOFTWARE;
    }
    else
    {
        PORT_USERDATA callback_data;

        if (state.common_settings.verbose)
            fprintf(stderr, "Starting component connection stage\n");

        camera_still_port   = state.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
        encoder_input_port  = state.encoder_component->input[0];
        encoder_output_port = state.encoder_component->output[0];

        if (status == MMAL_SUCCESS)
        {
            VCOS_STATUS_T vcos_status;

            if (state.common_settings.verbose)
                fprintf(stderr, "Connecting camera stills port to encoder input port\n");

            // Now connect the camera to the encoder
            status = connect_ports(camera_still_port, encoder_input_port, &state.encoder_connection);

            if (status != MMAL_SUCCESS)
            {
                vcos_log_error("%s: Failed to connect camera video port to encoder input", __func__);
                goto error;
            }

            // Set up our userdata - this is passed though to the callback where we need the information.
            // Null until we open our filename
            callback_data.file_handle = NULL;
            callback_data.pstate = &state;
            vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "RaspiStill-sem", 0);

            vcos_assert(vcos_status == VCOS_SUCCESS);


            FILE *output_file = NULL;

            state.frameNextMethod = FRAME_NEXT_IMMEDIATELY;


            // Open the file
            output_file = fopen(filename, "wb");
            if (!output_file)
            {
                // Notify user, carry on but discarding encoded output buffers
                vcos_log_error("%s: Error opening output file: %s\nNo output file will be generated\n", __func__, filename);
            }
            callback_data.file_handle = output_file;

            int num, q;

            mmal_port_parameter_set_boolean(state.encoder_component->output[0], MMAL_PARAMETER_EXIF_DISABLE, 1);

            if (mmal_port_parameter_set_boolean(camera_still_port, MMAL_PARAMETER_ENABLE_RAW_CAPTURE, 1) != MMAL_SUCCESS)
            {
                vcos_log_error("RAW was requested, but failed to enable");
            }

            // There is a possibility that shutter needs to be set each loop.
            if (mmal_status_to_int(mmal_port_parameter_set_uint32(state.camera_component->control, MMAL_PARAMETER_SHUTTER_SPEED, state.camera_parameters.shutter_speed)) != MMAL_SUCCESS)
                vcos_log_error("Unable to set shutter speed");

            // Enable the encoder output port
            encoder_output_port->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data;

            if (state.common_settings.verbose)
                fprintf(stderr, "Enabling encoder output port\n");

            // Enable the encoder output port and tell it its callback function
            status = mmal_port_enable(encoder_output_port, my_encoder_buffer_callback);

            // Send all the buffers to the encoder output port
            num = mmal_queue_length(state.encoder_pool->queue);

            for (q=0; q<num; q++)
            {
                MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state.encoder_pool->queue);

                if (!buffer)
                    vcos_log_error("Unable to get a required buffer %d from pool queue", q);

                if (mmal_port_send_buffer(encoder_output_port, buffer)!= MMAL_SUCCESS)
                    vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
            }

            if (mmal_port_parameter_set_boolean(camera_still_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS)
            {
                vcos_log_error("%s: Failed to start capture", __func__);
            }
            else
            {
                // Wait for capture to complete
                // For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
                // even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
                vcos_semaphore_wait(&callback_data.complete_semaphore);
            }

            // Ensure we don't die if get callback with no open file
            callback_data.file_handle = NULL;

            // Disable encoder output port
            status = mmal_port_disable(encoder_output_port);

            fclose(output_file);
            fprintf(stderr, "Closed output file\n");
            vcos_semaphore_delete(&callback_data.complete_semaphore);

        }
        else
        {
            mmal_status_to_int(status);
            vcos_log_error("%s: Failed to connect camera to preview", __func__);
        }

error:
        mmal_status_to_int(status);

        // Disable all our ports that are not handled by connections
        check_disable_port(encoder_output_port);

        if (state.encoder_connection)
            mmal_connection_destroy(state.encoder_connection);

        /* Disable components */
        if (state.encoder_component)
            mmal_component_disable(state.encoder_component);

        if (state.camera_component)
            mmal_component_disable(state.camera_component);

        destroy_encoder_component(&state);
        destroy_camera_component(&state);

    }

    if (status != MMAL_SUCCESS)
        raspicamcontrol_check_configuration(128);

    return exit_code;
}

