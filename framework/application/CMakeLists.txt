# Author: Tapparel Joachim@TCL,EPFL

cmake_minimum_required(VERSION 3.8)
project(cran_application)

set(CMAKE_CXX_STANDARD 14)

find_package(Boost REQUIRED COMPONENTS system filesystem)

include_directories(
    ${GNURADIO_ALL_INCLUDE_DIRS} 
    ${Boost_INCLUDE_DIRS}
    ${GNURADIO_PREFIX}/include/gnuradio/cran/
)
set(LIBRARIES
    boost_system
    boost_filesystem
    #log4cpp
    ${GNURADIO_PREFIX}/lib/libgnuradio-cran.so
)

add_library(application_lib application.cpp)
target_link_libraries(application_lib ${LIBRARIES})
