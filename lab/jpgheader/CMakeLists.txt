CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

PROJECT(jpegheader)

if(0)
SET(LIBJPEG_SOURCE_DIR "/home/labe/Private/git/build-libjpeg-turbo-Desktop-${CMAKE_BUILD_TYPE}")
SET(LIBJPEG_BUILD_DIR "/home/labe/Private/git/build-libjpeg-turbo-Desktop-${CMAKE_BUILD_TYPE}")

INCLUDE(FindPkgConfig QUIET)

PKG_CHECK_MODULES(LIBJPEG REQUIRED IMPORTED_TARGET libjpeg)

INCLUDE_DIRECTORIES(${LIBJPEG_SOURCE_DIR})
ADD_EXECUTABLE(jpgheader jpgheader.c)


MESSAGE(STATUS "Checking for libjpeg(${JPEG_LIB}) in ${LIBJPEG_BUILD_DIR} and ${LIBJPEG_LIBRARY_DIRS}")
FIND_LIBRARY(JPEG_LIB
    NAMES libturbojpeg.a libjpeg.a
    HINTS ${LIBJPEG_BUILD_DIR} ${LIBJPEG_LIBRARY_DIRS}
)

MESSAGE(STATUS "Using libjpeg as ${JPEG_LIB}")
TARGET_LINK_LIBRARIES(jpgheader ${JPEG_LIB})

else()

    ADD_EXECUTABLE(jpegheader jpegheader.cpp jpegpipeline.cpp broadcompipeline.cpp pipeline.cpp raw12tobayer16pipeline.cpp)

endif()
