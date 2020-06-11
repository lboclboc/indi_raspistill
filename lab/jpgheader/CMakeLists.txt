cmake_minimum_required(VERSION 3.10)

PROJECT(jpgheader)

include(FindPkgConfig QUIET)

pkg_check_modules(LIBJPEG REQUIRED libjpeg)

add_executable(jpgheader jpgheader.c)
target_link_libraries(jpgheader ${LIBJPEG_LDFLAGS})
