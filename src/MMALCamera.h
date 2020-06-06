#ifndef _MMAL_CAMERA_H
#define _MMAL_CAMERA_H


class MMALCamera;
#include <stdexcept>
#include <mmal.h>

class MMALCamera
{
public:
    class MMALException : public std::runtime_error
    {
    public:
        MMALException(const char *text);
        static void throw_if(bool status, const char *text);
    };

    static const int PREVIEW_PORT {0};
    static const int VIDEO_PORT {1};
    static const int CAPTURE_PORT {2};

    MMALCamera(int cameraNum = 0);
    virtual ~MMALCamera();
    static const int MMAL_CAMERA_PREVIEW_PORT {0};
    static const int MMAL_CAMERA_VIDEO_PORT {1};
    static const int MMAL_CAMERA_CAPTURE_PORT {2};

    int set_shutter_speed_us(long shutter_speed)  { this->shutter_speed = shutter_speed; return 0; }
    int set_iso(int iso) { this->iso = iso; return 0; }
    int set_gain(uint32_t gain) { this->gain = gain; return 0; }
    int capture();

private:
    void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    void create_camera_component();
    void setup_capture_port();
    void get_sensor_defaults();
    void set_camera_parameters();

    MMAL_POOL_T *pool {};
    FILE *file_handle {};
    VCOS_SEMAPHORE_T complete_semaphore {};
    MMAL_COMPONENT_T *camera  {};
    int32_t cameraNum {};
    char cameraName[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN] {};
    uint32_t shutter_speed {100000};
    unsigned int iso {100};
    uint32_t gain {1};
    uint32_t width {};
    uint32_t height {};

    friend void c_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
};

#endif // _MMAL_CAMERA_H
