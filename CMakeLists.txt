cmake_minimum_required(VERSION 3.0.0)

#cmake_policy(SET CMP0011 NEW)

PROJECT(indi_raspistill CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")

set(MMAL_DIR "/home/astroberry/git/userland/build")

find_package(MMAL REQUIRED)
find_package(INDI REQUIRED)

include_directories(${INDI_INCLUDE_DIR})

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(indi_raspistill_SRCS
   ${CMAKE_SOURCE_DIR}/src/mmalcamera.cpp
   ${CMAKE_SOURCE_DIR}/src/indi_mmal.cpp
   ${CMAKE_SOURCE_DIR}/src/mmaldriver.cpp
   ${CMAKE_SOURCE_DIR}/src/mmalencoder.cpp
   ${CMAKE_SOURCE_DIR}/src/mmalexception.cpp
   ${CMAKE_SOURCE_DIR}/src/mmalcomponent.cpp
   ${CMAKE_SOURCE_DIR}/src/cameracontrol.cpp
   ${CMAKE_SOURCE_DIR}/src/raw12tobayer16pipeline.cpp
   ${CMAKE_SOURCE_DIR}/src/pipeline.cpp
   ${CMAKE_SOURCE_DIR}/src/jpegpipeline.cpp
   ${CMAKE_SOURCE_DIR}/src/broadcompipeline.cpp
)
set (EGL_LIBS brcmGLESv2 brcmEGL)

add_executable(indi_raspistill ${indi_raspistill_SRCS})

target_link_libraries(indi_raspistill
    ${INDI_DRIVER_LIBRARIES}
    ${CFITSIO_LIBRARIES}
    ${INDI_LIBRARIES}
    ${MMAL_LIBRARIES}
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

#install(TARGETS indi_raspistill RUNTIME DESTINATION bin )
#install(FILES indi_raspistill.xml DESTINATION ${INDI_DATA_DIR})
