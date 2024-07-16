# Author: Tapparel Joachim@TCL,EPFL


cmake_minimum_required(VERSION 3.8)


include_directories(
    ${GNURADIO_ALL_INCLUDE_DIRS} 
    ${Boost_INCLUDE_DIRS}
    $ENV{HOME}/.grc_gnuradio
    ${GNURADIO_PREFIX}/include/gnuradio/lora_sdr/  
    ${GNURADIO_PREFIX}/include/gnuradio/cran/  
)
set(CMAKE_CXX_STANDARD 14)
find_package(Gnuradio "3.10" COMPONENTS
    blocks
    zeromq
    analog
    filter
    fft
    channels
)


set(GR_LIBRARIES
    boost_system
    gnuradio::gnuradio-blocks
    gnuradio::gnuradio-zeromq
    gnuradio::gnuradio-channels
    gnuradio-uhd
    uhd
    ${GNURADIO_PREFIX}/lib/libgnuradio-lora_sdr.so
    ${GNURADIO_PREFIX}/lib/libgnuradio-cran.so
)

set(LLR_COMB_WORKER_PERF_MEAS_SRCS lora_rx_demod_worker_perf_meas.cpp ../bbu.cpp )
add_library(lora_rx_demod_worker_perf_meas lora_rx_demod_worker_perf_meas.cpp)
target_link_libraries(lora_rx_demod_worker_perf_meas ${GR_LIBRARIES})
