cmake_minimum_required(VERSION 3.0.0)

#cmake_policy(SET CMP0011 NEW)

PROJECT(indi_raspistill CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")

find_package(MMAL REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(indi_raspistill_SRCS
   ${CMAKE_SOURCE_DIR}/src/MMALCamera.cpp
)

add_executable(indi_raspistill ${indi_raspistill_SRCS})

target_link_libraries(indi_raspistill ${MMAL_LIBRARIES} Threads::Threads ${CMAKE_DL_LIBS})
