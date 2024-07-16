/*
 * Copyright 2023 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/***********************************************************************************/
/* This file is automatically generated using bindtool and can be manually
 * edited  */
/* The following lines can be configured to regenerate this file during cmake */
/* If manual edits are made, the following tags should be modified accordingly.
 */
/* BINDTOOL_GEN_AUTOMATIC(0) */
/* BINDTOOL_USE_PYGCCXML(0) */
/* BINDTOOL_HEADER_FILE(rrh_zmq_sink.h) */
/* BINDTOOL_HEADER_FILE_HASH(1fc7c9f816a969814ae77ed0b8c38f30) */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/cran/rrh_zmq_sink.h>
// pydoc.h is automatically generated in the build directory
#include <rrh_zmq_sink_pydoc.h>

void bind_rrh_zmq_sink(py::module &m) {

  using rrh_zmq_sink = ::gr::cran::rrh_zmq_sink;

  py::class_<rrh_zmq_sink, gr::sync_block, gr::block, gr::basic_block,
             std::shared_ptr<rrh_zmq_sink>>(m, "rrh_zmq_sink", D(rrh_zmq_sink))

      .def(py::init(&rrh_zmq_sink::make), py::arg("broker_addr"),
           py::arg("frequency_indices"), py::arg("samp_rate"),
           D(rrh_zmq_sink, make))

      ;
}