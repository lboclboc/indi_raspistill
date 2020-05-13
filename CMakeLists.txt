cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_raspistill CXX C)

include(ExternalProject)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(RULES_INSTALL_DIR "/etc/udev/rules.d")
set(USERLAND_SRC "${PROJECT_SOURCE_DIR}/build/userland-prefix/src/userland")
set(USERLAND_LIB "${PROJECT_SOURCE_DIR}/build/userland-prefix/src/userland/build/lib")
set(RASPISTILL_OBJS "${PROJECT_SOURCE_DIR}/build/userland-prefix/src/userland-build/host_applications/linux/apps/raspicam/CMakeFiles/raspistill.dir")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads)

include_directories(${INDI_INCLUDE_DIR})
include_directories(${USERLAND_SRC})
include_directories(${USERLAND_SRC}/interface/khronos/include)
include_directories(${USERLAND_SRC}/host_applications/linux/apps/raspicam)
include_directories(${USERLAND_SRC}/host_applications/linux/libs/bcm_host/include)


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h ${CMAKE_CURRENT_BINARY_DIR}/config.h )

add_library(raspilib OBJECT IMPORTED GLOBAL)
set_property(TARGET raspilib PROPERTY IMPORTED_OBJECTS
    ${RASPISTILL_OBJS}/libgps_loader.c.o
    ${RASPISTILL_OBJS}/RaspiCamControl.c.o
    ${RASPISTILL_OBJS}/RaspiCLI.c.o
    ${RASPISTILL_OBJS}/RaspiCommonSettings.c.o
    ${RASPISTILL_OBJS}/RaspiGPS.c.o
    ${RASPISTILL_OBJS}/RaspiHelpers.c.o
    ${RASPISTILL_OBJS}/RaspiPreview.c.o
    ${RASPISTILL_OBJS}/RaspiTex.c.o
    ${RASPISTILL_OBJS}/RaspiTexUtil.c.o
    ${RASPISTILL_OBJS}/tga.c.o
    ${RASPISTILL_OBJS}/gl_scenes/teapot.c.o
    ${RASPISTILL_OBJS}/gl_scenes/mirror.c.o
    ${RASPISTILL_OBJS}/gl_scenes/models.c.o
    ${RASPISTILL_OBJS}/gl_scenes/square.c.o
    ${RASPISTILL_OBJS}/gl_scenes/vcsm_square.c.o
    ${RASPISTILL_OBJS}/gl_scenes/yuv.c.o
    ${RASPISTILL_OBJS}/gl_scenes/sobel.c.o
)

add_library(libuserland OBJECT IMPORTED GLOBAL)
set_property(TARGET libuserland PROPERTY IMPORTED_OBJECTS 
    ${USERLAND_LIB}/libvcos.so
    ${USERLAND_LIB}/libbcm_host.so
    ${USERLAND_LIB}/libmmal_core.so
    ${USERLAND_LIB}/libmmal_util.so
    ${USERLAND_LIB}/libbrcmGLESv2.so
    ${USERLAND_LIB}/libbrcmEGL.so
    ${USERLAND_LIB}/libvcsm.so
)

set(indi_raspistill_SRCS
   ${CMAKE_SOURCE_DIR}/src/RaspiStill.c
   ${CMAKE_SOURCE_DIR}/src/indi_mmal.cpp
   ${CMAKE_SOURCE_DIR}/src/mmaldriver.cpp
)

set(USERLAND_LIBRARIES raspilib libuserland)

ExternalProject_Add(userland
  GIT_REPOSITORY    https://github.com/raspberrypi/userland.git
  GIT_TAG           291f9cb826d51ac30c1114cdc165836eacd8db52
  BUILD_COMMAND     ${CMAKE_COMMAND} --build <BINARY_DIR> --config --target raspistill
  PATCH_COMMAND     ${PROJECT_SOURCE_DIR}/scripts/patch_raspistill.sh ${USERLAND_SRC}/host_applications/linux/apps/raspicam/RaspiStill.c
                        ${USERLAND_SRC}/host_applications/linux/apps/raspicam/RaspiStill-fixed.c &&
                    ${CMAKE_COMMAND} -E echo "Patched RaspiStill.c"
  INSTALL_COMMAND ""
)

get_target_property(value userland SOURCE_DIR)
message("userland value:" ${value})
get_target_property(value userland BINARY_DIR)
message("userland value:" ${value})


add_executable(indi_raspistill ${indi_raspistill_SRCS})

target_link_libraries(indi_raspistill ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${INDI_LIBRARIES} ${USERLAND_LIBRARIES} Threads::Threads ${CMAKE_DL_LIBS})

add_dependencies(indi_raspistill userland)

#install(TARGETS indi_raspistill RUNTIME DESTINATION bin )
#install(FILES indi_qsi.xml DESTINATION ${INDI_DATA_DIR})
#install(FILES 99-qsi.rules DESTINATION ${RULES_INSTALL_DIR})

