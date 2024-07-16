# Author: Tapparel Joachim@TCL,EPFL


cmake_minimum_required(VERSION 3.8)


include_directories(
    ${GNURADIO_ALL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${GNURADIO_PREFIX}/include/gnuradio/lora_sdr/
    ${GNURADIO_PREFIX}/include/gnuradio/cran/
    #../${GR_CRAN_DIR}/include
)

set(CMAKE_CXX_STANDARD 14)
find_package(Gnuradio "3.10" COMPONENTS
    blocks
)

set(GR_LIBRARIES
    boost_system
    boost_filesystem
    gnuradio::gnuradio-blocks
    gnuradio-uhd
    uhd
)

set(BUFFER_SRCS lora_rx_buffer.cpp ../bbu.cpp )
add_library(lora_rx_buffer lora_rx_buffer.cpp)
target_link_libraries(lora_rx_buffer ${GR_LIBRARIES})
