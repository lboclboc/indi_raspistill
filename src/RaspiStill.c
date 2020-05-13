#include "RaspiStill.h"

/**
 * main
 */
int main(int argc, const char **argv)
{
   // Our main data storage vessel..
   RASPISTILL_STATE state;
   int exit_code = EX_OK;

   printf("FIXME: Hey this is my main\n");

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

   if (state.useGL)
      raspitex_init(&state.raspitex_state);

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

         /* If GL preview is requested then start the GL threads */
         if (state.useGL && (raspitex_start(&state.raspitex_state) != 0))
            goto error;

         if (status != MMAL_SUCCESS)
         {
            vcos_log_error("Failed to setup encoder output");
            goto error;
         }

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

               // We only capture if a filename was specified and it opened
               if (state.useGL && state.glCapture && output_file)
               {
                  /* Save the next GL framebuffer as the next camera still */
                  int rc = raspitex_capture(&state.raspitex_state, output_file);
                  if (rc != 0)
                     vcos_log_error("Failed to capture GL preview");
                  rename_file(&state, output_file, final_filename, use_filename, frame);
               }
               else if (output_file)
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

      if (state.useGL)
      {
         raspitex_stop(&state.raspitex_state);
         raspitex_destroy(&state.raspitex_state);
      }

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
