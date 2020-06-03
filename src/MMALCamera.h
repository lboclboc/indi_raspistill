#ifndef _MMAL_CAMERA_H
#define _MMAL_CAMERA_H

#include <mmal.h>


class MMALCamera
{
public:
    MMALCamera();
    virtual ~MMALCamera();
    static const int MMAL_CAMERA_PREVIEW_PORT {0};
    static const int MMAL_CAMERA_VIDEO_PORT {1};
    static const int MMAL_CAMERA_CAPTURE_PORT {2};

private:
    void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
    void create_camera_component();
    int exposure(long exposure, int iso);

    MMAL_POOL_T *pool {nullptr};
    FILE *file_handle {nullptr};
    VCOS_SEMAPHORE_T complete_semaphore;
    MMAL_COMPONENT_T *camera  {nullptr};

    friend void c_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
};

#endif // _MMAL_CAMERA_H
