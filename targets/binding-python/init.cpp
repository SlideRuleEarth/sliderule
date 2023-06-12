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

#ifdef __geo__
#include "geo.h"
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

#ifdef __icesat2__
#include "icesat2.h"
#endif

#include <pybind11/pybind11.h>
#include "pyH5Coro.h"
#include "pyLua.h"
#include "pyLogger.h"
#include "pyS3Cache.h"
#include "pyCredentialStore.h"
#include "pyPlugin.h"

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

    #ifdef __geo__
        initgeo();
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

    #ifdef __icesat2__
        initicesat2();
    #endif

    m.doc() = "Python bindings for SlideRule on-demand data processing framework";

    py::class_<pyH5Coro>(m, "h5coro")

        .def(py::init<const std::string &,      // _asset
                      const std::string &,      // _resource
                      const std::string &,      // format
                      const std::string &,      // path
                      const std::string &,      // region
                      const std::string &>())   // endpoint

        .def("meta", &pyH5Coro::meta, "reads meta information for dataset from file",
            py::arg("dataset"),
            py::arg("col") = 0,
            py::arg("startrow") = 0,
            py::arg("numrows") = -1)

        .def("read", &pyH5Coro::read, "reads dataset from file",
            py::arg("dataset"),
            py::arg("col") = 0,
            py::arg("startrow") = 0,
            py::arg("numrows") = -1)

        .def("readp", &pyH5Coro::readp, "parallel read of datasets from file")

        .def("stat", &pyH5Coro::stat, "returns statistics");

    py::class_<pyS3Cache>(m, "s3cache")

        .def(py::init<const std::string &,      // _cache_root
                      const int>());            // _max_files

    py::class_<pyCredentialStore>(m, "credentials")

        .def(py::init<const std::string &>())   // _asset

        .def("provide", &pyCredentialStore::provide, "provide credentials for an asset",
            py::arg("credential"))

        .def("retrieve", &pyCredentialStore::retrieve, "retrieve credentials for an asset");

    py::class_<pyLua>(m, "lua")

        .def(py::init<const std::string &,      // scriptpath
                      const std::string &>());  // scriptarg

    py::class_<pyPlugin>(m, "plugin")

        .def(py::init<const std::string &>());  // full path to plugin

    py::class_<pyLogger>(m, "logger")

        .def(py::init<const int>())

        .def("critical", &pyLogger::log, "generates critical log message", py::arg("msg"), py::arg("level") = (int)CRITICAL)
        .def("error",    &pyLogger::log, "generates error log message",    py::arg("msg"), py::arg("level") = (int)ERROR)
        .def("warning",  &pyLogger::log, "generates warning log message",  py::arg("msg"), py::arg("level") = (int)WARNING)
        .def("info",     &pyLogger::log, "generates info log message",     py::arg("msg"), py::arg("level") = (int)INFO)
        .def("debug",    &pyLogger::log, "generates debug log message",    py::arg("msg"), py::arg("level") = (int)DEBUG);

    m.attr("all")       = (long)-1;
    m.attr("CRITICAL")  = (int)CRITICAL;
    m.attr("ERROR")     = (int)ERROR;
    m.attr("WARNING")   = (int)WARNING;
    m.attr("INFO")      = (int)INFO;
    m.attr("DEBUG")     = (int)DEBUG;

    // Register a callback function that is invoked when the BaseClass object is collected
    py::cpp_function cleanup_callback(
        [](py::handle weakref) {

            #ifdef __icesat2__
                deiniticesat2();
            #endif

            #ifdef __netsvc__
                deinitnetsvc();
            #endif

            #ifdef __legacy__
                deinitlegacy();
            #endif

            #ifdef __h5__
                deinith5();
            #endif

            #ifdef __geo__
                deinitgeo();
            #endif

            #ifdef __ccsds__
                deinitccsds();
            #endif

            #ifdef __aws__
                deinitaws();
            #endif

            deinitcore();

            weakref.dec_ref(); // release weak reference

            /*
             * This is a hack to avoid a coredump from exiting
             * python when this module is loaded in a conda environment
             * and the library was linked with a system environment. See
             * makefile target `config-python-conda` for the preferred
             * way to build bindings when using a conda environment.
             */
            #ifdef BEST_EFFORT_CONDA_ENV
            quick_exit(0);
            #endif
        }
    );

    // Create a weak reference with a cleanup callback and initially leak it
    (void) py::weakref(m.attr("logger"), cleanup_callback).release();
}
