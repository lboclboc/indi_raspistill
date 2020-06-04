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
        virtual ~MMALException();
        virtual const char* what() const throw() override;
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

private:
    void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    void create_camera_component();
    int exposure(long exposure, int iso);

    MMAL_POOL_T *pool {};
    FILE *file_handle {};
    VCOS_SEMAPHORE_T complete_semaphore {};
    MMAL_COMPONENT_T *camera  {};
    int cameraNum {};

    friend void c_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
};

#endif // _MMAL_CAMERA_H
