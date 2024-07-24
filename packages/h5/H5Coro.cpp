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

#include "MsgQ.h"
#include "OsApi.h"
#include "RecordObject.h"
#include "H5Dataset.h"
#include "H5Dense.h"
#include "H5Coro.h"

using H5Coro::range_t;
using H5Coro::Context;
using H5Coro::Future;
using H5Coro::MAX_NDIMS;

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

typedef struct {
    Context*                context;
    const char*             datasetname;
    RecordObject::valType_t valtype;
    range_t                 slice[MAX_NDIMS];
    int                     slicendims;
    uint32_t                traceid;
    Future*                 h5f;
} read_rqst_t;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

static Publisher*   rqstPub = NULL;
static Subscriber*  rqstSub = NULL;
static bool         readerActive = false;
static Thread**     readerPids = NULL;
static int          threadPoolSize = 0;

/******************************************************************************
 * FUTURE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5Coro::Future::Future (void)
{
    info.elements   = 0;
    info.typesize   = 0;
    info.datasize   = 0;
    info.data       = NULL;
    info.datatype   = RecordObject::INVALID_FIELD;

    for(int d = 0; d < MAX_NDIMS; d++)
    {
        info.shape[d] = 0;
    }

    complete        = false;
    valid           = false;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5Coro::Future::~Future (void)
{
    wait(IO_PEND);
    operator delete[](info.data, std::align_val_t(H5CORO_DATA_ALIGNMENT));
}

/*----------------------------------------------------------------------------
 * wait
 *----------------------------------------------------------------------------*/
H5Coro::Future::rc_t H5Coro::Future::wait (int timeout)
{
    rc_t rc;

    sync.lock();
    {
        if(!complete)
        {
            sync.wait(0, timeout);
        }

        if      (!valid)    rc = INVALID;
        else if (!complete) rc = TIMEOUT;
        else                rc = COMPLETE;
    }
    sync.unlock();

    return rc;
}

/*----------------------------------------------------------------------------
 * finish
 *----------------------------------------------------------------------------*/
 void H5Coro::Future::finish (bool _valid)
 {
    sync.lock();
    {
        valid = _valid;
        complete = true;
        sync.signal();
    }
    sync.unlock();
 }

/******************************************************************************
 * CONTEXT METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *  - assumes that asset is in scope for the duration this object is in scope
 *----------------------------------------------------------------------------*/
