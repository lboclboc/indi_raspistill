cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_raspistill CXX C)

include(ExternalProject)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(RULES_INSTALL_DIR "/etc/udev/rules.d")
set(RASPISTILL_DIR "${PROJECT_SOURCE_DIR}/userland/host_applications/linux/apps/raspicam")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads)

include_directories(${INDI_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/userland)
include_directories(${PROJECT_SOURCE_DIR}/userland/interface/khronos/include)
include_directories(${PROJECT_SOURCE_DIR}/userland/host_applications/linux/libs/bcm_host/include)


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h ${CMAKE_CURRENT_BINARY_DIR}/config.h )

set(indi_raspistill_SRCS
   ${CMAKE_SOURCE_DIR}/src/indi_mmal.cpp
   ${CMAKE_SOURCE_DIR}/src/mmaldriver.cpp
   ${RASPISTILL_DIR}/RaspiStill.c
   )

ExternalProject_Add(userland
  GIT_REPOSITORY    https://github.com/raspberrypi/userland.git
)

add_executable(indi_raspistill ${indi_raspistill_SRCS})

target_link_libraries(indi_raspistill ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${INDI_LIBRARIES} Threads::Threads )

install(TARGETS indi_raspistill RUNTIME DESTINATION bin )

#install(FILES indi_qsi.xml DESTINATION ${INDI_DATA_DIR})
#install(FILES 99-qsi.rules DESTINATION ${RULES_INSTALL_DIR})


