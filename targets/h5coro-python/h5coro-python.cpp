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
     * typedefs
     *--------------------------------------------------------------------*/
    typedef struct {
        std::string     dataset;
        int             col;
        int             startrow;
        int             numrows;
        Thread*         pid;
        H5Coro::info_t  info;
        py::list*       result;
        H5LiteFile*     file;
    } read_rqst_t;

    /*--------------------------------------------------------------------
     * constructor
     *--------------------------------------------------------------------*/
    H5LiteFile(const std::string &_url): url(_url) { }

    /*--------------------------------------------------------------------
     * tolist
     *--------------------------------------------------------------------*/
    py::list* tolist(H5Coro::info_t* info)
    {
        py::list* result = new py::list;

        if(info->datatype == RecordObject::REAL)
        {
            if(info->typesize == sizeof(double))
            {
                double* data_ptr = (double*)info->data;
                for(int i = 0; i < info->elements; i++)
                {
                    result->append(data_ptr[i]);
                }
            }
            else if(info->typesize == sizeof(float))
            {
                float* data_ptr = (float*)info->data;
                for(int i = 0; i < info->elements; i++)
                {
                    result->append(data_ptr[i]);
                }
            }
        }
        else if(info->datatype == RecordObject::INTEGER)
        {
            if(info->typesize == sizeof(long))
            {
                long* data_ptr = (long*)info->data;
                for(int i = 0; i < info->elements; i++)
                {
                    result->append(data_ptr[i]);
                }
            }
            else if(info->typesize == sizeof(int))
            {
                int* data_ptr = (int*)info->data;
                for(int i = 0; i < info->elements; i++)
                {
                    result->append(data_ptr[i]);
                }
            }
            else if(info->typesize == sizeof(short))
            {
                short* data_ptr = (short*)info->data;
                for(int i = 0; i < info->elements; i++)
                {
                    result->append(data_ptr[i]);
                }
            }
            else if(info->typesize == sizeof(unsigned char))
            {
                unsigned char* data_ptr = (unsigned char*)info->data;
                for(int i = 0; i < info->elements; i++)
                {
                    result->append(data_ptr[i]);
                }
            }
        }

        return result;
    }

    /*--------------------------------------------------------------------
     * read_thread
     *--------------------------------------------------------------------*/
    static void* read_thread (void* parm)
    {
        read_rqst_t* rqst = (read_rqst_t*)parm;

        // perform read of dataset
        rqst->info = H5Coro::read(rqst->file->url.c_str(), rqst->dataset.c_str(), RecordObject::DYNAMIC, rqst->col, rqst->startrow, rqst->numrows, &rqst->file->context);

        // build dataset array
        rqst->result = rqst->file->tolist(&rqst->info);

        // clean up data
        if(rqst->info.data) delete [] rqst->info.data;

        // exit
        return NULL;
    }

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
        py::list* result = tolist(&info);

        // clean up data
        if(info.data) delete [] info.data;

        // return list
        return result;
    }

    /*--------------------------------------------------------------------
     * readp
     *--------------------------------------------------------------------*/
    const py::dict* readp(const py::list& datasets) 
    {
        List<read_rqst_t*> readers;

        // traverse list of datasets to read
        for(auto entry : datasets)
        {
            // build request
            read_rqst_t* rqst = new read_rqst_t;
            rqst->dataset   = py::cast<std::string>(PyList_GetItem(entry.ptr(), 0));
            rqst->col       = py::cast<int>(PyList_GetItem(entry.ptr(), 1));
            rqst->startrow  = py::cast<int>(PyList_GetItem(entry.ptr(), 2));
            rqst->numrows   = py::cast<int>(PyList_GetItem(entry.ptr(), 3));
            rqst->file      = this;

            // workaround for binding to default argument value
            if(rqst->numrows < 0) rqst->numrows = H5Coro::ALL_ROWS;

            // spawn thread
            rqst->pid = new Thread(read_thread, rqst);
            readers.add(rqst);
        }

        // process results
        py::dict* result = new py::dict;
        for(int i = 0; i < readers.length(); i++)
        {
            // wait for read to complete
            delete readers[i]->pid;

            // populate result dictionary
            py::str key(readers[i]->dataset);
            (*result)[key] = readers[i]->result;

            // clean up request
            delete readers[i];
        }

        // return result dictionary
        return result;
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
            py::arg("numrows") = -1)

        .def("readp", &H5LiteFile::readp, "parallel read of datasets from file");

    m.attr("all") = (long)-1;
}