H5Coro::Context::Context (const Asset* asset, const char* resource):
    name                (NULL),
    ioDriver            (NULL),
    l1                  (IO_CACHE_L1_ENTRIES, hashL1),
    l2                  (IO_CACHE_L2_ENTRIES, hashL2),
    cache_miss          (0),
    l1_cache_replace    (0),
    l2_cache_replace    (0),
    bytes_read          (0)
{
    try
    {
        name = StringLib::duplicate(resource);
        ioDriver = asset->createDriver(resource);
    }
    catch(const RunTimeException& e)
    {
        delete [] name;
        mlog(e.level(), "Failed to create H5 context: %s", e.what());
        throw;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5Coro::Context::~Context (void)
{
    delete [] name;
    delete ioDriver;

    /* Empty L1 Cache */
    {
        cache_entry_t entry;
        uint64_t key = l1.first(&entry);
        while(key != INVALID_KEY)
        {
            assert(entry.data);
            delete [] entry.data;
            key = l1.next(&entry);
        }
    }

    /* Empty L2 Cache */
    {
        cache_entry_t entry;
        uint64_t key = l2.first(&entry);
        while(key != INVALID_KEY)
        {
            assert(entry.data);
            delete [] entry.data;
            key = l2.next(&entry);
        }
    }
}

/*----------------------------------------------------------------------------
 * ioRequest
 *----------------------------------------------------------------------------*/
void H5Coro::Context::ioRequest (uint64_t* pos, int64_t size, uint8_t* buffer, int64_t hint, bool cache_the_data)
{
    cache_entry_t entry;
    int64_t data_offset = 0;
    const uint64_t file_position = *pos;
    bool cached = false;
    mut.lock();
    {
        /* Attempt to fulfill data request from I/O cache
        *  note that this is only checked if a buffer is supplied;
        *  otherwise the purpose of the call is to cache the entry */
        if(buffer)
        {
            if( checkCache(file_position, size, &l1, IO_CACHE_L1_MASK, &entry) ||
                checkCache(file_position, size, &l2, IO_CACHE_L2_MASK, &entry) )
            {
                /* Entry Found in Cache */
                cached = true;

                /* Set Offset to Start of Requested Data */
                data_offset = file_position - entry.pos;

                /* Copy Data into Buffer */
                memcpy(buffer, &entry.data[data_offset], size);
            }
            else
            {
                /* Count Cache Miss */
                cache_miss++;
            }
        }
    }
    mut.unlock();

    /* Read data to fulfill request */
    if(!cached)
    {
        /* Cacluate How Much Data to Read and Set Data Buffer */
        int64_t read_size;
        if(cache_the_data)
        {
            read_size = MAX(size, hint); // overread when caching
            entry.data = new uint8_t [read_size];
        }
        else
        {
            assert(buffer); // logically inconsistent to not cache when buffer is null
            read_size = size;
            entry.data = buffer;
        }

        /* Mark File Position of Entry */
        entry.pos = file_position;

        /* Read into Cache */
        try
        {
            entry.size = ioDriver->ioRead(entry.data, read_size, entry.pos);
        }
        catch (const RunTimeException& e)
        {
            if(cache_the_data) delete [] entry.data;
            throw; // rethrow exception
        }

        /* Check Enough Data was Read */
        if(entry.size < size)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read %ld bytes of data: %ld", size, entry.size);
        }

        /* Handle Caching */
        if(cache_the_data)
        {
            /* Copy Data into Buffer
            *  only if caching, and buffer supplied;
            *  else the data was already read directly into the buffer,
            *  or the purpose was only to cache the entry (prefetch) */
            if(buffer)
            {
                memcpy(buffer, &entry.data[data_offset], size);
            }

            /* Select Cache */
            cache_t* cache = NULL;
            long* cache_replace = NULL;
            if(entry.size <= IO_CACHE_L1_LINESIZE)
            {
                cache = &l1;
                cache_replace = &l1_cache_replace;
            }
            else
            {
                cache = &l2;
                cache_replace = &l2_cache_replace;
            }

            /* Cache Entry */
            mut.lock();
            {
                /* Ensure Room in Cache */
                if(cache->isfull())
                {
                    /* Replace Oldest Entry */
                    cache_entry_t oldest_entry;
                    const uint64_t oldest_pos = cache->first(&oldest_entry);
                    if(oldest_pos != (uint64_t)INVALID_KEY)
                    {
                        delete [] oldest_entry.data;
                        cache->remove(oldest_pos);
                    }
                    else
                    {
                        mut.unlock();
                        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to make room in cache");
                    }

                    /* Count Cache Replacement */
                    (*cache_replace)++;
                }

                /* Add Cache Entry */
                if(!cache->add(file_position, entry, true))
                {
                    /* Free Previously Allocated Entry
                     *  should only fail to add if the cache line was
                     *  already added, in which case it is safe to just
                     *  delete what was allocated and move on */
                    delete [] entry.data;
                }

                /* Count Bytes Read */
                bytes_read += entry.size;
            }
            mut.unlock();
        }
        else // data not being cached
        {
            /* Count Bytes Read
             *  the logic below is repeated to avoid locking the ioContext mutex
             *  any more than it needs to be locked; the bytes_read could be updated
             *  in a single place below, but then the ioRequest function would always
             *  incur an additional mutex lock, whereas here it only occurs an aditional
             *  time when data isn't being cached (which is rare)
             */
            mut.lock();
            {
                bytes_read += entry.size;
            }
            mut.unlock();
        }
    }

    /* Update Position */
    *pos += size;
}

/*----------------------------------------------------------------------------
 * checkCache
 *----------------------------------------------------------------------------*/
bool H5Coro::Context::checkCache (uint64_t pos, int64_t size, cache_t* cache, uint64_t line_mask, cache_entry_t* entry)
{
    const uint64_t prev_line_pos = (pos & ~line_mask) - 1;
    const bool check_prev = pos > prev_line_pos; // checks for rollover

    if( cache->find(pos, cache_t::MATCH_NEAREST_UNDER, entry, true) ||
        (check_prev && cache->find(prev_line_pos, cache_t::MATCH_NEAREST_UNDER, entry, true)) )
    {
        if((pos >= entry->pos) && ((pos + size) <= (entry->pos + entry->size)))
        {
            return true;
        }
    }

    return false;
}

/*----------------------------------------------------------------------------
 * ioHashL1
 *----------------------------------------------------------------------------*/
uint64_t H5Coro::Context::hashL1 (uint64_t key)
{
    return key & (~IO_CACHE_L1_MASK);
}

/*----------------------------------------------------------------------------
 * ioHashL2
 *----------------------------------------------------------------------------*/
uint64_t H5Coro::Context::hashL2 (uint64_t key)
{
    return key & (~IO_CACHE_L2_MASK);
}

/******************************************************************************
 * H5CORO METHODS
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
            readerPids[t] = new Thread(readerThread, NULL);
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
H5Coro::info_t H5Coro::read (Context* context, const char* datasetname, RecordObject::valType_t valtype, const range_t* slice, int slicendims, bool _meta_only, uint32_t parent_trace_id)
{
    info_t info;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, parent_trace_id, "h5coro_read", "{\"context\":\"%s\", \"dataset\":\"%s\"}", context->name, datasetname);

    /* Open Resource and Read Dataset */
    const H5Dataset dataset(&info, context, datasetname, slice, slicendims, _meta_only);
    if(info.data)
    {
        bool data_valid = true;

        /* Perform Integer Type Translation */
        if(valtype == RecordObject::INTEGER)
        {
            /* Allocate Buffer of Integers */
            long* tbuf = new (std::align_val_t(H5CORO_DATA_ALIGNMENT)) long [info.elements];

            /* Float to Long */
            if(info.datatype == RecordObject::FLOAT)
            {
                const float* dptr = reinterpret_cast<float*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<long>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Double to Long */
            else if(info.datatype == RecordObject::DOUBLE)
            {
                const double* dptr = reinterpret_cast<double*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<long>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Char to Long */
            else if(info.datatype == RecordObject::INT8)
            {
                const int8_t* cptr = reinterpret_cast<int8_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<long>(cptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else if(info.datatype == RecordObject::UINT8)
            {
                const uint8_t* cptr = info.data;
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<long>(cptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
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
                    tbuf[i] = static_cast<long>(dptr[i]);
                }
                info.elements = length;
            }
            /* Short to Long */
            else if(info.datatype == RecordObject::INT16)
            {
                const int16_t* dptr = reinterpret_cast<int16_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<long>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else if(info.datatype == RecordObject::UINT16)
            {
                const uint16_t* dptr = reinterpret_cast<uint16_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<long>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Int to Long */
            else if(info.datatype == RecordObject::INT32)
            {
                const int32_t* dptr = reinterpret_cast<int32_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = dptr[i]; // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else if(info.datatype == RecordObject::UINT32)
            {
                const uint32_t* dptr = reinterpret_cast<uint32_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = dptr[i]; // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Long to Long */
            else if(info.datatype == RecordObject::INT64)
            {
                const int64_t* dptr = reinterpret_cast<int64_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<long>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else if(info.datatype == RecordObject::UINT64)
            {
                const uint64_t* dptr = reinterpret_cast<uint64_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<long>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Invalid */
            else
            {
                data_valid = false;
            }

            /* Switch Buffers */
            operator delete[](info.data, std::align_val_t(H5CORO_DATA_ALIGNMENT));
            info.data = reinterpret_cast<uint8_t*>(tbuf);
            info.datasize = sizeof(long) * info.elements;
        }
        /* Perform Integer Type Transaltion */
        else if(valtype == RecordObject::REAL)
        {
            /* Allocate Buffer of doubles */
            double* tbuf = new (std::align_val_t(H5CORO_DATA_ALIGNMENT)) double [info.elements];

            /* Float to Double */
            if(info.datatype == RecordObject::FLOAT)
            {
                const float* dptr = reinterpret_cast<float*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]); // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Double to Double */
            else if(info.datatype == RecordObject::DOUBLE)
            {
                const double* dptr = reinterpret_cast<double*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = dptr[i]; // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Char to Double */
            else if(info.datatype == RecordObject::INT8)
            {
                const int8_t* dptr = reinterpret_cast<int8_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else if(info.datatype == RecordObject::UINT8)
            {
                const uint8_t* dptr = info.data;
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Short to Double */
            else if(info.datatype == RecordObject::INT16)
            {
                const int16_t* dptr = reinterpret_cast<int16_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else if(info.datatype == RecordObject::UINT16)
            {
                const uint16_t* dptr = reinterpret_cast<uint16_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Int to Double */
            else if(info.datatype == RecordObject::INT32)
            {
                const int32_t* dptr = reinterpret_cast<int32_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else if(info.datatype == RecordObject::UINT32)
            {
                const uint32_t* dptr = reinterpret_cast<uint32_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Long to Double */
            else if(info.datatype == RecordObject::INT64)
            {
                const int64_t* dptr = reinterpret_cast<int64_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            else if(info.datatype == RecordObject::UINT64)
            {
                const uint64_t* dptr = reinterpret_cast<uint64_t*>(info.data);
                for(uint32_t i = 0; i < info.elements; i++)
                {
                    tbuf[i] = static_cast<double>(dptr[i]);  // NOLINT(clang-analyzer-core.uninitialized.Assign)
                }
            }
            /* Invalid */
            else
            {
                data_valid = false;
            }

            /* Switch Buffers */
            operator delete[](info.data, std::align_val_t(H5CORO_DATA_ALIGNMENT));
            info.data = reinterpret_cast<uint8_t*>(tbuf);
            info.datasize = sizeof(double) * info.elements;
        }

        /* Check Data Valid */
        if(!data_valid)
        {
            operator delete[](info.data, std::align_val_t(H5CORO_DATA_ALIGNMENT));
            info.data = NULL;
            info.datasize = 0;
            throw RunTimeException(CRITICAL, RTE_ERROR, "data translation failed for %s: [%d] %d --> %d", datasetname, info.typesize, (int)info.datatype, (int)valtype);
        }
    }
    else if(!_meta_only)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read dataset: %s", datasetname);
    }

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Log Info Message */
    mlog(DEBUG, "Read %d elements (%ld bytes) from %s/%s", info.elements, info.datasize, context->name, datasetname);

    /* Return Info */
    return info;
}

/*----------------------------------------------------------------------------
 * readp
 *----------------------------------------------------------------------------*/
H5Coro::Future* H5Coro::readp (Context* context, const char* datasetname, RecordObject::valType_t valtype, const range_t* slice, int slicendims)
{
    read_rqst_t rqst = {
        .context        = context,
        .datasetname    = StringLib::duplicate(datasetname),
        .valtype        = valtype,
        .slice          = {{EOR, EOR}, {EOR, EOR}, {EOR, EOR}},
        .slicendims     = slicendims,
        .traceid        = EventLib::grabId(),
        .h5f            = new H5Coro::Future()
    };

    const int ndims = MIN(H5Coro::MAX_NDIMS, slicendims);
    for(int d = 0; d < ndims; d++)
    {
        rqst.slice[d] = slice[d];
    }

    const int post_status = rqstPub->postCopy(&rqst, sizeof(read_rqst_t), IO_CHECK);
    if(post_status <= 0)
    {
        mlog(CRITICAL, "Failed to post read request for %s/%s: %d", context->name, datasetname, post_status);
        delete [] rqst.datasetname;
        delete rqst.h5f;
        return NULL;
    }

    return rqst.h5f;
}

/*----------------------------------------------------------------------------
 * readerThread
 *----------------------------------------------------------------------------*/
void* H5Coro::readerThread (void* parm)
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
                rqst.h5f->info = read(rqst.context, rqst.datasetname, rqst.valtype, rqst.slice, rqst.slicendims, false, rqst.traceid);
                valid = true;
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "Failure reading %s/%s: %s", rqst.context->name, rqst.datasetname, e.what());
                valid = false;
            }

            /* Free Request */
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
