# Author: Tapparel Joachim@TCL,EPFL

cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 14)
project(lora_cran_rrh)

include_directories(
    ${GNURADIO_ALL_INCLUDE_DIRS} 
    ${Boost_INCLUDE_DIRS}
    $ENV{HOME}/.grc_gnuradio
    ${GNURADIO_PREFIX}/include/gnuradio/lora_sdr/  
    ${GNURADIO_PREFIX}/include/gnuradio/cran/  
)
find_package(Gnuradio "3.10" COMPONENTS
    blocks
    zeromq
    gr-soapy
    uhd
    uhd UHD 4.0 RFNoC
)
set(GR_LIBRARIES
    boost_system
    gnuradio::gnuradio-blocks
    gnuradio::gnuradio-zeromq
    gnuradio-soapy
    gnuradio-uhd
    uhd 
    #log4cpp
    ${GNURADIO_PREFIX}/lib/libgnuradio-lora_sdr.so
    ${GNURADIO_PREFIX}/lib/libgnuradio-cran.so
    /usr/lib/aarch64-linux-gnu/libserial.so
    /usr/lib/aarch64-linux-gnu/libzmq.so

)
add_library(raspi_single_channel_rrh_lib raspi_single_channel_rrh.cpp)
target_link_libraries(raspi_single_channel_rrh_lib ${GR_LIBRARIES})
