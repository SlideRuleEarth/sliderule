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

#include "H5Coro.h"
#include "RecordObject.h"

/******************************************************************************
 * NAMESPACES
 ******************************************************************************/

namespace py = pybind11;

/******************************************************************************
 * H5Coro Class
 ******************************************************************************/

struct H5LiteFile
{
    /*--------------------------------------------------------------------
     * constructor
     *--------------------------------------------------------------------*/
    H5LiteFile(const std::string &_url): url(_url) { }

    /*--------------------------------------------------------------------
     * read
     *--------------------------------------------------------------------*/
    py::list* read(const std::string &datasetname, long col, long startrow, long numrows) 
    { 
        // workaround for binding to default argument value
        if(numrows < 0) numrows = H5Coro::ALL_ROWS;

        // perform read of dataset
        H5Coro::info_t info = H5Coro::read(url.c_str(), datasetname.c_str(), RecordObject::DYNAMIC, col, startrow, numrows, &context);

        // build dataset array
        py::list* dataset = new py::list;
        if(info.datatype == RecordObject::REAL)
        {
            if(info.typesize == sizeof(double))
            {
                double* data_ptr = (double*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    dataset->append(data_ptr[i]);
                }
            }
            else if(info.typesize == sizeof(float))
            {
                float* data_ptr = (float*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    dataset->append(data_ptr[i]);
                }
            }
        }
        else if(info.datatype == RecordObject::INTEGER)
        {
            if(info.typesize == sizeof(long))
            {
                long* data_ptr = (long*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    dataset->append(data_ptr[i]);
                }
            }
            else if(info.typesize == sizeof(int))
            {
                int* data_ptr = (int*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    dataset->append(data_ptr[i]);
                }
            }
            else if(info.typesize == sizeof(short))
            {
                short* data_ptr = (short*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    dataset->append(data_ptr[i]);
                }
            }
            else if(info.typesize == sizeof(unsigned char))
            {
                unsigned char* data_ptr = (unsigned char*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    dataset->append(data_ptr[i]);
                }
            }
        }
        
        // clean up data
        if(info.data) delete [] info.data;

        // return list
        return dataset;
    }

    /*--------------------------------------------------------------------
     * data
     *--------------------------------------------------------------------*/
    std::string url;
    H5Coro::context_t context;
};

/******************************************************************************
 * BINDINGS
 ******************************************************************************/

PYBIND11_MODULE(h5coro, m) 
{
    m.doc() = "H5Coro module for read-only access to *.h5 files";

    py::class_<H5LiteFile>(m, "file")
        
        .def(py::init<const std::string &>())
        
        .def("read", &H5LiteFile::read, "reads dataset from file", 
            py::arg("dataset"), 
            py::arg("col") = 0, 
            py::arg("startrow") = 0, 
            py::arg("numrows") = -1);

    m.attr("all") = (long)-1;
}
