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

    int capture(long shutter_speed, int iso);

private:
    void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    void create_camera_component();
    void set_camera_parameters();
    void set_rational(uint32_t param, int32_t num, int32_t den);
    void setup_capture_port();
    void setup_preview_port();
//    MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection);

    MMAL_POOL_T *pool {};
    FILE *file_handle {};
    VCOS_SEMAPHORE_T complete_semaphore {};
    MMAL_COMPONENT_T *camera  {};
    int cameraNum {};
    long shutter_speed;

    friend void c_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
};

#endif // _MMAL_CAMERA_H
