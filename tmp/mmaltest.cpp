#include <stdio.h>
#include <sysexits.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_logging.h>
#include <interface/mmal/mmal_buffer.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_util_params.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_connection.h>
#include <interface/mmal/mmal_parameters_camera.h>
#include <bcm_host.h>


#define MAX_USER_EXIF_TAGS      32

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

#define VIDEO_OUTPUT_BUFFERS_NUM 3
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1


#include "mmaltest.h"

void check_mmal(int status)
{
   if (status != MMAL_SUCCESS)
   {
      fprintf(stderr, "Could not set sensor mode : error %d", status);
      exit(1);
   }
}
/**
 *  buffer header callback function for encoder
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
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
 * Destroy the encoder component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_encoder_component(RASPISTILL_STATE *state)
{
   // Get rid of any port buffers first
   if (state->encoder_pool)
   {
      mmal_port_pool_destroy(state->encoder_component->output[0], state->encoder_pool);
   }

   if (state->encoder_component)
   {
      mmal_component_destroy(state->encoder_component);
      state->encoder_component = NULL;
   }
}


/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component(RASPISTILL_STATE *state)
{
   if (state->camera_component)
   {
      mmal_component_destroy(state->camera_component);
      state->camera_component = NULL;
   }
}

bool get_sensor_defaults(int camera_num, char *camera_name, int *width, int *height )
{
   MMAL_COMPONENT_T *camera_info;
   MMAL_STATUS_T status;

   // Try to get the camera name and maximum supported resolution
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);
   if (status == MMAL_SUCCESS)
   {
      MMAL_PARAMETER_CAMERA_INFO_T param;
      param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
      param.hdr.size = sizeof(param)-4;  // Deliberately undersize to check firmware version
      status = mmal_port_parameter_get(camera_info->control, &param.hdr);

      if (status != MMAL_SUCCESS)
      {
         // Running on newer firmware
         param.hdr.size = sizeof(param);
         status = mmal_port_parameter_get(camera_info->control, &param.hdr);
         if (status == MMAL_SUCCESS && (int)param.num_cameras > camera_num)
         {
            // Take the parameters from the first camera listed.
            *width = param.cameras[camera_num].max_width;
            *height = param.cameras[camera_num].max_height;
            strncpy(camera_name, param.cameras[camera_num].camera_name, MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
            camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN-1] = 0;
         }
         else
            fprintf(stderr, "Cannot read camera info, keeping the defaults for OV5647");
      }
      else
      {
          fprintf(stderr, "Too old firmare");
          return false;
      }

      mmal_component_destroy(camera_info);

      return true;
   }
   else
   {
      fprintf(stderr, "Failed to create camera_info component");
      return false;
   }
}

static MMAL_STATUS_T create_camera_component(RASPISTILL_STATE *state)
{
   MMAL_COMPONENT_T *camera = 0;
   MMAL_ES_FORMAT_T *format;
   MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
   MMAL_STATUS_T status;

   /* Create the component */
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Failed to create camera component");
      goto error;
   }

#if 0
   status = raspicamcontrol_set_stereo_mode(camera->output[0], &state->camera_parameters.stereo_mode) ? MMAL_EFAULT : MMAL_SUCCESS;
   check_mmal(status);
   status = raspicamcontrol_set_stereo_mode(camera->output[1], &state->camera_parameters.stereo_mode) ? MMAL_EFAULT : MMAL_SUCCESS;
   check_mmal(status);
   status = raspicamcontrol_set_stereo_mode(camera->output[2], &state->camera_parameters.stereo_mode) ? MMAL_EFAULT : MMAL_SUCCESS;
   check_mmal(status);
