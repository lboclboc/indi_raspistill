cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_raspistill CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/indi/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(RULES_INSTALL_DIR "/etc/udev/rules.d")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(${INDI_INCLUDE_DIR})


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.h ${CMAKE_CURRENT_BINARY_DIR}/config.h )

set(indi_raspistill_SRCS
   ${CMAKE_SOURCE_DIR}/src/indi_mmal.cpp
   ${CMAKE_SOURCE_DIR}/src/mmaldriver.cpp
   )

add_executable(indi_raspistill ${indi_raspistill_SRCS})

target_link_libraries(indi_raspistill ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${INDI_LIBRARIES} )

install(TARGETS indi_raspistill RUNTIME DESTINATION bin )

#install(FILES indi_qsi.xml DESTINATION ${INDI_DATA_DIR})
#install(FILES 99-qsi.rules DESTINATION ${RULES_INSTALL_DIR})
