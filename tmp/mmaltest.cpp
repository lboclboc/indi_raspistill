#include <stdio.h>
#include <interface/mmal/mmal.h>
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_parameters_camera.h"

#define MMAL_CAMERA_CAPTURE_PORT 2

/** Default camera callback function
 * Handles the --settings
 * @param port
 * @param Callback data
 */
void default_camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   fprintf(stderr, "Camera control callback  cmd=0x%08x", buffer->cmd);

   if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED)
   {
      MMAL_EVENT_PARAMETER_CHANGED_T *param = (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
      switch (param->hdr.id)
      {
      case MMAL_PARAMETER_CAMERA_SETTINGS:
         MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T*)param;
         fprintf(stderr, "Exposure now %u, analog gain %u/%u, digital gain %u/%u",
                        settings->exposure,
                        settings->analog_gain.num, settings->analog_gain.den,
                        settings->digital_gain.num, settings->digital_gain.den);
         fprintf(stderr, "AWB R=%u/%u, B=%u/%u",
                        settings->awb_red_gain.num, settings->awb_red_gain.den,
                        settings->awb_blue_gain.num, settings->awb_blue_gain.den);
         break;
      }
   }
   else if (buffer->cmd == MMAL_EVENT_ERROR)
   {
      fprintf(stderr, "No data received from sensor. Check all connections, including the Sunny one on the camera board");
   }
   else
   {
      fprintf(stderr, "Received unexpected camera control callback event, 0x%08x", buffer->cmd);
   }

   mmal_buffer_header_release(buffer);
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
         if (status == MMAL_SUCCESS && param.num_cameras > camera_num)
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

int main(int argc, char **argv)
{
    MMAL_COMPONENT_T *camera = 0;
    MMAL_STATUS_T status;
    char camera_name[1024];
    int width, height;
    
    get_sensor_defaults(0, camera_name, &width, &height);
    printf("Camera %s: %dx%d\n", camera_name, width, height);

    /* Create the camera component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

    if (status != MMAL_SUCCESS)
    {
        fprintf(stderr, "Failed to open camera");
        exit(1);
    }

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, default_camera_control_callback);

    if (status != MMAL_SUCCESS)
    {
        fprintf(stderr, "Unable to enable control port : error %d", status);
        exit(1);
    }

    MMAL_PARAMETER_INT32_T camera_num = {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, 0};

    status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

    if (status != MMAL_SUCCESS)
    {
        fprintf(stderr, "Could not select camera : error %d", status);
        exit(1);
    }

    printf("Opened camera ok\n");

    return 0;
}

// vim: sw=4 ai et
