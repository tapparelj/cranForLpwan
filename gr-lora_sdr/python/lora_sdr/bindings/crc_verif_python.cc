/*
 * Copyright 2022 Free Software Foundation, Inc.
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
/* BINDTOOL_HEADER_FILE(crc_verif.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(f21d76dec9ed1d7a8019b05dbdcceee7) */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/lora_sdr/crc_verif.h>
// pydoc.h is automatically generated in the build directory
#include <crc_verif_pydoc.h>

void bind_crc_verif(py::module &m) {

  using crc_verif = ::gr::lora_sdr::crc_verif;

  py::class_<crc_verif, gr::block, gr::basic_block, std::shared_ptr<crc_verif>>(
      m, "crc_verif", D(crc_verif))

      .def(py::init(&crc_verif::make), py::arg("print_rx_msg"),
           py::arg("output_crc_check"), D(crc_verif, make))

      ;
}
