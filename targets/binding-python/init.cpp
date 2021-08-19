/*
 * Copyright (c) 2021, University of Washington
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the University of Washington nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************
 INCLUDES
 ******************************************************************************/

#include "core.h"

#ifdef __aws__
#include "aws.h"
#endif

#ifdef __ccsds__
#include "ccsds.h"
#endif

#ifdef __h5__
#include "h5.h"
#endif

#ifdef __legacy__
#include "legacy.h"
#endif

#ifdef __netsvc__
#include "netsvc.h"
#endif

#include <pybind11/pybind11.h>
#include "pyH5Coro.h"
#include "pyS3Cache.h"

/******************************************************************************
 * Namespaces
 ******************************************************************************/

namespace py = pybind11;

/******************************************************************************
 * Bindings
 ******************************************************************************/

PYBIND11_MODULE(srpybin, m)
{
    initcore();

    #ifdef __aws__
        initaws();
    #endif

    #ifdef __ccsds__
        initccsds();
    #endif

    #ifdef __h5__
        inith5();
    #endif

    #ifdef __legacy__
        initlegacy();
    #endif

    #ifdef __netsvc__
        initnetsvc();
    #endif

    m.doc() = "Python bindings for SlideRule on-demand data processing framework";

    py::class_<pyH5Coro>(m, "h5coro")

        .def(py::init<const std::string &,      // _resource
                      const std::string &,      // format
                      const std::string &,      // path
                      const std::string &,      // region
                      const std::string &>())   // endpoint

        .def("read", &pyH5Coro::read, "reads dataset from file",
            py::arg("dataset"),
            py::arg("col") = 0,
            py::arg("startrow") = 0,
            py::arg("numrows") = -1)

        .def("readp", &pyH5Coro::readp, "parallel read of datasets from file");

    m.attr("all") = (long)-1;

    py::class_<pyS3Cache>(m, "s3cache")

        .def(py::init<const std::string &,      // _cache_root
                      const int>());            // _max_files
}
