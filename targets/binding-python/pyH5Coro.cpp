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
#include <exception>

#include "RecordObject.h"
#include "H5Coro.h"
#include "pyH5Coro.h"
#include "Asset.h"

/******************************************************************************
 * NAMESPACES
 ******************************************************************************/

namespace py = pybind11;

/******************************************************************************
 * Static
 ******************************************************************************/

Mutex pyH5Coro::pyMut;

/******************************************************************************
 * pyH5Coro Class
 ******************************************************************************/

/*--------------------------------------------------------------------
 * Constructor
 *--------------------------------------------------------------------*/
pyH5Coro::pyH5Coro (const std::string &_resource, const std::string &format, const std::string &path, const std::string &region, const std::string &endpoint):
    resource(_resource)
{
    asset = Asset::pythonCreate(format.c_str(), path.c_str(), NULL, region.c_str(), endpoint.c_str());
    if(asset == NULL) throw std::invalid_argument("failed to create asset, likely missing driver for provided format");
}

/*--------------------------------------------------------------------
 * Destructor
 *--------------------------------------------------------------------*/
pyH5Coro::~pyH5Coro (void)
{
    delete asset;
}

/*--------------------------------------------------------------------
 * meta
 *--------------------------------------------------------------------*/
py::dict pyH5Coro::meta (const std::string &datasetname, long col, long startrow, long numrows)
{
    py::dict result;

    // workaround for binding to default argument value
    if(numrows < 0) numrows = H5Coro::ALL_ROWS;

    // perform read of dataset
    H5Coro::info_t info = H5Coro::read(asset, resource.c_str(), datasetname.c_str(), RecordObject::DYNAMIC, col, startrow, numrows, &context, true);

    // construct meta dictionary
    result["elements"] = info.elements;
    result["typesize"] = info.typesize;
    result["datasize"] = info.datasize;
    result["datatype"] = RecordObject::ft2str(info.datatype);
    result["numcols"]  = info.numcols;
    result["numrows"]  = info.numrows;

    // return dictionary
    return result;
}

/*--------------------------------------------------------------------
 * read
 *--------------------------------------------------------------------*/
py::list pyH5Coro::read (const std::string &datasetname, long col, long startrow, long numrows)
{
    py::list result;

    // workaround for binding to default argument value
    if(numrows < 0) numrows = H5Coro::ALL_ROWS;

    // perform read of dataset
    H5Coro::info_t info = H5Coro::read(asset, resource.c_str(), datasetname.c_str(), RecordObject::DYNAMIC, col, startrow, numrows, &context);

    // build dataset array
    result = tolist(&info);

    // clean up data
    if(info.data) delete [] info.data;

    // return list
    return result;
}

/*--------------------------------------------------------------------
 * readp
 *--------------------------------------------------------------------*/
const py::dict pyH5Coro::readp (const py::list& datasets)
{
    py::dict result;
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
        rqst->e_ptr     = NULL;

        // workaround for binding to default argument value
        if(rqst->numrows < 0) rqst->numrows = H5Coro::ALL_ROWS;

        // spawn thread
        rqst->pid = new Thread(read_thread, rqst);
        readers.add(rqst);
    }

    // process results
    for(int i = 0; i < readers.length(); i++)
    {
        // wait for read to complete
        delete readers[i]->pid;

        // check for exceptions
        if(readers[i]->e_ptr)
        {
            std::rethrow_exception(readers[i]->e_ptr);
        }

        // populate result dictionary
        pyMut.lock();
        {
            py::str key(readers[i]->dataset);
            result[key] = readers[i]->result;
        }
        pyMut.unlock();

        // clean up data
        if(readers[i]->info.data) delete [] readers[i]->info.data;

        // clean up request
        delete readers[i];
    }

    // return result dictionary
    return result;
}

/*--------------------------------------------------------------------
 * stat
 *--------------------------------------------------------------------*/
py::dict pyH5Coro::stat (void)
{
    py::dict stats;
    stats["pre_prefetch_request"]   = context.pre_prefetch_request;
    stats["post_prefetch_request"]  = context.post_prefetch_request;
    stats["cache_miss"]             = context.cache_miss;
    stats["l1_cache_replace"]       = context.l1_cache_replace;
    stats["l2_cache_replace"]       = context.l2_cache_replace;
    return stats;
}

/*--------------------------------------------------------------------
 * tolist
 *--------------------------------------------------------------------*/
py::list pyH5Coro::tolist (H5Coro::info_t* info)
{
    py::list result;

    if(info->data)
    {
        if(info->datatype == RecordObject::DOUBLE)
        {
            double* data_ptr = (double*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::FLOAT)
        {
            float* data_ptr = (float*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::INT64)
        {
            int64_t* data_ptr = (int64_t*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::UINT64)
        {
            uint64_t* data_ptr = (uint64_t*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::INT32)
        {
            int32_t* data_ptr = (int32_t*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::UINT32)
        {
            uint32_t* data_ptr = (uint32_t*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::INT16)
        {
            int16_t* data_ptr = (int16_t*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::UINT16)
        {
            uint16_t* data_ptr = (uint16_t*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::INT8)
        {
            int8_t* data_ptr = (int8_t*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::UINT8)
        {
            uint8_t* data_ptr = (uint8_t*)info->data;
            for(int i = 0; i < info->elements; i++)
            {
                result.append(data_ptr[i]);
            }
        }
        else if(info->datatype == RecordObject::STRING)
        {
            char* data_ptr = (char*)info->data;
            data_ptr[info->datasize - 1] = '\0';
            py::str data_str(data_ptr);
            result.append(data_str);
        }
    }

    return result;
}

/*--------------------------------------------------------------------
 * read_thread
 *--------------------------------------------------------------------*/
void* pyH5Coro::read_thread (void* parm)
{
    read_rqst_t* rqst = (read_rqst_t*)parm;

    try
    {
        // perform read of dataset
        rqst->info = H5Coro::read(rqst->file->asset, rqst->file->resource.c_str(), rqst->dataset.c_str(), RecordObject::DYNAMIC, rqst->col, rqst->startrow, rqst->numrows, &rqst->file->context);

        // build dataset array
        pyMut.lock();
        {
            rqst->result = rqst->file->tolist(&rqst->info);
        }
        pyMut.unlock();
    }
    catch(...)
    {
        rqst->e_ptr = std::current_exception();
    }

    // exit
    return NULL;
}

