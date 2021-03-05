/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <pybind11/pybind11.h>

#include "H5Lite.h"
#include "RecordObject.h"

/******************************************************************************
 * NAMESPACES
 ******************************************************************************/

namespace py = pybind11;

/******************************************************************************
 * H5Lite Class
 ******************************************************************************/

struct H5LiteFile
{
    H5LiteFile(const std::string &_url): url(_url) { }

    int read(const std::string &datasetname, int valtype, long col, long startrow, long numrows) 
    { 
        if(numrows < 0) numrows = H5Lite::ALL_ROWS; // workaround for binding to default argument value
        H5Lite::info_t info = H5Lite::read(url.c_str(), datasetname.c_str(), (RecordObject::valType_t)valtype, col, startrow, numrows, &context);
        if(info.data) delete [] info.data;
        return (int)info.datasize;
    }

    std::string url;
    H5Lite::context_t context;
};

/******************************************************************************
 * BINDINGS
 ******************************************************************************/

PYBIND11_MODULE(h5coro, m) 
{
    m.doc() = "H5Lite module for read-only access to *.h5 files";

    py::class_<H5LiteFile>(m, "file")
        .def(py::init<const std::string &>())
        .def("read", &H5LiteFile::read, "reads dataset from file", 
            py::arg("dataset"), 
            py::arg("type"), 
            py::arg("col") = 0, 
            py::arg("startrow") = 0, 
            py::arg("numrows") = -1);

    m.attr("all") = (long)-1;
    m.attr("float") = (long)RecordObject::REAL;
    m.attr("int") = (long)RecordObject::INTEGER;
}