#endif

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not set stereo mode : error %d", status);
      goto error;
   }

   MMAL_PARAMETER_INT32_T camera_num =
   {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->common_settings.cameraNum};

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

   preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
   video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
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
      MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
      {
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

   format = preview_port->format;
   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;

   if(state->camera_parameters.shutter_speed > 6000000)
   {
      MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
         { 5, 1000 }, {166, 1000}
      };
      mmal_port_parameter_set(preview_port, &fps_range.hdr);
   }
   else if(state->camera_parameters.shutter_speed > 1000000)
   {
      MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
         { 166, 1000 }, {999, 1000}
      };
      mmal_port_parameter_set(preview_port, &fps_range.hdr);
   }
   if (state->fullResPreview)
   {
      // In this mode we are forcing the preview to be generated from the full capture resolution.
      // This runs at a max of 15fps with the OV5647 sensor.
      format->es->video.width = VCOS_ALIGN_UP(state->common_settings.width, 32);
      format->es->video.height = VCOS_ALIGN_UP(state->common_settings.height, 16);
      format->es->video.crop.x = 0;
      format->es->video.crop.y = 0;
      format->es->video.crop.width = state->common_settings.width;
      format->es->video.crop.height = state->common_settings.height;
      format->es->video.frame_rate.num = FULL_RES_PREVIEW_FRAME_RATE_NUM;
      format->es->video.frame_rate.den = FULL_RES_PREVIEW_FRAME_RATE_DEN;
   }
   else
   {
      // Use a full FOV 4:3 mode
      format->es->video.width = VCOS_ALIGN_UP(state->preview_parameters.previewWindow.width, 32);
      format->es->video.height = VCOS_ALIGN_UP(state->preview_parameters.previewWindow.height, 16);
      format->es->video.crop.x = 0;
      format->es->video.crop.y = 0;
      format->es->video.crop.width = state->preview_parameters.previewWindow.width;
      format->es->video.crop.height = state->preview_parameters.previewWindow.height;
      format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
      format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;
   }

   status = mmal_port_format_commit(preview_port);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera viewfinder format couldn't be set");
      goto error;
   }

   // Set the same format on the video  port (which we don't use here)
   mmal_format_full_copy(video_port->format, format);
   status = mmal_port_format_commit(video_port);

   if (status  != MMAL_SUCCESS)
   {
      vcos_log_error("camera video format couldn't be set");
      goto error;
   }

   // Ensure there are enough buffers to avoid dropping frames
   if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

   format = still_port->format;

   if(state->camera_parameters.shutter_speed > 6000000)
   {
      MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
         { 50, 1000 }, {166, 1000}
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

int main(int argc, char **argv)
{
   // Our main data storage vessel..
   RASPISTILL_STATE state;
   int exit_code = EX_OK;

   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_PORT_T *camera_preview_port = NULL;
   MMAL_PORT_T *camera_video_port = NULL;
   MMAL_PORT_T *camera_still_port = NULL;
   MMAL_PORT_T *preview_input_port = NULL;
   MMAL_PORT_T *encoder_input_port = NULL;
   MMAL_PORT_T *encoder_output_port = NULL;

   bcm_host_init();

   // Register our application with the logging system
   vcos_log_register("RaspiStill", VCOS_LOG_CATEGORY);

   signal(SIGINT, default_signal_handler);

   // Disable USR1 and USR2 for the moment - may be reenabled if go in to signal capture mode
   signal(SIGUSR1, SIG_IGN);
   signal(SIGUSR2, SIG_IGN);

   set_app_name(argv[0]);

   // Do we have any parameters
   if (argc == 1)
   {
      display_valid_parameters(basename(argv[0]), &application_help_message);
      exit(EX_USAGE);
   }

   default_status(&state);

   // Parse the command line and put options in to our status structure
   if (parse_cmdline(argc, argv, &state))
   {
      exit(EX_USAGE);
   }

   if (state.timeout == -1)
      state.timeout = 5000;

   // Setup for sensor specific parameters
   get_sensor_defaults(state.common_settings.cameraNum, state.common_settings.camera_name,
                       &state.common_settings.width, &state.common_settings.height);

   if (state.common_settings.verbose)
   {
      print_app_details(stderr);
      dump_status(&state);
   }

   if (state.common_settings.gps)
   {
      if (raspi_gps_setup(state.common_settings.verbose))
         state.common_settings.gps = false;
   }


   // OK, we have a nice set of parameters. Now set up our components
   // We have three components. Camera, Preview and encoder.
   // Camera and encoder are different in stills/video, but preview
   // is the same so handed off to a separate module

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

      camera_preview_port = state.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
      camera_video_port   = state.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
      camera_still_port   = state.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
      encoder_input_port  = state.encoder_component->input[0];
      encoder_output_port = state.encoder_component->output[0];

      if (! state.useGL)
      {
         if (state.common_settings.verbose)
            fprintf(stderr, "Connecting camera preview port to video render.\n");

         // Note we are lucky that the preview and null sink components use the same input port
         // so we can simple do this without conditionals
         preview_input_port  = state.preview_parameters.preview_component->input[0];

         // Connect camera to preview (which might be a null_sink if no preview required)
         status = connect_ports(camera_preview_port, preview_input_port, &state.preview_connection);
      }

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

         if (state.demoMode)
         {
            // Run for the user specific time..
            int num_iterations = state.timeout / state.demoInterval;
            int i;
            for (i=0; i<num_iterations; i++)
            {
               raspicamcontrol_cycle_test(state.camera_component);
               vcos_sleep(state.demoInterval);
            }
         }
         else
         {
            int frame, keep_looping = 1;
            FILE *output_file = NULL;
            char *use_filename = NULL;      // Temporary filename while image being written
            char *final_filename = NULL;    // Name that file gets once writing complete

            frame = state.frameStart - 1;

            while (keep_looping)
            {
               keep_looping = wait_for_next_frame(&state, &frame);

               if (state.datetime)
               {
                  time_t rawtime;
                  struct tm *timeinfo;

                  time(&rawtime);
                  timeinfo = localtime(&rawtime);

                  frame = timeinfo->tm_mon+1;
                  frame *= 100;
                  frame += timeinfo->tm_mday;
                  frame *= 100;
                  frame += timeinfo->tm_hour;
                  frame *= 100;
                  frame += timeinfo->tm_min;
                  frame *= 100;
                  frame += timeinfo->tm_sec;
               }
               if (state.timestamp)
               {
                  frame = (int)time(NULL);
               }

               // Open the file
               if (state.common_settings.filename)
               {
                  if (state.common_settings.filename[0] == '-')
                  {
                     output_file = stdout;
                  }
                  else
                  {
                     vcos_assert(use_filename == NULL && final_filename == NULL);
                     status = create_filenames(&final_filename, &use_filename, state.common_settings.filename, frame);
                     if (status  != MMAL_SUCCESS)
                     {
                        vcos_log_error("Unable to create filenames");
                        goto error;
                     }

                     if (state.common_settings.verbose)
                        fprintf(stderr, "Opening output file %s\n", final_filename);
                     // Technically it is opening the temp~ filename which will be renamed to the final filename

                     output_file = fopen(use_filename, "wb");

                     if (!output_file)
                     {
                        // Notify user, carry on but discarding encoded output buffers
                        vcos_log_error("%s: Error opening output file: %s\nNo output file will be generated\n", __func__, use_filename);
                     }
                  }

                  callback_data.file_handle = output_file;
               }

               if (output_file)
               {
                  int num, q;

                  // Must do this before the encoder output port is enabled since
                  // once enabled no further exif data is accepted
                  if ( state.enableExifTags )
                  {
                     struct gps_data_t *gps_data = raspi_gps_lock();
                     add_exif_tags(&state, gps_data);
                     raspi_gps_unlock();
                  }
                  else
                  {
                     mmal_port_parameter_set_boolean(
                        state.encoder_component->output[0], MMAL_PARAMETER_EXIF_DISABLE, 1);
                  }

                  // Same with raw, apparently need to set it for each capture, whilst port
                  // is not enabled
                  if (state.wantRAW)
                  {
                     if (mmal_port_parameter_set_boolean(camera_still_port, MMAL_PARAMETER_ENABLE_RAW_CAPTURE, 1) != MMAL_SUCCESS)
                     {
                        vcos_log_error("RAW was requested, but failed to enable");
                     }
                  }

                  // There is a possibility that shutter needs to be set each loop.
                  if (mmal_status_to_int(mmal_port_parameter_set_uint32(state.camera_component->control, MMAL_PARAMETER_SHUTTER_SPEED, state.camera_parameters.shutter_speed)) != MMAL_SUCCESS)
                     vcos_log_error("Unable to set shutter speed");

                  // Enable the encoder output port
                  encoder_output_port->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data;

                  if (state.common_settings.verbose)
                     fprintf(stderr, "Enabling encoder output port\n");

                  // Enable the encoder output port and tell it its callback function
                  status = mmal_port_enable(encoder_output_port, encoder_buffer_callback);

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

                  if (state.burstCaptureMode)
                  {
                     mmal_port_parameter_set_boolean(state.camera_component->control,  MMAL_PARAMETER_CAMERA_BURST_CAPTURE, 1);
                  }

                  if(state.camera_parameters.enable_annotate)
                  {
                     if ((state.camera_parameters.enable_annotate & ANNOTATE_APP_TEXT) && state.common_settings.gps)
                     {
                        char *text = raspi_gps_location_string();
                        raspicamcontrol_set_annotate(state.camera_component, state.camera_parameters.enable_annotate,
                                                     text,
                                                     state.camera_parameters.annotate_text_size,
                                                     state.camera_parameters.annotate_text_colour,
                                                     state.camera_parameters.annotate_bg_colour,
                                                     state.camera_parameters.annotate_justify,
                                                     state.camera_parameters.annotate_x,
                                                     state.camera_parameters.annotate_y
                                                    );
                        free(text);
                     }
                     else
                        raspicamcontrol_set_annotate(state.camera_component, state.camera_parameters.enable_annotate,
                                                     state.camera_parameters.annotate_string,
                                                     state.camera_parameters.annotate_text_size,
                                                     state.camera_parameters.annotate_text_colour,
                                                     state.camera_parameters.annotate_bg_colour,
                                                     state.camera_parameters.annotate_justify,
                                                     state.camera_parameters.annotate_x,
                                                     state.camera_parameters.annotate_y
                                                    );
                  }

                  if (state.common_settings.verbose)
                     fprintf(stderr, "Starting capture %d\n", frame);

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
                     if (state.common_settings.verbose)
                        fprintf(stderr, "Finished capture %d\n", frame);
                  }

                  // Ensure we don't die if get callback with no open file
                  callback_data.file_handle = NULL;

                  if (output_file != stdout)
                  {
                     rename_file(&state, output_file, final_filename, use_filename, frame);
                  }
                  else
                  {
                     fflush(output_file);
                  }
                  // Disable encoder output port
                  status = mmal_port_disable(encoder_output_port);
               }

               if (use_filename)
               {
                  free(use_filename);
                  use_filename = NULL;
               }
               if (final_filename)
               {
                  free(final_filename);
                  final_filename = NULL;
               }
            } // end for (frame)

            vcos_semaphore_delete(&callback_data.complete_semaphore);
         }
      }
      else
      {
         mmal_status_to_int(status);
         vcos_log_error("%s: Failed to connect camera to preview", __func__);
      }

