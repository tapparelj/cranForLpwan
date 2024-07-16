# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT cran_sources)
    MESSAGE(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT cran_sources)

########################################################################
# Check for pygccxml
########################################################################
GR_PYTHON_CHECK_MODULE_RAW(
    "pygccxml"
    "import pygccxml"
    PYGCCXML_FOUND
    )

include(GrPybind)

########################################################################
# Python Bindings
########################################################################
list(APPEND cran_python_files
    end_node_zmq_sink_python.cc
    rrh_zmq_sink_python.cc
    worker_zmq_source_python.cc
    rx_combining_python.cc
    zmq_dealer_sink_python.cc
    lora_frame_sync_python.cc
    worker_zmq_sink_python.cc
    lora_frame_detector_single_sf_python.cc python_bindings.cc)

GR_PYBIND_MAKE_OOT(cran
   ../../..
   gr::cran
   "${cran_python_files}")

# copy bindings extension for use in QA test module
add_custom_command(TARGET cran_python POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cran_python>
    ${CMAKE_BINARY_DIR}/test_modules/gnuradio/cran/
)

install(TARGETS cran_python DESTINATION ${GR_PYTHON_DIR}/gnuradio/cran COMPONENT pythonapi)
