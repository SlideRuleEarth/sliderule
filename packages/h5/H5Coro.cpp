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

#include "H5Coro.h"
#include "H5Dense.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "RecordObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Publisher*   H5Coro::rqstPub;
Subscriber*  H5Coro::rqstSub;
bool         H5Coro::readerActive;
Thread**     H5Coro::readerPids;
int          H5Coro::threadPoolSize;

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5Coro::init (int num_threads)
{
    rqstPub = new Publisher(NULL);

    if(num_threads > 0)
    {
        readerActive = true;
        rqstSub = new Subscriber(*rqstPub);
        threadPoolSize = num_threads;
        readerPids = new Thread* [threadPoolSize];
        for(int t = 0; t < threadPoolSize; t++)
        {
            readerPids[t] = new Thread(reader_thread, NULL);
        }
    }
    else
    {
        readerActive = false;
        rqstSub = NULL;
        threadPoolSize = 0;
        readerPids = NULL;
    }
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void H5Coro::deinit (void)
{
    if(readerActive)
    {
        readerActive = false;
        for(int t = 0; t < threadPoolSize; t++)
        {
            delete readerPids[t];
        }
        delete [] readerPids;
        delete rqstSub;
    }

    delete rqstPub;
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
H5Coro::info_t H5Coro::read (const Asset* asset, const char* resource, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows, context_t* context, bool _meta_only, uint32_t parent_trace_id)
{
    info_t info;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, parent_trace_id, "h5coro_read", "{\"asset\":\"%s\", \"resource\":\"%s\", \"dataset\":\"%s\"}", asset->getName(), resource, datasetname);

    /* Open Resource and Read Dataset */
    const H5Dataset dataset(&info, context, asset, resource, datasetname, startrow, numrows, _meta_only);
    if(info.data)
    {
        bool data_valid = true;
        /* Perform Column Translation */
        if((info.numcols > 1) && (col != ALL_COLS))
        {
            /* Allocate Column Buffer */
            const int64_t tbuf_size = info.datasize / info.numcols;
            uint8_t* tbuf = new uint8_t [tbuf_size];

            /* Copy Column into Buffer */
            const int64_t tbuf_row_size = info.datasize / info.numrows;
            const int64_t tbuf_col_size = tbuf_row_size / info.numcols;
            for(int row = 0; row < info.numrows; row++)
            {
                const int64_t tbuf_offset = (row * tbuf_col_size);
                const int64_t data_offset = (row * tbuf_row_size) + (col * tbuf_col_size);
                memcpy(&tbuf[tbuf_offset], &info.data[data_offset], tbuf_col_size);
            }

            /* Switch Buffers */
            delete [] info.data;
            info.data = tbuf;
            info.datasize = tbuf_size;
            info.elements = info.elements / info.numcols;
        }

        /* Perform Integer Type Translation */
        if(valtype == RecordObject::INTEGER)
        {
            /* Allocate Buffer of Integers */
            int* tbuf = new int [info.elements];

            /* Float to Int */
            if(info.datatype == RecordObject::FLOAT)
            {
                float* dptr = reinterpret_cast<float*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<int>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Double to Int */
            else if(info.datatype == RecordObject::DOUBLE)
            {
                double* dptr = reinterpret_cast<double*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<int>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Char to Int */
            else if(info.datatype == RecordObject::UINT8 || info.datatype == RecordObject::INT8)
            {
                char* cptr = reinterpret_cast<char*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<int>(cptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* String to Int - assumes ASCII encoding */
            else if(info.datatype == RecordObject::STRING)
            {
                uint8_t* dptr = info.data;

                // NOTE this len calc is redundant, but metaData not visible to scope
                uint8_t* len_cnt = dptr;
                uint32_t length = 0;
                while (*len_cnt != '\0') {  // NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)
                    length++;
                    len_cnt++;
                }
                for(uint32_t i = 0; i < length; i++)
                {
                    tbuf[i] = static_cast<int>(dptr[i]);
                }
                info.elements = length;
            }

            /* Short to Int */
            else if(info.datatype == RecordObject::UINT16 || info.datatype == RecordObject::INT16)
            {
                uint16_t* dptr = reinterpret_cast<uint16_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<int>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Int to Int */
            else if(info.datatype == RecordObject::UINT32 || info.datatype == RecordObject::INT32)
            {
                int* dptr = reinterpret_cast<int*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = dptr[i]; // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Long to Int */
            else if(info.datatype == RecordObject::UINT64 || info.datatype == RecordObject::INT64)
            {
                int64_t* dptr = reinterpret_cast<int64_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<int>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else
            {
                data_valid = false;
            }

            /* Switch Buffers */
            delete [] info.data;
            info.data = reinterpret_cast<uint8_t*>(tbuf);
            info.datasize = sizeof(int) * info.elements;
        }

        /* Perform Integer Type Transaltion */
        if(valtype == RecordObject::REAL)
        {
            /* Allocate Buffer of Integers */
            double* tbuf = new double [info.elements];

            /* Float to Double */
            if(info.datatype == RecordObject::FLOAT)
            {
                float* dptr = reinterpret_cast<float*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Double to Double */
            else if(info.datatype == RecordObject::DOUBLE)
            {
                double* dptr = reinterpret_cast<double*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = dptr[i]; // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Char to Double */
            else if(info.datatype == RecordObject::UINT8 || info.datatype == RecordObject::INT8)
            {
                uint8_t* dptr = info.data;
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Short to Double */
            else if(info.datatype == RecordObject::UINT16 || info.datatype == RecordObject::INT16)
            {
                uint16_t* dptr = reinterpret_cast<uint16_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Int to Double */
            else if(info.datatype == RecordObject::UINT32 || info.datatype == RecordObject::INT32)
            {
                uint32_t* dptr = reinterpret_cast<uint32_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Long to Double */
            else if(info.datatype == RecordObject::UINT64 || info.datatype == RecordObject::INT64)
            {
                uint64_t* dptr = reinterpret_cast<uint64_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else
            {
                data_valid = false;
            }

            /* Switch Buffers */
            delete [] info.data;
            info.data = reinterpret_cast<uint8_t*>(tbuf);
            info.datasize = sizeof(double) * info.elements;
        }

        /* Check Data Valid */
        if(!data_valid)
        {
            delete [] info.data;
            info.data = NULL;
            info.datasize = 0;
            throw RunTimeException(CRITICAL, RTE_ERROR, "data translation failed for %s: [%d,%d] %d --> %d", datasetname, info.numcols, info.typesize, (int)info.datatype, (int)valtype);
        }
    }
    else if(!_meta_only)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read dataset: %s", datasetname);
    }

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Log Info Message */
    mlog(DEBUG, "Read %d elements (%ld bytes) from %s/%s", info.elements, info.datasize, asset->getName(), datasetname);

    /* Return Info */
    return info;
}

/*----------------------------------------------------------------------------
 * traverse
 *----------------------------------------------------------------------------*/
bool H5Coro::traverse (const Asset* asset, const char* resource, int max_depth, const char* start_group)
{
    (void)max_depth;

    const bool status = true;

    try
    {
        /* Open File */
        info_t info;
        const H5Dataset dataset(&info, NULL, asset, resource, start_group, 0, 0);

        /* Free Data */
        delete [] info.data;
    }
    catch (const RunTimeException& e)
    {
        mlog(e.level(), "Failed to traverse resource: %s", e.what());
    }

    /* Return Status */
    return status;
}


/*----------------------------------------------------------------------------
 * readp
 *----------------------------------------------------------------------------*/
H5Future* H5Coro::readp (const Asset* asset, const char* resource, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows, context_t* context)
{
    read_rqst_t rqst = {
        .asset          = asset,
        .resource       = StringLib::duplicate(resource),
        .datasetname    = StringLib::duplicate(datasetname),
        .valtype        = valtype,
        .col            = col,
        .startrow       = startrow,
        .numrows        = numrows,
        .context        = context,
        .traceid        = EventLib::grabId(),
        .h5f            = new H5Future()
    };

    const int post_status = rqstPub->postCopy(&rqst, sizeof(read_rqst_t), IO_CHECK);
    if(post_status <= 0)
    {
        mlog(CRITICAL, "Failed to post read request for %s/%s: %d", resource, datasetname, post_status);
        delete [] rqst.resource;
        delete [] rqst.datasetname;
        delete rqst.h5f;
        return NULL;
    }

    return rqst.h5f;
}

/*----------------------------------------------------------------------------
 * reader_thread
 *----------------------------------------------------------------------------*/
void* H5Coro::reader_thread (void* parm)
{
    (void)parm;

    while(readerActive)
    {
        read_rqst_t rqst;
        const int recv_status = rqstSub->receiveCopy(&rqst, sizeof(read_rqst_t), SYS_TIMEOUT);
        if(recv_status > 0)
        {
            bool valid;
            try
            {
                rqst.h5f->info = read(rqst.asset, rqst.resource, rqst.datasetname, rqst.valtype, rqst.col, rqst.startrow, rqst.numrows, rqst.context, false, rqst.traceid);
                valid = true;
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "Failure reading %s://%s/%s: %s", rqst.asset->getName(), rqst.resource, rqst.datasetname, e.what());
                valid = false;
            }

            /* Free Request */
            delete [] rqst.resource;
            delete [] rqst.datasetname;

            /* Signal Complete */
            rqst.h5f->finish(valid);
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "Failed to receive read request: %d", recv_status);
            break;
        }
    }

    return NULL;
}
