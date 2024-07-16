# Author: Tapparel Joachim@TCL,EPFL

cmake_minimum_required(VERSION 3.8)
project(cran_bbu)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
include_directories(
    ${GNURADIO_ALL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    $ENV{HOME}/.grc_gnuradio
    ${GNURADIO_PREFIX}/include/gnuradio/lora_sdr/  
    ${GNURADIO_PREFIX}/include/gnuradio/cran/
)

add_subdirectory(lora_rx_worker)
add_subdirectory(lora_rx_sync_worker)
add_subdirectory(lora_rx_demod_worker)
add_subdirectory(lora_rx_demod_worker_perf_meas)
add_subdirectory(lora_rx_buffer)
set(CMAKE_CXX_STANDARD 14)
find_package(Gnuradio "3.10" COMPONENTS
    blocks
    uhd
    uhd UHD 4.0 RFNoC
)

set(GR_LIBRARIES
    boost_system
    gnuradio::gnuradio-blocks
    gnuradio-uhd
    gnuradio-filter
    uhd
    ${GNURADIO_PREFIX}/lib/libgnuradio-lora_sdr.so
    ${GNURADIO_PREFIX}/lib/libgnuradio-cran.so
)
add_library(bbu_lib broker.cpp)
target_link_libraries(bbu_lib ${GR_LIBRARIES} lora_rx_worker lora_rx_sync_worker lora_rx_demod_worker lora_rx_demod_worker_perf_meas lora_rx_buffer)