error:

      mmal_status_to_int(status);

      if (state.common_settings.verbose)
         fprintf(stderr, "Closing down\n");

      // Disable all our ports that are not handled by connections
      check_disable_port(camera_video_port);
      check_disable_port(encoder_output_port);

      if (state.preview_connection)
         mmal_connection_destroy(state.preview_connection);

      if (state.encoder_connection)
         mmal_connection_destroy(state.encoder_connection);

      /* Disable components */
      if (state.encoder_component)
         mmal_component_disable(state.encoder_component);

      if (state.preview_parameters.preview_component)
         mmal_component_disable(state.preview_parameters.preview_component);

      if (state.camera_component)
         mmal_component_disable(state.camera_component);

      destroy_encoder_component(&state);
      raspipreview_destroy(&state.preview_parameters);
      destroy_camera_component(&state);

      if (state.common_settings.verbose)
         fprintf(stderr, "Close down completed, all components disconnected, disabled and destroyed\n\n");

      if (state.common_settings.gps)
         raspi_gps_shutdown(state.common_settings.verbose);
   }

   if (status != MMAL_SUCCESS)
      raspicamcontrol_check_configuration(128);

   return exit_code;
}

int xmain(int argc, char **argv)
{
   // Our main data storage vessel..
   RASPISTILL_STATE state;
   int exit_code = EX_OK;

   MMAL_STATUS_T status = MMAL_SUCCESS;

#if 0
   MMAL_PORT_T *camera_preview_port = NULL;
   MMAL_PORT_T *camera_video_port = NULL;
   MMAL_PORT_T *camera_still_port = NULL;
   MMAL_PORT_T *preview_input_port = NULL;
   MMAL_PORT_T *encoder_input_port = NULL;
   MMAL_PORT_T *encoder_output_port = NULL;
#endif

   bcm_host_init();

   if ((status = create_camera_component(&state)) != MMAL_SUCCESS)
   {
      vcos_log_error("%s: Failed to create camera component", __func__);
      exit_code = EX_SOFTWARE;
   }
}
