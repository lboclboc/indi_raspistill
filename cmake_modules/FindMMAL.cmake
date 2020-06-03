cmake_minimum_required(VERSION 3.0.0)

include_directories(/opt/vc/include)
include_directories(/opt/vc/include/interface/mmal)
include_directories(/opt/vc/include/interface/mmal/util)
include_directories(/opt/vc/include/interface/vcos/pthreads)
include_directories(/opt/vc/include/interface/vmcs_host)
include_directories(/opt/vc/include/interface/vmcs_host/linux)

link_directories(/opt/vc/lib)

foreach(lib mmal_core bcm_host mmal_util mmal_vc_client brcmGLESv2 brcmEGL vcsm)
    set(libpath)
    find_library(libpath
                 NAMES ${lib}
                 HINTS /opt/vc/lib
    )
    if (DEFINED libpath)
        set(MMAL_LIBRARIES ${MMAL_LIBRARIES} ${libpath})
    endif()
endforeach(lib)

if(NOT DEFINED MMAL_LIBRARIES)
    message(FATAL_ERROR "Failed to find libmmal_core library")
endif()
