# Author: Tapparel Joachim@TCL,EPFL

cmake_minimum_required(VERSION 3.8)

add_compile_definitions(PRINT_DEBUG=false)

option(RASPI "Build for raspi RRH" OFF)

project(lora_cran)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -lzmq -pthread -g")# -O3

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to be used")

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

if(RASPI)
    message(STATUS "----------- Build for Raspi -----------")
    set(GNURADIO_PREFIX /usr/local/) 

    include_directories(
        /usr/lib/aarch64-linux-gnu/
    )
    add_subdirectory(raspi_single_channel_rrh)
    set(EXTRA_LIBRARIES
        /usr/lib/aarch64-linux-gnu/libzmq.so
        /usr/lib/aarch64-linux-gnu/libserial.so
    )
    set(EXEC_FILES
        raspi_single_channel_rrh.cxx
    )
else(RASPI)
    message(STATUS "----------- Build for Server -----------")
    set(GNURADIO_PREFIX $ENV{CONDA_PREFIX})

    add_subdirectory(application)
    add_subdirectory(bbu)
    set(EXEC_FILES
        application.cxx
        bbu.cxx
    )  
endif(RASPI)

include_directories(
    ${GNURADIO_ALL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    $ENV{HOME}/.grc_gnuradio
    ${GNURADIO_PREFIX}/include/gnuradio/lora_sdr
    ${GNURADIO_PREFIX}/include/gnuradio/cran
    ../${GR_CRAN_DIR}/include/gnuradio/cran
)

# find_package(Boost 1.40 COMPONENTS program_options REQUIRED )
find_package(spdlog)
message(STATUS "Gnuradio install prefix: ${GNURADIO_PREFIX}")
 
find_package(Gnuradio "3.10" COMPONENTS
    blocks
    fec
    fft
    filter
    analog
    digital
    dtv
    audio
    channels
    pdu
    iio
    qtgui
    trellis
    uhd
    uhd UHD 4.0 RFNoC
    video-sdl
    vocoder
    wavelet
    zeromq
)

set(GR_LIBRARIES
    boost_system
    gnuradio::gnuradio-blocks
    gnuradio::gnuradio-zeromq
    gnuradio::gnuradio-runtime
    gnuradio::gnuradio-pmt
    gnuradio::gnuradio-uhd
    gnuradio::gnuradio-channels
    ${GNURADIO_PREFIX}/lib/libuhd.so
    ${GNURADIO_PREFIX}/lib/libgnuradio-uhd.so
    ${GNURADIO_PREFIX}/lib/libgnuradio-lora_sdr.so
    ${GNURADIO_PREFIX}/lib/libgnuradio-cran.so
)   

foreach(exec_file ${EXEC_FILES})
    string(REPLACE ".cxx" "" exec_name ${exec_file})
    string(REPLACE ".cxx" "_lib" exec_lib ${exec_file})
    string(JOIN "/" exec_path ${exec_name} ${exec_file})
    message(STATUS "Building " ${exec_name})
    
    add_executable(${exec_name} ${exec_path})
    target_link_libraries(${exec_name} ${GR_LIBRARIES} ${EXTRA_LIBRARIES} ${Boost_LIBRARIES} ${exec_lib})

endforeach()

################################################################
# Doxygen
################################################################

find_package(Doxygen)
if (DOXYGEN_FOUND)
  doxygen_add_docs(
    doxygen
    ${PROJECT_SOURCE_DIR}
    COMMENT "Generate html pages"
    )
  add_custom_target(doc DEPENDS doxygen)
endif(DOXYGEN_FOUND)

