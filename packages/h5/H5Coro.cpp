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
#include "core.h"

#include <zlib.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#ifndef H5_VERBOSE
#define H5_VERBOSE false
#endif

#ifndef H5_EXTRA_DEBUG
#define H5_EXTRA_DEBUG false
#endif

#ifndef H5_ERROR_CHECKING
#define H5_ERROR_CHECKING true
#endif

/******************************************************************************
 * MACROS
 ******************************************************************************/

#define H5_INVALID(var)  (var == (0xFFFFFFFFFFFFFFFFllu >> (64 - (sizeof(var) * 8))))

/******************************************************************************
 * H5 FUTURE CLASS
 ******************************************************************************/

// NOLINTBEGIN(misc-no-recursion)

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5Future::H5Future (void)
{
    info.elements   = 0;
    info.typesize   = 0;
    info.datasize   = 0;
    info.data       = NULL;
    info.datatype   = RecordObject::INVALID_FIELD;
    info.numcols    = 0;
    info.numrows    = 0;

    complete        = false;
    valid           = false;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5Future::~H5Future (void)
{
    wait(IO_PEND);
    delete [] info.data;
}

/*----------------------------------------------------------------------------
 * wait
 *----------------------------------------------------------------------------*/
H5Future::rc_t H5Future::wait (int timeout)
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
 void H5Future::finish (bool _valid)
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
 * H5 FILE BUFFER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Static Data
 *----------------------------------------------------------------------------*/
H5FileBuffer::meta_repo_t H5FileBuffer::metaRepo(MAX_META_STORE);
Mutex H5FileBuffer::metaMutex;

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::io_context_t::io_context_t (void):
    l1(IO_CACHE_L1_ENTRIES, ioHashL1),
    l2(IO_CACHE_L2_ENTRIES, ioHashL2)
{
    pre_prefetch_request = 0;
    post_prefetch_request = 0;
    cache_miss = 0;
    l1_cache_replace = 0;
    l2_cache_replace = 0;
    bytes_read = 0;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::io_context_t::~io_context_t (void)
{
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
 * Constructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::H5FileBuffer (info_t* info, io_context_t* context, const Asset* asset, const char* resource, const char* dataset, long startrow, long numrows, bool _meta_only)
{
    assert(asset);
    assert(resource);
    assert(dataset);

    /* Initialize Class Data */
    ioDriver                = NULL;
    ioContextLocal          = true;
    ioContext               = NULL;
    ioBucket                = NULL;
    ioPostPrefetch          = false;
    dataChunkBuffer         = NULL;
    dataChunkFilterBuffer   = NULL;
    datasetName             = StringLib::duplicate(dataset);
    datasetPrint            = StringLib::duplicate(dataset);
    datasetStartRow         = startrow;
    datasetNumRows          = numrows;
    metaOnly                = _meta_only;
    ioKey                   = NULL;
    dataChunkBufferSize     = 0;
    highestDataLevel        = 0;
    dataSizeHint            = 0;

    /* Initialize Info */
    info->elements = 0;
    info->datasize = 0;
    info->data     = NULL;
    info->datatype = RecordObject::INVALID_FIELD;
    info->numrows  = 0;
    info->numcols  = 0;

    /* Process File */
    try
    {
        /* Initialize Driver */
        ioDriver = asset->createDriver(resource);

        /* Set or Create I/O Context */
        if(context)
        {
            ioContext = context;
            ioContextLocal = false;
        }
        else
        {
            ioContext = new io_context_t;
            ioContextLocal = true;
        }

        /* Check Meta Repository */
        char meta_url[MAX_META_NAME_SIZE];
        metaGetUrl(meta_url, resource, dataset);
        const uint64_t meta_key = metaGetKey(meta_url);
        bool meta_found = false;
        metaMutex.lock();
        {
            if(metaRepo.find(meta_key, meta_repo_t::MATCH_EXACTLY, &metaData, true))
            {
                meta_found = StringLib::match(metaData.url, meta_url);
            }
        }
        metaMutex.unlock();

        if(!meta_found)
        {
            /* Initialize Meta Data */
            memcpy(metaData.url, meta_url, MAX_META_NAME_SIZE);
            metaData.type           = UNKNOWN_TYPE;
            metaData.typesize       = UNKNOWN_VALUE;
            metaData.fill.fill_ll   = 0LL;
            metaData.fillsize       = 0;
            metaData.ndims          = UNKNOWN_VALUE;
            metaData.chunkelements  = 0;
            metaData.elementsize    = 0;
            metaData.offsetsize     = 0;
            metaData.lengthsize     = 0;
            metaData.layout         = UNKNOWN_LAYOUT;
            metaData.address        = 0;
            metaData.size           = 0;
            for(int f = 0; f < NUM_FILTERS; f++)
            {
                metaData.filter[f]  = INVALID_FILTER;
            }

            /* Get Dataset Path */
            parseDataset();

            /* Read Superblock */
            const uint64_t root_group_offset = readSuperblock();

            /* Read Data Attributes (Start at Root Group) */
            readObjHdr(root_group_offset, 0);
        }

        /* Read Dataset */
        readDataset(info);

        /* Add to Meta Repository */
        metaMutex.lock();
        {
            /* Remove Oldest Entry if Repository is Full */
            if(metaRepo.isfull())
            {
                metaRepo.remove(metaRepo.first(NULL));
            }

            /* Add Entry to Repository */
            metaRepo.add(meta_key, metaData, false);
        }
        metaMutex.unlock();
    }
    catch(const RunTimeException& e)
    {
        /* Clean Up Data Allocations */
        delete [] info->data;
        info->data= NULL;
        info->datasize = 0;

        /* Clean Up Class Allocations */
        tearDown();

        /* Rethrow Error */
        throw RunTimeException(CRITICAL, RTE_ERROR, "%s (%s)", e.what(), dataset);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::~H5FileBuffer (void)
{
    tearDown();
}

/*----------------------------------------------------------------------------
 * tearDown
 *----------------------------------------------------------------------------*/
void H5FileBuffer::tearDown (void)
{
    /* Close I/O Resources */
    delete ioDriver;

    /* Delete Local Context */
    if(ioContextLocal)
    {
        delete ioContext;
    }

    /* Delete Dataset Strings */
    delete [] datasetName;
    delete [] datasetPrint;

    /* Delete Chunk Buffer */
    delete [] dataChunkBuffer;
    delete [] dataChunkFilterBuffer;
}

/*----------------------------------------------------------------------------
 * ioRequest
 *----------------------------------------------------------------------------*/
void H5FileBuffer::ioRequest (uint64_t* pos, int64_t size, uint8_t* buffer, int64_t hint, bool cache_the_data)
{
    cache_entry_t entry;
    int64_t data_offset = 0;
    const uint64_t file_position = *pos;
    bool cached = false;
    ioContext->mut.lock();
    {
        /* Count I/O Request */
        if(ioPostPrefetch) ioContext->post_prefetch_request++;
        else ioContext->pre_prefetch_request++;

        /* Attempt to fulfill data request from I/O cache
        *  note that this is only checked if a buffer is supplied;
        *  otherwise the purpose of the call is to cache the entry */
        if(buffer)
        {
            if( ioCheckCache(file_position, size, &ioContext->l1, IO_CACHE_L1_MASK, &entry) ||
                ioCheckCache(file_position, size, &ioContext->l2, IO_CACHE_L2_MASK, &entry) )
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
                ioContext->cache_miss++;
            }
        }
    }
    ioContext->mut.unlock();

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
                cache = &ioContext->l1;
                cache_replace = &ioContext->l1_cache_replace;
            }
            else
            {
                cache = &ioContext->l2;
                cache_replace = &ioContext->l2_cache_replace;
            }

            /* Cache Entry */
            ioContext->mut.lock();
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
                        ioContext->mut.unlock();
                        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to make room in cache for %s", datasetPrint);
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
                ioContext->bytes_read += entry.size;
            }
            ioContext->mut.unlock();
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
            ioContext->mut.lock();
            {
                ioContext->bytes_read += entry.size;
            }
            ioContext->mut.unlock();
        }
    }

    /* Update Position */
    *pos += size;
}

/*----------------------------------------------------------------------------
 * ioCheckCache
 *----------------------------------------------------------------------------*/
bool H5FileBuffer::ioCheckCache (uint64_t pos, int64_t size, cache_t* cache, uint64_t line_mask, cache_entry_t* entry)
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
uint64_t H5FileBuffer::ioHashL1 (uint64_t key)
{
    return key & (~IO_CACHE_L1_MASK);
}

/*----------------------------------------------------------------------------
 * ioHashL2
 *----------------------------------------------------------------------------*/
uint64_t H5FileBuffer::ioHashL2 (uint64_t key)
{
    return key & (~IO_CACHE_L2_MASK);
}

/*----------------------------------------------------------------------------
 * readField
 *----------------------------------------------------------------------------*/
void H5FileBuffer::readByteArray (uint8_t* data, int64_t size, uint64_t* pos)
{
    assert(data);
    ioRequest(pos, size, data, IO_CACHE_L1_LINESIZE, true);
}

/*----------------------------------------------------------------------------
 * readField
 *----------------------------------------------------------------------------*/
uint64_t H5FileBuffer::readField (int64_t size, uint64_t* pos)
{
    assert(pos);
    assert(size > 0);
    assert(size <= 8);

    uint64_t value = 0;
    uint8_t data_ptr[8];

    /* Request Data from I/O */
    ioRequest(pos, size, data_ptr, IO_CACHE_L1_LINESIZE, true);

    /*  Read Field Value */
    switch(size)
    {
        case 8:
        {
            value = *reinterpret_cast<uint64_t*>(data_ptr);
            #ifdef __be__
                value = OsApi::swapll(value);
            #endif
            break;
        }

        case 4:
        {
            value = *reinterpret_cast<uint32_t*>(data_ptr);
            #ifdef __be__
                value = OsApi::swapl(value);
            #endif
            break;
        }

        case 2:
        {
            value = *reinterpret_cast<uint16_t*>(data_ptr);
            #ifdef __be__
                value = OsApi::swaps(value);
            #endif
            break;
        }

        case 1:
        {
            value = *reinterpret_cast<uint8_t*>(data_ptr);
            break;
        }

        default: assert(false);
    }

    /* Return Field Value */
    return value;
}

/*----------------------------------------------------------------------------
 * readDataset
 *----------------------------------------------------------------------------*/
void H5FileBuffer::readDataset (info_t* info)
{
    /* Populate Type Size */
    info->typesize = metaData.typesize;

    /* Sanity Check Data Attributes */
    if(metaData.typesize <= 0)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "missing data type information");
    }
    if(metaData.ndims < 0)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "missing data dimension information");
    }

    /* Calculate Size of Data Row (note dimension starts at 1) */
    uint64_t row_size = metaData.typesize;
    for(int d = 1; d < metaData.ndims; d++)
    {
        row_size *= metaData.dimensions[d];
    }

    /* Get Number of Rows */
    const uint64_t first_dimension = (metaData.ndims > 0) ? metaData.dimensions[0] : 1;
    datasetNumRows = (datasetNumRows == ALL_ROWS) ? first_dimension : datasetNumRows;
    if((datasetStartRow + datasetNumRows) > first_dimension)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "read exceeds number of rows: %d + %d > %d", (int)datasetStartRow, datasetNumRows, (int)first_dimension);
    }

    /* Allocate Data Buffer */
    uint8_t* buffer = NULL;
    const int64_t buffer_size = row_size * datasetNumRows;
    if(!metaOnly && buffer_size > 0)
    {
        /* Add Space for Terminator for Strings */
        const int64_t extra_space_for_terminator = (metaData.type == STRING_TYPE);

        /* Allocate */
        buffer = new uint8_t [buffer_size + extra_space_for_terminator];

        /* Gaurantee Termination of String */
        if(metaData.type == STRING_TYPE)
        {
            buffer[buffer_size] = '\0'; 
        }

        /* Fill Buffer with Fill Value (if provided) */
        #if 0
        if(metaData.fillsize > 0)
        {
            for(int64_t i = 0; i < buffer_size; i += metaData.fillsize)
            {
                memcpy(&buffer[i], &metaData.fill.fill_ll, metaData.fillsize);
            }
        }
        #endif
    }

    /* Populate Rest of Info Struct */
    info->elements = buffer_size / metaData.typesize;
    info->datasize = buffer_size;
    info->data     = buffer;
    info->numrows  = datasetNumRows;

    if      (metaData.ndims == 0)   info->numcols = 0;
    else if (metaData.ndims == 1)   info->numcols = 1;
    else if (metaData.ndims >= 2)   info->numcols = metaData.dimensions[1];

    if(metaData.type == FIXED_POINT_TYPE)
    {
        if(metaData.signedval)
        {
            if      (metaData.typesize == 1) info->datatype = RecordObject::INT8;
            else if (metaData.typesize == 2) info->datatype = RecordObject::INT16;
            else if (metaData.typesize == 4) info->datatype = RecordObject::INT32;
            else if (metaData.typesize == 8) info->datatype = RecordObject::INT64;
            else throw RunTimeException(CRITICAL, RTE_ERROR, "invalid type size for signed integer: %d", metaData.typesize);
        }
        else
        {
            if      (metaData.typesize == 1) info->datatype = RecordObject::UINT8;
            else if (metaData.typesize == 2) info->datatype = RecordObject::UINT16;
            else if (metaData.typesize == 4) info->datatype = RecordObject::UINT32;
            else if (metaData.typesize == 8) info->datatype = RecordObject::UINT64;
            else throw RunTimeException(CRITICAL, RTE_ERROR, "invalid type size for unsigned integer: %d", metaData.typesize);
        }
    }
    else if(metaData.type == FLOATING_POINT_TYPE)
    {
        if      (metaData.typesize == 4) info->datatype = RecordObject::FLOAT;
        else if (metaData.typesize == 8) info->datatype = RecordObject::DOUBLE;
        else throw RunTimeException(CRITICAL, RTE_ERROR, "invalid type size for floating point number: %d", metaData.typesize);
    }
    else if(metaData.type == STRING_TYPE)
    {
        info->datatype = RecordObject::STRING;
    }

    /* Calculate Buffer Start */
    const uint64_t buffer_offset = row_size * datasetStartRow;

    /* Check if Data Address and Data Size is Valid */
    if(H5_ERROR_CHECKING)
    {
        if(H5_INVALID(metaData.address))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "data not allocated in contiguous layout");
        }
        if(metaData.size != 0 && metaData.size < ((int64_t)buffer_offset + buffer_size))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "read exceeds available data: %ld != %ld", (long)metaData.size, (long)buffer_size);
        }
        if((metaData.filter[DEFLATE_FILTER] || metaData.filter[SHUFFLE_FILTER]) && ((metaData.layout == COMPACT_LAYOUT) || (metaData.layout == CONTIGUOUS_LAYOUT)))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "filters unsupported on non-chunked layouts");
        }
    }

    /* Read Dataset */
    if(!metaOnly && buffer_size > 0)
    {
        switch(metaData.layout)
        {
            case COMPACT_LAYOUT:
            case CONTIGUOUS_LAYOUT:
            {
                uint64_t data_addr = metaData.address + buffer_offset;
                ioRequest(&data_addr, buffer_size, buffer, 0, false);
                break;
            }

            case CHUNKED_LAYOUT:
            {
                /* Chunk Layout Specific Error Checks */
                if(H5_ERROR_CHECKING)
                {
                    if(metaData.elementsize != metaData.typesize)
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "chunk element size does not match data element size: %d != %d", metaData.elementsize, metaData.typesize);
                    }
                    if(metaData.chunkelements == 0)
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid number of chunk elements: %ld", (long)metaData.chunkelements);
                    }
                }

                /* Allocate Data Chunk Buffer */
                dataChunkBufferSize = metaData.chunkelements * metaData.typesize;
                dataChunkBuffer = new uint8_t [dataChunkBufferSize];
                dataChunkFilterBuffer = new uint8_t [dataChunkBufferSize * FILTER_SIZE_SCALE];

                /*
                 * Prefetch and Set Data Size Hint
                 *  If reading all of the data from the start of the data segment in the file
                 *  past where the desired subset is consistutes only a 2x increase in the
                 *  overall data that would be read, then prefetch the entire block from the
                 *  beginning and set the size hint to the L1 cache line size.
                 */
                ioPostPrefetch = true;
                if(buffer_offset < (uint64_t)buffer_size)
                {
                    ioRequest(&metaData.address, 0, NULL, buffer_offset + buffer_size, true);
                    dataSizeHint = IO_CACHE_L1_LINESIZE;
                }
                else
                {
                    dataSizeHint = buffer_size;
                }

                /* Read B-Tree */
                readBTreeV1(metaData.address, buffer, buffer_size, buffer_offset);

                /* Check Need to Flatten Chunks */
                bool flatten = false;
                for(int d = 1; d < metaData.ndims; d++)
                {
                    if(metaData.chunkdims[d] != metaData.dimensions[d])
                    {
                        flatten = true;
                        break;
                    }
                }

                /* Flatten Chunks - Place Dataset in Row Order*/
                if(flatten)
                {
                    /* New Flattened Buffer */
                    uint8_t* fbuf = new uint8_t[buffer_size];
                    uint64_t bi = 0; // index into source buffer

                    /* Build Number of Each Chunk per Dimension */
                    uint64_t cdimnum[MAX_NDIMS * 2] = {0};
                    for(int i = 0; i < metaData.ndims; i++)
                    {
                        cdimnum[i] = metaData.dimensions[i] / metaData.chunkdims[i];
                        cdimnum[i + metaData.ndims] = metaData.chunkdims[i];
                    }

                    /* Build Size of Each Chunk per Flattened Dimension */
                    uint64_t cdimsizes[FLAT_NDIMS];
                    cdimsizes[0] = metaData.chunkdims[0] * metaData.typesize; // number of chunk rows
                    for(int i = 1; i < metaData.ndims; i++)
                    {
                        cdimsizes[0] *= cdimnum[i]; // number of columns of chunks
                        cdimsizes[0] *= metaData.chunkdims[i]; // number of columns in chunks
                    }
                    cdimsizes[1] = metaData.typesize;
                    for(int i = 1; i < metaData.ndims; i++)
                    {
                        cdimsizes[1] *= metaData.chunkdims[i]; // number of columns in chunks
                    }
                    cdimsizes[2] = metaData.typesize;
                    for(int i = 1; i < metaData.ndims; i++)
                    {
                        cdimsizes[2] *= cdimnum[i]; // number of columns of chunks
                        cdimsizes[2] *= metaData.chunkdims[i]; // number of columns in chunks
                    }

                    /* Initialize Loop Variables */
                    int ci = FLAT_NDIMS - 1; // chunk dimension index
                    uint64_t dimi[MAX_NDIMS * 2]; // chunk dimension indices
                    memset(dimi, 0, sizeof(dimi));

                    /* Loop Through Each Chunk */
                    while(true)
                    {
                        /* Calculate Start Position */
                        uint64_t start = 0;
                        for(int i = 0; i < FLAT_NDIMS; i++)
                        {
                            start += dimi[i] * cdimsizes[i];
                        }

                        /* Copy Into New Buffer */
                        for(uint64_t k = 0; k < cdimsizes[1]; k++)
                        {
                            fbuf[start + k] = buffer[bi++];   // NOLINT(clang-analyzer-core.uninitialized.Assign)
                        }

                        /* Update Indices */
                        dimi[ci]++;
                        while(dimi[ci] == cdimnum[ci])
                        {
                            dimi[ci--] = 0;
                            if(ci < 0) break;
                            dimi[ci]++;
                        }

                        /* Check Exit Condition */
                        if(ci < 0) break;
                        ci = FLAT_NDIMS - 1;
                    }

                    /* Replace Buffer */
                    delete [] buffer;
                    info->data = fbuf;
                }

                break;
            }

            default:
            {
                if(H5_ERROR_CHECKING)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "invalid data layout: %d", (int)metaData.layout);
                }
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * readSuperblock
 *----------------------------------------------------------------------------*/
uint64_t H5FileBuffer::readSuperblock (void)
{
    uint64_t pos = 0;
    uint64_t root_group_offset = 0;

    /* Signature and Version */
    const uint64_t signature = readField(8, &pos);
    if(signature != H5_SIGNATURE_LE)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid h5 file signature: 0x%llX", (unsigned long long)signature);
    }

    const uint64_t superblock_version = readField(1, &pos);
    if((superblock_version != 0) && (superblock_version != 2))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported h5 file superblock version: %d", (int)superblock_version);
    }

    /* Super Block Version 0 */
    if(superblock_version == 0)
    {
        /* Read and Verify Superblock Info */
        if(H5_ERROR_CHECKING)
        {
            const uint64_t freespace_version = readField(1, &pos);
            if(freespace_version != 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported h5 file free space version: %d", (int)freespace_version);
            }

            const uint64_t roottable_version = readField(1, &pos);
            if(roottable_version != 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported h5 file root table version: %d", (int)roottable_version);
            }
        }

        /* Read Sizes */
        pos = 13;
        metaData.offsetsize = readField(1, &pos);
        metaData.lengthsize = readField(1, &pos);

        /* Read Base Address */
        if(H5_ERROR_CHECKING)
        {
            pos = 24;
            const uint64_t base_address = readField(metaData.offsetsize, &pos);
            if(base_address != 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported h5 file base address: %lu", (unsigned long)base_address);
            }
        }

        /* Read Group Offset */
        pos = 24 + (5 * metaData.offsetsize);
        root_group_offset = readField(metaData.offsetsize, &pos);

        if(H5_VERBOSE)
        {
            print2term("\n----------------\n");
            print2term("File Information\n");
            print2term("----------------\n");
            print2term("Size of Offsets:                                                 %lu\n",     (unsigned long)metaData.offsetsize);
            print2term("Size of Lengths:                                                 %lu\n",     (unsigned long)metaData.lengthsize);
            print2term("Root Object Header Address:                                      0x%lX\n",   (long unsigned)root_group_offset);
        }
    }
    /* Super Block Version 2 */
    else // if(superblock_version == 2)
    {
        /* Read Sizes */
        pos = 9;
        metaData.offsetsize = readField(1, &pos);
        metaData.lengthsize = readField(1, &pos);

        /* Read Base Address */
        if(H5_ERROR_CHECKING)
        {
            pos = 12;
            const uint64_t base_address = readField(8, &pos);
            if(base_address != 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported h5 file base address: %lu", (unsigned long)base_address);
            }
        }

        /* Read Group Offset */
        pos = 12 + (3 * metaData.offsetsize);
        root_group_offset = readField(metaData.offsetsize, &pos);

        if(H5_VERBOSE)
        {
            print2term("\n----------------\n");
            print2term("File Information\n");
            print2term("----------------\n");
            print2term("Size of Offsets:                                                 %lu\n",     (unsigned long)metaData.offsetsize);
            print2term("Size of Lengths:                                                 %lu\n",     (unsigned long)metaData.lengthsize);
            print2term("Root Object Header Address:                                      0x%lX\n",   (long unsigned)root_group_offset);
        }
    }

    /* Return Root Group Offset */
    return root_group_offset;
}

/*----------------------------------------------------------------------------
 * readFractalHeap
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readFractalHeap (msg_type_t msg_type, uint64_t pos, uint8_t hdr_flags, int dlvl, heap_info_t* heap_info_ptr)
{
    static const int FRHP_CHECKSUM_DIRECT_BLOCKS = 0x02;

    const uint64_t starting_position = pos;

    if(!H5_ERROR_CHECKING)
    {
        pos += 5;
    }
    else
    {
        const uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_FRHP_SIGNATURE_LE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid heap signature: 0x%llX", (unsigned long long)signature);
        }

        const uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid heap version: %d", (int)version);
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Fractal Heap [%d]: %d, 0x%lx\n", dlvl, (int)msg_type, starting_position);
        print2term("----------------\n");
    }

    /*  Read Fractal Heap Header */
    const uint16_t    heap_obj_id_len     = (uint16_t)readField(2, &pos); // Heap ID Length
    const uint16_t    io_filter_len       = (uint16_t)readField(2, &pos); // I/O Filters' Encoded Length
    const uint8_t     flags               =  (uint8_t)readField(1, &pos); // Flags
    const uint32_t    max_size_mg_obj     = (uint32_t)readField(4, &pos); // Maximum Size of Managed Objects
    const uint64_t    next_huge_obj_id    = readField(metaData.lengthsize, &pos); // Next Huge Object ID
    const uint64_t    btree_addr_huge_obj = readField(metaData.offsetsize, &pos); // v2 B-tree Address of Huge Objects
    const uint64_t    free_space_mg_blks  = readField(metaData.lengthsize, &pos); // Amount of Free Space in Managed Blocks
    const uint64_t    addr_free_space_mg  = readField(metaData.offsetsize, &pos); // Address of Managed Block Free Space Manager
    const uint64_t    mg_space            = readField(metaData.lengthsize, &pos); // Amount of Manged Space in Heap
    const uint64_t    alloc_mg_space      = readField(metaData.lengthsize, &pos); // Amount of Allocated Managed Space in Heap
    const uint64_t    dblk_alloc_iter     = readField(metaData.lengthsize, &pos); // Offset of Direct Block Allocation Iterator in Managed Space
    const uint64_t    mg_objs             = readField(metaData.lengthsize, &pos); // Number of Managed Objects in Heap
    const uint64_t    huge_obj_size       = readField(metaData.lengthsize, &pos); // Size of Huge Objects in Heap
    const uint64_t    huge_objs           = readField(metaData.lengthsize, &pos); // Number of Huge Objects in Heap
    const uint64_t    tiny_obj_size       = readField(metaData.lengthsize, &pos); // Size of Tiny Objects in Heap
    const uint64_t    tiny_objs           = readField(metaData.lengthsize, &pos); // Number of Tiny Objects in Heap
    const uint16_t    table_width         = readField(2, &pos); // Table Width
    const uint64_t    starting_blk_size   = readField(metaData.lengthsize, &pos); // Starting Block Size
    const uint64_t    max_dblk_size       = readField(metaData.lengthsize, &pos); // Maximum Direct Block Size
    const uint16_t    max_heap_size       = readField(2, &pos); // Maximum Heap Size
    const uint16_t    start_num_rows      = readField(2, &pos); // Starting # of Rows in Root Indirect Block
    const uint64_t    root_blk_addr       = readField(metaData.offsetsize, &pos); // Address of Root Block
    const uint16_t    curr_num_rows       = (uint16_t)readField(2, &pos); // Current # of Rows in Root Indirect Block
    if(H5_VERBOSE)
    {
        print2term("Heap ID Length:                                                  %lu\n", (unsigned long)heap_obj_id_len);
        print2term("I/O Filters' Encoded Length:                                     %lu\n", (unsigned long)io_filter_len);
        print2term("Flags:                                                           0x%lx\n", (unsigned long)flags);
        print2term("Maximum Size of Managed Objects:                                 %lu\n", (unsigned long)max_size_mg_obj);
        print2term("Next Huge Object ID:                                             %lu\n", (unsigned long)next_huge_obj_id);
        print2term("v2 B-tree Address of Huge Objects:                               0x%lx\n", (unsigned long)btree_addr_huge_obj);
        print2term("Amount of Free Space in Managed Blocks:                          %lu\n", (unsigned long)free_space_mg_blks);
        print2term("Address of Managed Block Free Space Manager:                     0x%lx\n", (unsigned long)addr_free_space_mg);
        print2term("Amount of Manged Space in Heap:                                  %lu\n", (unsigned long)mg_space);
        print2term("Amount of Allocated Managed Space in Heap:                       %lu\n", (unsigned long)alloc_mg_space);
        print2term("Offset of Direct Block Allocation Iterator in Managed Space:     %lu\n", (unsigned long)dblk_alloc_iter);
        print2term("Number of Managed Objects in Heap:                               %lu\n", (unsigned long)mg_objs);
        print2term("Size of Huge Objects in Heap:                                    %lu\n", (unsigned long)huge_obj_size);
        print2term("Number of Huge Objects in Heap:                                  %lu\n", (unsigned long)huge_objs);
        print2term("Size of Tiny Objects in Heap:                                    %lu\n", (unsigned long)tiny_obj_size);
        print2term("Number of Tiny Objects in Heap:                                  %lu\n", (unsigned long)tiny_objs);
        print2term("Table Width:                                                     %lu\n", (unsigned long)table_width);
        print2term("Starting Block Size:                                             %lu\n", (unsigned long)starting_blk_size);
        print2term("Maximum Direct Block Size:                                       %lu\n", (unsigned long)max_dblk_size);
        print2term("Maximum Heap Size:                                               %lu\n", (unsigned long)max_heap_size);
        print2term("Starting # of Rows in Root Indirect Block:                       %lu\n", (unsigned long)start_num_rows);
        print2term("Address of Root Block:                                           0x%lx\n", (unsigned long)root_blk_addr);
        print2term("Current # of Rows in Root Indirect Block:                        %lu\n", (unsigned long)curr_num_rows);
    }
    else
    {
        (void)heap_obj_id_len;
        (void)max_size_mg_obj;
        (void)next_huge_obj_id;
        (void)btree_addr_huge_obj;
        (void)free_space_mg_blks;
        (void)addr_free_space_mg;
        (void)mg_space;
        (void)alloc_mg_space;
        (void)dblk_alloc_iter;
        (void)huge_obj_size;
        (void)huge_objs;
        (void)tiny_obj_size;
        (void)tiny_objs;
        (void)start_num_rows;
    }

    /* Read Filter Information */
    if(io_filter_len > 0)
    {
        const uint64_t filter_root_dblk   = readField(metaData.lengthsize, &pos); // Size of Filtered Root Direct Block
        const uint32_t filter_mask        = (uint32_t)readField(4, &pos); // I/O Filter Mask
        print2term("Size of Filtered Root Direct Block:                              %lu\n", (unsigned long)filter_root_dblk);
        print2term("I/O Filter Mask:                                                 %lu\n", (unsigned long)filter_mask);

        throw RunTimeException(CRITICAL, RTE_ERROR, "Filtering unsupported on fractal heap: %d", io_filter_len);
        // readMessage(FILTER_MSG, io_filter_len, pos, hdr_flags, dlvl); // this currently populates filter for dataset
    }

    /* Check Checksum */
    const uint64_t check_sum = readField(4, &pos);
    (void)check_sum; // unused


    /* for heap len size - follow https://github.com/HDFGroup/hdf5/blob/f73da83a94f6fe563ff351603aa4d34525ef612b/src/H5HFhdr.c#L199 */
    const uint8_t min_calc = std::min((uint32_t)max_dblk_size, (uint32_t)((H5BTreeV2::log2Gen((uint64_t) max_size_mg_obj) / 8) + 1));

    /* Build Heap Info Structure */
    heap_info_ptr->table_width        = table_width;
    heap_info_ptr->curr_num_rows      = curr_num_rows;
    heap_info_ptr->starting_blk_size  = (int)starting_blk_size;
    heap_info_ptr->max_dblk_size      = (int)max_dblk_size;
    heap_info_ptr->blk_offset_size    = ((max_heap_size + 7) / 8);
    heap_info_ptr->dblk_checksum      = ((flags & FRHP_CHECKSUM_DIRECT_BLOCKS) != 0);
    heap_info_ptr->msg_type           = msg_type;
    heap_info_ptr->num_objects        = (int)mg_objs;
    heap_info_ptr->cur_objects        = 0; // updated as objects are read
    heap_info_ptr->root_blk_addr      = root_blk_addr;
    heap_info_ptr->max_size_mg_obj    = max_size_mg_obj;
    heap_info_ptr->max_heap_size      = max_heap_size;
    heap_info_ptr->hdr_flags          = hdr_flags;
    heap_info_ptr->heap_off_size      = (uint8_t) H5BTreeV2::sizeOffsetBits(max_heap_size);
    heap_info_ptr->heap_len_size      = min_calc;
    heap_info_ptr->dlvl               = dlvl;

    /* Process Blocks */
    if(heap_info_ptr->curr_num_rows == 0)
    {
        /* Direct Blocks */
        const int bytes_read = readDirectBlock(heap_info_ptr, heap_info_ptr->starting_blk_size, root_blk_addr, hdr_flags, dlvl);
        if(H5_ERROR_CHECKING && (bytes_read > heap_info_ptr->starting_blk_size))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "direct block contianed more bytes than specified: %d > %d", bytes_read, heap_info_ptr->starting_blk_size);
        }
        pos += heap_info_ptr->starting_blk_size;
    }
    else
    {
        /* Indirect Blocks */
        const int bytes_read = readIndirectBlock(heap_info_ptr, 0, root_blk_addr, hdr_flags, dlvl);
        if(H5_ERROR_CHECKING && (bytes_read > heap_info_ptr->starting_blk_size))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "indirect block contianed more bytes than specified: %d > %d", bytes_read, heap_info_ptr->starting_blk_size);
        }
        pos += bytes_read;
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readDirectBlock
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readDirectBlock (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    const uint64_t starting_position = pos;

    if(!H5_ERROR_CHECKING)
    {
        pos += 5;
    }
    else
    {
        const uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_FHDB_SIGNATURE_LE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid direct block signature: 0x%llX", (unsigned long long)signature);
        }

        const uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid direct block version: %d", (int)version);
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Direct Block [%d,%d,%d]: 0x%lx\n", dlvl, (int)heap_info->msg_type, block_size, (unsigned long)starting_position);
        print2term("----------------\n");
    }

    /* Read Block Header */
    if(!H5_VERBOSE)
    {
        pos += metaData.offsetsize + heap_info->blk_offset_size;
    }
    else
    {
        const uint64_t heap_hdr_addr = readField(metaData.offsetsize, &pos); // Heap Header Address
        const int MAX_BLOCK_OFFSET_SIZE = 8;
        uint8_t block_offset_buf[MAX_BLOCK_OFFSET_SIZE];
        if(H5_ERROR_CHECKING && (heap_info->blk_offset_size > MAX_BLOCK_OFFSET_SIZE))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "block offset size too large: %d", heap_info->blk_offset_size);
        }
        readByteArray(block_offset_buf, heap_info->blk_offset_size, &pos); // Block Offset
        print2term("Heap Header Address:                                             0x%lx\n", heap_hdr_addr);
        print2term("Block Offset:                                                    0x");
        for(int i = 0; i < heap_info->blk_offset_size; i++) print2term("%02X", block_offset_buf[i]);
    }

    if(heap_info->dblk_checksum)
    {
        const uint64_t check_sum = readField(4, &pos);
        (void)check_sum; // unused
    }

    /* Read Block Data */
    int data_left = block_size - (5 + metaData.offsetsize + heap_info->blk_offset_size + ((int)heap_info->dblk_checksum * 4));
    while(data_left > 0)
    {
        /* Peak if More Messages */
        uint64_t peak_addr = pos;
        const int peak_size = MIN((1 << highestBit(data_left)), 8);
        if(readField(peak_size, &peak_addr) == 0)
        {
            if(H5_VERBOSE)
            {
                print2term("\nExiting direct block 0x%lx early at 0x%lx\n", starting_position, pos);
            }
            break;
        }

        /* Read Message */
        const uint64_t data_read = readMessage(heap_info->msg_type, data_left, pos, hdr_flags, dlvl);
        pos += data_read;
        data_left -= data_read;

        /* Update Number of Objects Read
         *  There are often more links in a heap than managed objects
         *  therefore, the number of objects cannot be used to know when
         *  to stop reading links.
         */
        heap_info->cur_objects++;

        /* Check Reading Past Block */
        if(H5_ERROR_CHECKING)
        {
            if(data_left < 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "reading message exceeded end of direct block: 0x%lx", starting_position);
            }
        }

        /* Check if Dataset Found */
        if(highestDataLevel > dlvl)
        {
            break; // dataset found
        }
    }

    /* Skip to End of Block */
    pos += data_left;

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readIndirectBlock
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readIndirectBlock (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    const uint64_t starting_position = pos;

    if(!H5_ERROR_CHECKING)
    {
        pos += 5;
    }
    else
    {
        const uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_FHIB_SIGNATURE_LE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid direct block signature: 0x%llX", (unsigned long long)signature);
        }

        const uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid direct block version: %d", (int)version);
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Indirect Block [%d,%d]: 0x%lx\n", dlvl, (int)heap_info->msg_type, (unsigned long)starting_position);
        print2term("----------------\n");
    }

    /* Read Block Header */
    if(!H5_VERBOSE)
    {
        pos += metaData.offsetsize + heap_info->blk_offset_size;
    }
    else
    {
        const uint64_t heap_hdr_addr = readField(metaData.offsetsize, &pos); // Heap Header Address
        const int MAX_BLOCK_OFFSET_SIZE = 8;
        uint8_t block_offset_buf[MAX_BLOCK_OFFSET_SIZE];
        if(H5_ERROR_CHECKING && (heap_info->blk_offset_size > MAX_BLOCK_OFFSET_SIZE))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "block offset size too large: %d", heap_info->blk_offset_size);
        }
        readByteArray(block_offset_buf, heap_info->blk_offset_size, &pos); // Block Offset
        print2term("Heap Header Address:                                             0x%lx\n", heap_hdr_addr);
        print2term("Block Offset:                                                    0x");
        for(int i = 0; i < heap_info->blk_offset_size; i++) print2term("%02X", block_offset_buf[i]);
    }

    /* Calculate Number of Direct and Indirect Blocks (see III.G. Disk Format: Level 1G - Fractal Heap) */
    int nrows = heap_info->curr_num_rows; // used for "root" indirect block only
    const int curr_size = heap_info->starting_blk_size * heap_info->table_width;
    if(block_size > 0) nrows = (highestBit(block_size) - highestBit(curr_size)) + 1;
    const int max_dblock_rows = (highestBit(heap_info->max_dblk_size) - highestBit(heap_info->starting_blk_size)) + 2;
    const int K = MIN(nrows, max_dblock_rows) * heap_info->table_width;
    const int N = K - (max_dblock_rows * heap_info->table_width);
    if(H5_VERBOSE)
    {
        print2term("Number of Rows:                                                  %d\n", nrows);
        print2term("Maximum Direct Block Rows:                                       %d\n", max_dblock_rows);
        print2term("Number of Direct Blocks (K):                                     %d\n", K);
        print2term("Number of Indirect Blocks (N):                                   %d\n", N);
    }

    /* Read Direct Child Blocks */
    for(int row = 0; row < nrows; row++)
    {
        /* Calculate Row's Block Size */
        int row_block_size;
        if      (row == 0)  row_block_size = heap_info->starting_blk_size;
        else if (row == 1)  row_block_size = heap_info->starting_blk_size;
        else                row_block_size = heap_info->starting_blk_size * (0x2 << (row - 2));

        /* Process Entries in Row */
        for(int entry = 0; entry < heap_info->table_width; entry++)
        {
            /* Direct Block Entry */
            if(row_block_size <= heap_info->max_dblk_size)
            {
                if(H5_ERROR_CHECKING)
                {
                    if(row >= K)
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "unexpected direct block row: %d, %d >= %d\n", row_block_size, row, K);
                    }
                }

                /* Read Direct Block Address */
                const uint64_t direct_block_addr = readField(metaData.offsetsize, &pos);
                // note: filters are unsupported, but if present would be read here
                if(!H5_INVALID(direct_block_addr) && (dlvl >= highestDataLevel))
                {
                    /* Read Direct Block */
                    const int bytes_read = readDirectBlock(heap_info, row_block_size, direct_block_addr, hdr_flags, dlvl);
                    if(H5_ERROR_CHECKING && (bytes_read > row_block_size))
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "direct block contained more bytes than specified: %d > %d", bytes_read, row_block_size);
                    }
                }
            }
            else /* Indirect Block Entry */
            {
                if(H5_ERROR_CHECKING)
                {
                    if(row < K || row >= N)
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "unexpected indirect block row: %d, %d, %d\n", row_block_size, row, N);
                    }
                }

                /* Read Indirect Block Address */
                const uint64_t indirect_block_addr = readField(metaData.offsetsize, &pos);
                if(!H5_INVALID(indirect_block_addr) && (dlvl >= highestDataLevel))
                {
                    /* Read Indirect Block */
                    const int bytes_read = readIndirectBlock(heap_info, row_block_size, indirect_block_addr, hdr_flags, dlvl);
                    if(H5_ERROR_CHECKING && (bytes_read > row_block_size))
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "indirect block contained more bytes than specified: %d > %d", bytes_read, row_block_size);
                    }
                }
            }
        }
    }

    /* Read Checksum */
    const uint64_t check_sum = readField(4, &pos);
    (void)check_sum; // unused

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readBTreeV1
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readBTreeV1 (uint64_t pos, uint8_t* buffer, uint64_t buffer_size, uint64_t buffer_offset)
{
    const uint64_t starting_position = pos;
    const uint64_t data_key1 = datasetStartRow;
    const uint64_t data_key2 = datasetStartRow + datasetNumRows - 1;

    /* Check Signature and Node Type */
    if(!H5_ERROR_CHECKING)
    {
        pos += 5;
    }
    else
    {
        const uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_TREE_SIGNATURE_LE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid b-tree signature: 0x%llX", (unsigned long long)signature);
        }

        const uint8_t node_type = (uint8_t)readField(1, &pos);
        if(node_type != 1)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "only raw data chunk b-trees supported: %d", node_type);
        }
    }

    /* Read Node Level and Number of Entries */
    const uint8_t node_level = (uint8_t)readField(1, &pos);
    const uint16_t entries_used = (uint16_t)readField(2, &pos);

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("B-Tree Node: 0x%lx\n", (unsigned long)starting_position);
        print2term("----------------\n");
        print2term("Node Level:                                                      %d\n", (int)node_level);
        print2term("Entries Used:                                                    %d\n", (int)entries_used);
    }

    /* Skip Sibling Addresses */
    pos += metaData.offsetsize * 2;

    /* Read First Key */
    btree_node_t curr_node = readBTreeNodeV1(metaData.ndims, &pos);

    /* Read Children */
    for(int e = 0; e < entries_used; e++)
    {
        /* Read Child Address */
        uint64_t child_addr = readField(metaData.offsetsize, &pos);

        /* Read Next Key */
        const btree_node_t next_node = readBTreeNodeV1(metaData.ndims, &pos);

        /*  Get Child Keys */
        const uint64_t child_key1 = curr_node.row_key;
        uint64_t child_key2 = next_node.row_key; // there is always +1 keys
        if(next_node.chunk_size == 0 && metaData.ndims > 0)
        {
            child_key2 = metaData.dimensions[0];
        }

        /* Display */
        if(H5_VERBOSE && H5_EXTRA_DEBUG)
        {
            print2term("\nEntry:                                                           %d[%d]\n", (int)node_level, e);
            print2term("Chunk Size:                                                      %u | %u\n", (unsigned int)curr_node.chunk_size, (unsigned int)next_node.chunk_size);
            print2term("Filter Mask:                                                     0x%x | 0x%x\n", (unsigned int)curr_node.filter_mask, (unsigned int)next_node.filter_mask);
            print2term("Chunk Key:                                                       %lu | %lu\n", (unsigned long)child_key1, (unsigned long)child_key2);
            print2term("Data Key:                                                        %lu | %lu\n", (unsigned long)data_key1, (unsigned long)data_key2);
            print2term("Slice:                                                           ");
            for(int s = 0; s < metaData.ndims; s++) print2term("%lu ", (unsigned long)curr_node.slice[s]);
            print2term("\n");
            print2term("Child Address:                                                   0x%lx\n", (unsigned long)child_addr);
        }

        /* Check Inclusion */
        if ((data_key1  >= child_key1 && data_key1  <  child_key2) ||
            (data_key2  >= child_key1 && data_key2  <  child_key2) ||
            (child_key1 >= data_key1  && child_key1 <= data_key2)  ||
            (child_key2 >  data_key1  && child_key2 <  data_key2))
        {
            /* Process Child Entry */
            if(node_level > 0)
            {
                readBTreeV1(child_addr, buffer, buffer_size, buffer_offset);
            }
            else
            {
                /* Calculate Chunk Location */
                uint64_t chunk_offset = 0;
                for(int i = 0; i < metaData.ndims; i++)
                {
                    uint64_t slice_size = curr_node.slice[i] * metaData.typesize;
                    for(int k = 0; k < i; k++)
                    {
                        slice_size *= metaData.chunkdims[k];
                    }
                    for(int j = i + 1; j < metaData.ndims; j++)
                    {
                        slice_size *= metaData.dimensions[j];
                    }
                    chunk_offset += slice_size;
                }

                /* Calculate Buffer Index - offset into data buffer to put chunked data */
                uint64_t buffer_index = 0;
                if(chunk_offset > buffer_offset)
                {
                    buffer_index = chunk_offset - buffer_offset;
                    if(buffer_index >= buffer_size)
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid location to read data: %ld, %lu", (unsigned long)chunk_offset, (unsigned long)buffer_offset);
                    }
                }

                /* Calculate Chunk Index - offset into chunk buffer to read from */
                uint64_t chunk_index = 0;
                if(buffer_offset > chunk_offset)
                {
                    chunk_index = buffer_offset - chunk_offset;
                    if((int64_t)chunk_index >= dataChunkBufferSize)
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid location to read chunk: %ld, %lu", (unsigned long)chunk_offset, (unsigned long)buffer_offset);
                    }
                }

                /* Calculate Chunk Bytes - number of bytes to read from chunk buffer */
                int64_t chunk_bytes = dataChunkBufferSize - chunk_index;
                if(chunk_bytes < 0)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "no bytes of chunk data to read: %ld, %lu", (long)chunk_bytes, (unsigned long)chunk_index);
                }
                if((buffer_index + chunk_bytes) > buffer_size)
                {
                    chunk_bytes = buffer_size - buffer_index;
                }

                /* Display Info */
                if(H5_VERBOSE && H5_EXTRA_DEBUG)
                {
                    print2term("Chunk Offset:                                                    %lu (%lu)\n", (unsigned long)chunk_offset, (unsigned long)(chunk_offset/metaData.typesize));
                    print2term("Buffer Index:                                                    %lu (%lu)\n", (unsigned long)buffer_index, (unsigned long)(buffer_index/metaData.typesize));
                    print2term("Chunk Bytes:                                                     %lu (%lu)\n", (unsigned long)chunk_bytes, (unsigned long)(chunk_bytes/metaData.typesize));
                }

                /* Read Chunk */
                if(metaData.filter[DEFLATE_FILTER])
                {
                    /* Check Current Node Chunk Size */
                    if(curr_node.chunk_size > (dataChunkBufferSize * FILTER_SIZE_SCALE))
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "Compressed chunk size exceeds buffer: %u > %lu", curr_node.chunk_size, (unsigned long)dataChunkBufferSize);
                    }

                    /* Read Data into Chunk Filter Buffer (holds the compressed data) */
                    ioRequest(&child_addr, curr_node.chunk_size, dataChunkFilterBuffer, dataSizeHint, true);
                    if((chunk_bytes == dataChunkBufferSize) && (!metaData.filter[SHUFFLE_FILTER]))
                    {
                        /* Inflate Directly into Data Buffer */
                        inflateChunk(dataChunkFilterBuffer, curr_node.chunk_size, &buffer[buffer_index], chunk_bytes);
                    }
                    else
                    {
                        /* Inflate into Data Chunk Buffer */
                        inflateChunk(dataChunkFilterBuffer, curr_node.chunk_size, dataChunkBuffer, dataChunkBufferSize);

                        if(metaData.filter[SHUFFLE_FILTER])
                        {
                            /* Shuffle Data Chunk Buffer into Data Buffer */
                            shuffleChunk(dataChunkBuffer, dataChunkBufferSize, &buffer[buffer_index], chunk_index, chunk_bytes, metaData.typesize);
                        }
                        else
                        {
                            /* Copy Data Chunk Buffer into Data Buffer */
                            memcpy(&buffer[buffer_index], &dataChunkBuffer[chunk_index], chunk_bytes);
                        }
                    }

                    /* Handle Caching */
                    dataSizeHint = IO_CACHE_L1_LINESIZE;
                }
                else /* no supported filters */
                {
                    if(H5_ERROR_CHECKING)
                    {
                        if(metaData.filter[SHUFFLE_FILTER])
                        {
                            throw RunTimeException(CRITICAL, RTE_ERROR, "shuffle filter unsupported on uncompressed chunk");
                        }
                        if(dataChunkBufferSize != curr_node.chunk_size)
                        {
                            throw RunTimeException(CRITICAL, RTE_ERROR, "mismatch in chunk size: %lu, %lu", (unsigned long)curr_node.chunk_size, (unsigned long)dataChunkBufferSize);
                        }
                    }

                    /* Read Data into Data Buffer */
                    uint64_t chunk_offset_addr = child_addr + chunk_index;
                    ioRequest(&chunk_offset_addr, chunk_bytes, &buffer[buffer_index], dataSizeHint, true);
                    dataSizeHint = IO_CACHE_L1_LINESIZE;
                }
            }
        }

        /* Goto Next Key */
        curr_node = next_node;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * readBTreeNodeV1
 *----------------------------------------------------------------------------*/
H5FileBuffer::btree_node_t H5FileBuffer::readBTreeNodeV1 (int ndims, uint64_t* pos)
{
    btree_node_t node;

    /* Read Key */
    node.chunk_size = (uint32_t)readField(4, pos);
    node.filter_mask = (uint32_t)readField(4, pos);
    for(int d = 0; d < ndims; d++)
    {
        node.slice[d] = readField(8, pos);
    }

    /* Read Trailing Zero */
    const uint64_t trailing_zero = readField(8, pos);
    if(H5_ERROR_CHECKING)
    {
        if(trailing_zero % metaData.typesize != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "key did not include a trailing zero: %lu", trailing_zero);
        }
        if(H5_VERBOSE && H5_EXTRA_DEBUG)
        {
            print2term("Trailing Zero:                                                   %d\n", (int)trailing_zero);
        }
    }

    /* Set Node Key */
    if(ndims > 0)   node.row_key = node.slice[0];
    else            node.row_key = 0;

    /* Return Copy of Node */
    return node;
}

/*----------------------------------------------------------------------------
 * readSymbolTable
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readSymbolTable (uint64_t pos, uint64_t heap_data_addr, int dlvl)
{
    const uint64_t starting_position = pos;

    /* Check Signature and Version */
    if(!H5_ERROR_CHECKING)
    {
        pos += 6;
    }
    else
    {
        const uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_SNOD_SIGNATURE_LE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid symbol table signature: 0x%llX", (unsigned long long)signature);
        }

        const uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 1)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "incorrect version of symbole table: %d", (int)version);
        }

        const uint8_t reserved0 = (uint8_t)readField(1, &pos);
        if(reserved0 != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "incorrect reserved value: %d", (int)reserved0);
        }
    }

    /* Read Symbols */
    const uint16_t num_symbols = (uint16_t)readField(2, &pos);
    for(int s = 0; s < num_symbols; s++)
    {
        /* Read Symbol Entry */
        const uint64_t link_name_offset = readField(metaData.offsetsize, &pos);
        const uint64_t obj_hdr_addr = readField(metaData.offsetsize, &pos);
        const uint32_t cache_type = (uint32_t)readField(4, &pos);
        pos += 20; // reserved + scratch pad

        /* Read Link Name */
        uint64_t link_name_addr = heap_data_addr + link_name_offset;
        uint8_t link_name[STR_BUFF_SIZE];
        int i = 0;
        while(true)
        {
            if(i >= STR_BUFF_SIZE)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "link name string exceeded maximum length: %d, 0x%lx\n", i, (unsigned long)pos);
            }

            const uint8_t c = (uint8_t)readField(1, &link_name_addr);
            link_name[i++] = c;

            if(c == 0)
            {
                break;
            }
        }
        link_name[i] = '\0';
        if(H5_VERBOSE)
        {
            print2term("Link Name:                                                       %s\n", link_name);
            print2term("Object Header Address:                                           0x%lx\n", obj_hdr_addr);
        }

        /* Process Link */
        if(dlvl < static_cast<int>(datasetPath.size()))
        {
            if(StringLib::match(reinterpret_cast<const char*>(link_name), datasetPath[dlvl]))
            {
                if(cache_type == 2)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "symbolic links are unsupported (%s)", link_name);
                }
                highestDataLevel = dlvl + 1;
                readObjHdr(obj_hdr_addr, highestDataLevel);
                break; // dataset found
            }
        }
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readObjHdr
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readObjHdr (uint64_t pos, int dlvl)
{
    const uint64_t starting_position = pos;

    /* Peek at Version / Process Version 1 */
    uint64_t peeking_position = pos;
    const uint8_t peek = readField(1, &peeking_position);
    if(peek == 1) return readObjHdrV1(starting_position, dlvl);

    /* Read Object Header */
    if(!H5_ERROR_CHECKING)
    {
        pos += 5; // move past signature and version
    }
    else
    {
        const uint64_t signature = readField(4, &pos);
        if(signature != H5_OHDR_SIGNATURE_LE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid header signature: 0x%llX", (unsigned long long)signature);
        }

        const uint64_t version = readField(1, &pos);
        if(version != 2)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid header version: %d", (int)version);
        }
    }

    /* Read Option Time Fields */
    const uint8_t obj_hdr_flags = (uint8_t)readField(1, &pos);
    if(obj_hdr_flags & FILE_STATS_BIT)
    {
        if(!H5_VERBOSE)
        {
            pos += 16; // move past time fields
        }
        else
        {
            const uint64_t access_time         = readField(4, &pos);
            const uint64_t modification_time   = readField(4, &pos);
            const uint64_t change_time         = readField(4, &pos);
            const uint64_t birth_time          = readField(4, &pos);

            print2term("\n----------------\n");
            print2term("Object Information [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
            print2term("----------------\n");

            const TimeLib::gmt_time_t access_gmt = TimeLib::sys2gmttime(access_time * 1000);
            print2term("Access Time:                                                     %d:%d:%d:%d:%d\n", access_gmt.year, access_gmt.doy, access_gmt.hour, access_gmt.minute, access_gmt.second);

            const TimeLib::gmt_time_t modification_gmt = TimeLib::sys2gmttime(modification_time * 1000);
            print2term("Modification Time:                                               %d:%d:%d:%d:%d\n", modification_gmt.year, modification_gmt.doy, modification_gmt.hour, modification_gmt.minute, modification_gmt.second);

            const TimeLib::gmt_time_t change_gmt = TimeLib::sys2gmttime(change_time * 1000);
            print2term("Change Time:                                                     %d:%d:%d:%d:%d\n", change_gmt.year, change_gmt.doy, change_gmt.hour, change_gmt.minute, change_gmt.second);

            const TimeLib::gmt_time_t birth_gmt = TimeLib::sys2gmttime(birth_time * 1000);
            print2term("Birth Time:                                                      %d:%d:%d:%d:%d\n", birth_gmt.year, birth_gmt.doy, birth_gmt.hour, birth_gmt.minute, birth_gmt.second);
        }
    }

    /* Optional Phase Attributes */
    if(obj_hdr_flags & STORE_CHANGE_PHASE_BIT)
    {
        if(!H5_VERBOSE)
        {
            pos += 4; // move past phase attributes
        }
        else
        {
            const uint64_t max_compact_attr = readField(2, &pos); (void)max_compact_attr;
            const uint64_t max_dense_attr = readField(2, &pos); (void)max_dense_attr;
        }
    }

    /* Read Header Messages */
    const uint64_t size_of_chunk0 = readField(1 << (obj_hdr_flags & SIZE_OF_CHUNK_0_MASK), &pos);
    const uint64_t end_of_hdr = pos + size_of_chunk0;
    pos += readMessages (pos, end_of_hdr, obj_hdr_flags, dlvl);

    /* Verify Checksum */
    const uint64_t check_sum = readField(4, &pos);
    (void)check_sum; // unused

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readMessages
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readMessages (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl)
{
    const uint64_t starting_position = pos;

    while(pos < end)
    {
        /* Read Message Info */
        const uint8_t     msg_type    = (uint8_t)readField(1, &pos);
        const uint16_t    msg_size    = (uint16_t)readField(2, &pos);
        const uint8_t     msg_flags   = (uint8_t)readField(1, &pos); (void)msg_flags;

        if(hdr_flags & ATTR_CREATION_TRACK_BIT)
        {
            const uint64_t msg_order = (uint8_t)readField(2, &pos); (void)msg_order;
        }

        /* Read Each Message */
        const int bytes_read = readMessage((msg_type_t)msg_type, msg_size, pos, hdr_flags, dlvl);
        if(H5_ERROR_CHECKING && (bytes_read != msg_size))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "header message different size than specified: %d != %d", bytes_read, msg_size);
        }

        /* Check if Dataset Found */
        if(highestDataLevel > dlvl)
        {
            pos = end; // go directly to end of header
            break; // dataset found
        }

        /* Update Position */
        pos += bytes_read;
    }

    /* Check Size */
    if(H5_ERROR_CHECKING)
    {
        if(pos != end)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "did not read correct number of bytes: %lu != %lu", (unsigned long)pos, (unsigned long)end);
        }
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readObjHdrV1
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readObjHdrV1 (uint64_t pos, int dlvl)
{
    const uint64_t starting_position = pos;

    /* Read Version */
    if(!H5_ERROR_CHECKING)
    {
        pos += 2;
    }
    else
    {
        const uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 1)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid header version: %d", (int)version);
        }

        const uint8_t reserved0 = (uint8_t)readField(1, &pos);
        if(reserved0 != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid reserved field: %d", (int)reserved0);
        }
    }

    /* Read Number of Header Messages */
    if(!H5_VERBOSE)
    {
        pos += 2;
    }
    else
    {
        print2term("\n----------------\n");
        print2term("Object Information V1 [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        print2term("----------------\n");

        const uint16_t num_hdr_msgs = (uint16_t)readField(2, &pos);
        print2term("Number of Header Messages:                                       %d\n", (int)num_hdr_msgs);
    }

    /* Read Object Reference Count */
    if(!H5_VERBOSE)
    {
        pos += 4;
    }
    else
    {
        const uint32_t obj_ref_count = (uint32_t)readField(4, &pos);
        print2term("Object Reference Count:                                          %d\n", (int)obj_ref_count);
    }

    /* Read Object Header Size */
    const uint64_t obj_hdr_size = readField(metaData.lengthsize, &pos);
    const uint64_t end_of_hdr = pos + obj_hdr_size;
    if(H5_VERBOSE)
    {
        print2term("Object Header Size:                                              %d\n", (int)obj_hdr_size);
        print2term("End of Header:                                                   0x%lx\n", (unsigned long)end_of_hdr);
    }

    /* Read Header Messages */
    pos += readMessagesV1(pos, end_of_hdr, H5CORO_CUSTOM_V1_FLAG, dlvl);

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readMessagesV1
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readMessagesV1 (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl)
{
    static const int SIZE_OF_V1_PREFIX = 8;

    const uint64_t starting_position = pos;

    while(pos < (end - SIZE_OF_V1_PREFIX))
    {
        const uint16_t    msg_type    = (uint16_t)readField(2, &pos);
        const uint16_t    msg_size    = (uint16_t)readField(2, &pos);
        const uint8_t     msg_flags   = (uint8_t)readField(1, &pos); (void)msg_flags;

        /* Reserved Bytes */
        if(!H5_ERROR_CHECKING)
        {
            pos += 3;
        }
        else
        {
            const uint8_t  reserved1 = (uint8_t)readField(1, &pos);
            const uint16_t reserved2 = (uint16_t)readField(2, &pos);
            if((reserved1 != 0) && (reserved2 != 0))
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid reserved fields: %d, %d", (int)reserved1, (int)reserved2);
            }
        }

        /* Read Each Message */
        int bytes_read = readMessage((msg_type_t)msg_type, msg_size, pos, hdr_flags, dlvl);

        /* Handle 8-byte Alignment of Messages */
        if((bytes_read % 8) > 0) bytes_read += 8 - (bytes_read % 8);
        if(H5_ERROR_CHECKING && (bytes_read != msg_size))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "message of type %d at position 0x%lx different size than specified: %d != %d", (int)msg_type, (unsigned long)pos, bytes_read, msg_size);
        }

        /* Check if Dataset Found */
        if(highestDataLevel > dlvl)
        {
            pos = end; // go directly to end of header
            break; // dataset found
        }

        /* Update Position */
        pos += bytes_read;
    }

    /* Move Past Gap */
    if(pos < end) pos = end;

    /* Check Size */
    if(H5_ERROR_CHECKING)
    {
        if(pos != end)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "did not read correct number of bytes: %lu != %lu", (unsigned long)pos, (unsigned long)end);
        }
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readMessage
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readMessage (msg_type_t msg_type, uint64_t size, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    switch(msg_type)
    {
        case DATASPACE_MSG:       return readDataspaceMsg(pos, hdr_flags, dlvl);
        case LINK_INFO_MSG:       return readLinkInfoMsg(pos, hdr_flags, dlvl);
        case DATATYPE_MSG:        return readDatatypeMsg(pos, hdr_flags, dlvl);
        case FILL_VALUE_MSG:      return readFillValueMsg(pos, hdr_flags, dlvl);
        case LINK_MSG:            return readLinkMsg(pos, hdr_flags, dlvl);
        case DATA_LAYOUT_MSG:     return readDataLayoutMsg(pos, hdr_flags, dlvl);
        case FILTER_MSG:          return readFilterMsg(pos, hdr_flags, dlvl);
#ifdef H5CORO_ATTRIBUTE_SUPPORT
        case ATTRIBUTE_MSG:       return readAttributeMsg(pos, hdr_flags, dlvl, size);
        case ATTRIBUTE_INFO_MSG:  return readAttributeInfoMsg(pos, hdr_flags, dlvl);
#endif
        case HEADER_CONT_MSG:     return readHeaderContMsg(pos, hdr_flags, dlvl);
        case SYMBOL_TABLE_MSG:    return readSymbolTableMsg(pos, hdr_flags, dlvl);

        default:
        {
            if(H5_VERBOSE)
            {
                print2term("Skipped Message [%d]: 0x%x, %d, 0x%lx\n", dlvl, (int)msg_type, (int)size, (unsigned long)pos);
            }

            return size;
        }
    }
}

/*----------------------------------------------------------------------------
 * readDataspaceMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readDataspaceMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    static const int MAX_DIM_PRESENT    = 0x1;
    static const int PERM_INDEX_PRESENT = 0x2;

    const uint64_t starting_position = pos;

    const uint8_t version         = (uint8_t)readField(1, &pos);
    const uint8_t dimensionality  = (uint8_t)readField(1, &pos);
    const uint8_t flags           = (uint8_t)readField(1, &pos);
    pos += version == 1 ? 5 : 1; // go past reserved bytes

    if(H5_ERROR_CHECKING)
    {
        if(version != 1 && version != 2)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid dataspace version: %d", (int)version);
        }

        if(flags & PERM_INDEX_PRESENT)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported permutation indexes");
        }

        if(dimensionality > MAX_NDIMS)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported number of dimensions: %d", dimensionality);
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Dataspace Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        print2term("----------------\n");
        print2term("Version:                                                         %d\n", (int)version);
        print2term("Dimensionality:                                                  %d\n", (int)dimensionality);
        print2term("Flags:                                                           0x%x\n", (int)flags);
    }

    /* Read and Populate Data Dimensions */
    uint64_t num_elements = 0;
    metaData.ndims = MIN(dimensionality, MAX_NDIMS);
    if(metaData.ndims > 0)
    {
        num_elements = 1;
        for(int d = 0; d < metaData.ndims; d++)
        {
            metaData.dimensions[d] = readField(metaData.lengthsize, &pos);
            num_elements *= metaData.dimensions[d];
            if(H5_VERBOSE)
            {
                print2term("Dimension %d:                                                     %lu\n", (int)metaData.ndims, (unsigned long)metaData.dimensions[d]);
            }
        }

        /* Skip Over Maximum Dimensions */
        if(flags & MAX_DIM_PRESENT)
        {
            const int skip_bytes = dimensionality * metaData.lengthsize;
            pos += skip_bytes;
        }
    }

    if(H5_VERBOSE)
    {
        print2term("Number of Elements:                                              %lu\n", (unsigned long)num_elements);
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readLinkInfoMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readLinkInfoMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    static const int MAX_CREATE_PRESENT_BIT     = 0x01;
    static const int CREATE_ORDER_PRESENT_BIT   = 0x02;

    const uint64_t starting_position = pos;

    const uint64_t version = readField(1, &pos);
    const uint64_t flags = readField(1, &pos);

    if(H5_ERROR_CHECKING)
    {
        if(version != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid link info version: %d", (int)version);
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Link Information Message [%d], 0x%lx\n", dlvl, (unsigned long)starting_position);
        print2term("----------------\n");
    }

    /* Read Maximum Creation Index (number of elements in group) */
    if(flags & MAX_CREATE_PRESENT_BIT)
    {
        if(H5_VERBOSE)
        {
            const uint64_t max_create_index = readField(8, &pos);
            print2term("Maximum Creation Index:                                          %lu\n", (unsigned long)max_create_index);
        }
        else
        {
            pos += 8;
        }
    }

    /* Read Heap and Name Offsets */
    const uint64_t heap_address = readField(metaData.offsetsize, &pos);
    if(H5_VERBOSE)
    {
        const uint64_t name_index = readField(metaData.offsetsize, &pos);
        print2term("Heap Address:                                                    %lX\n", (unsigned long)heap_address);
        print2term("Name Index:                                                      %lX\n", (unsigned long)name_index);
    }
    else
    {
        pos += metaData.offsetsize;
    }

    if(flags & CREATE_ORDER_PRESENT_BIT)
    {
        if(H5_VERBOSE)
        {
            const uint64_t create_order_index = readField(metaData.offsetsize, &pos);
            print2term("Creation Order Index:                                            %lX\n", (unsigned long)create_order_index);
        }
        else
        {
            pos += metaData.offsetsize;
        }
    }

    // TODO: redundant, could be expelled in future
    heap_info_t heap_info_dense;

    /* Follow Heap Address if Provided */
    if((int)heap_address != -1)
    {
        readFractalHeap(LINK_MSG, heap_address, hdr_flags, dlvl, &heap_info_dense);
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readDatatypeMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readDatatypeMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    const uint64_t starting_position = pos;

    /* Read Message Info */
    const uint64_t version_class = readField(4, &pos);
    metaData.typesize = (int)readField(4, &pos);
    const uint64_t version = (version_class & 0xF0) >> 4;
    const uint64_t databits = version_class >> 8;

    if(H5_ERROR_CHECKING)
    {
        if(version != 1)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid datatype version: %d", (int)version);
        }
    }

    /* Set Data Type */
    metaData.type = (data_type_t)(version_class & 0x0F);
    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Datatype Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        print2term("----------------\n");
        print2term("Version:                                                         %d\n", (int)version);
        print2term("Data Class:                                                      %d, %s\n", (int)metaData.type, type2str(metaData.type));
        print2term("Data Size:                                                       %d\n", metaData.typesize);
    }

    /* Read Data Class Properties */
    switch(metaData.type)
    {
        case FIXED_POINT_TYPE:
        {
            metaData.signedval = ((databits & 0x08) >> 3) == 1;

            if(!H5_VERBOSE)
            {
                pos += 4;
            }
            else
            {
                const unsigned int byte_order = databits & 0x1;
                const unsigned int pad_type   = (databits & 0x06) >> 1;

                const uint16_t bit_offset     = (uint16_t)readField(2, &pos);
                const uint16_t bit_precision  = (uint16_t)readField(2, &pos);

                print2term("Byte Order:                                                      %d\n", (int)byte_order);
                print2term("Pading Type:                                                     %d\n", (int)pad_type);
                print2term("Signed Value:                                                    %d\n", (int)metaData.signedval);
                print2term("Bit Offset:                                                      %d\n", (int)bit_offset);
                print2term("Bit Precision:                                                   %d\n", (int)bit_precision);
            }
            break;
        }

        case FLOATING_POINT_TYPE:
        {
            if(!H5_VERBOSE)
            {
                pos += 12;
            }
            else
            {
                const unsigned int byte_order = ((databits & 0x40) >> 5) | (databits & 0x1);
                const unsigned int pad_type = (databits & 0x0E) >> 1;
                const unsigned int mant_norm = (databits & 0x30) >> 4;
                const unsigned int sign_loc = (databits & 0xFF00) >> 8;

                const uint16_t bit_offset     = (uint16_t)readField(2, &pos);
                const uint16_t bit_precision  = (uint16_t)readField(2, &pos);
                const uint8_t  exp_location   =  (uint8_t)readField(1, &pos);
                const uint8_t  exp_size       =  (uint8_t)readField(1, &pos);
                const uint8_t  mant_location  =  (uint8_t)readField(1, &pos);
                const uint8_t  mant_size      =  (uint8_t)readField(1, &pos);
                const uint32_t exp_bias       = (uint32_t)readField(4, &pos);

                print2term("Byte Order:                                                      %d\n", (int)byte_order);
                print2term("Pading Type:                                                     %d\n", (int)pad_type);
                print2term("Mantissa Normalization:                                          %d\n", (int)mant_norm);
                print2term("Sign Location:                                                   %d\n", (int)sign_loc);
                print2term("Bit Offset:                                                      %d\n", (int)bit_offset);
                print2term("Bit Precision:                                                   %d\n", (int)bit_precision);
                print2term("Exponent Location:                                               %d\n", (int)exp_location);
                print2term("Exponent Size:                                                   %d\n", (int)exp_size);
                print2term("Mantissa Location:                                               %d\n", (int)mant_location);
                print2term("Mantissa Size:                                                   %d\n", (int)mant_size);
                print2term("Exponent Bias:                                                   %d\n", (int)exp_bias);
            }
            break;
        }

        case VARIABLE_LENGTH_TYPE:
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "variable length data types require reading a global heap, which is not yet supported");
#if 0
            if(H5_VERBOSE)
            {
                unsigned int vt_type = databits & 0xF; // variable length type
                unsigned int padding = (databits & 0xF0) >> 4;
                unsigned int charset = (databits & 0xF00) >> 8;

                const char* vt_type_str = "unknown";
                if(vt_type == 0) vt_type_str = "Sequence";
                else if(vt_type == 1) vt_type_str = "String";

                const char* padding_str = "unknown";
                if(padding == 0) padding_str = "Null Terminate";
                else if(padding == 1) padding_str = "Null Pad";
                else if(padding == 2) padding_str = "Space Pad";

                const char* charset_str = "unknown";
                if(charset == 0) charset_str = "ASCII";
                else if(charset == 1) charset_str = "UTF-8";

                print2term("Variable Type:                                                   %d %s\n", (int)vt_type, vt_type_str);
                print2term("Padding Type:                                                    %d %s\n", (int)padding, padding_str);
                print2term("Character Set:                                                   %d %s\n", (int)charset, charset_str);
            }
            pos += readDatatypeMsg (pos, hdr_flags, dlvl);
            break;
#endif
        }

        case STRING_TYPE:
        {
            if(H5_VERBOSE)
            {
                const unsigned int padding = databits & 0x0F;
                const unsigned int charset = (databits & 0xF0) >> 4;

                const char* padding_str = "unknown";
                if(padding == 0) padding_str = "Null Terminate";
                else if(padding == 1) padding_str = "Null Pad";
                else if(padding == 2) padding_str = "Space Pad";

                const char* charset_str = "unknown";
                if(charset == 0) charset_str = "ASCII";
                else if(charset == 1) charset_str = "UTF-8";

                print2term("Padding Type:                                                    %d %s\n", (int)padding, padding_str);
                print2term("Character Set:                                                   %d %s\n", (int)charset, charset_str);
            }
            break;
        }

        default:
        {
            if(H5_ERROR_CHECKING)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported datatype: %d", (int)metaData.type);
            }
            break;
        }
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readFillValueMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readFillValueMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    const uint64_t starting_position = pos;

    const uint64_t version = readField(1, &pos);

    if(H5_ERROR_CHECKING)
    {
        if((version != 2) && (version != 3))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid fill value version: %d", (int)version);
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Fill Value Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        print2term("----------------\n");
    }

    if(version == 2)
    {
        if(!H5_VERBOSE)
        {
            pos += 2;
        }
        else
        {
            const uint8_t space_allocation_time = (uint8_t)readField(1, &pos);
            const uint8_t fill_value_write_time = (uint8_t)readField(1, &pos);

            print2term("Space Allocation Time:                                           %d\n", (int)space_allocation_time);
            print2term("Fill Value Write Time:                                           %d\n", (int)fill_value_write_time);
        }

        const uint8_t fill_value_defined = (uint8_t)readField(1, &pos);
        if(fill_value_defined)
        {
            metaData.fillsize = (int)readField(4, &pos);
            if(H5_VERBOSE)
            {
                print2term("Fill Value Size:                                                 %d\n", metaData.fillsize);
            }

            if(metaData.fillsize > 0)
            {
                const uint64_t fill_value = readField(metaData.fillsize, &pos);
                metaData.fill.fill_ll = fill_value;
                if(H5_VERBOSE)
                {
                    print2term("Fill Value:                                                      0x%llX\n", (unsigned long long)fill_value);
                }
            }
        }
    }
    else // if(version == 3)
    {
        const uint8_t flags = (uint8_t)readField(1, &pos);
        if(H5_VERBOSE)
        {
            print2term("Fill Flags:                                                      %02X\n", flags);
        }

        const uint8_t fill_value_defined = flags & 0x20;
        if(fill_value_defined)
        {
            metaData.fillsize = (int)readField(4, &pos);
            if(H5_VERBOSE)
            {
                print2term("Fill Value Size:                                                 %d\n", metaData.fillsize);
            }

            const uint64_t fill_value = readField(metaData.fillsize, &pos);
            metaData.fill.fill_ll = fill_value;
            if(H5_VERBOSE)
            {
                print2term("Fill Value:                                                      0x%llX\n", (unsigned long long)fill_value);
            }
        }
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readLinkMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readLinkMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    static const int SIZE_OF_LEN_OF_NAME_MASK   = 0x03;
    static const int CREATE_ORDER_PRESENT_BIT   = 0x04;
    static const int LINK_TYPE_PRESENT_BIT      = 0x08;
    static const int CHAR_SET_PRESENT_BIT       = 0x10;

    const uint64_t starting_position = pos;

    const uint64_t version = readField(1, &pos);
    const uint64_t flags = readField(1, &pos);

    if(H5_ERROR_CHECKING)
    {
        if(version != 1)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid link version: %d", (int)version);
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Link Message [%d]: 0x%x, 0x%lx\n", dlvl, (unsigned)flags, (unsigned long)starting_position);
        print2term("----------------\n");
    }

    /* Read Link Type */
    uint8_t link_type = 0; // default to hard link
    if(flags & LINK_TYPE_PRESENT_BIT)
    {
        link_type = readField(1, &pos);
        if(H5_VERBOSE)
        {
            print2term("Link Type:                                                       %lu\n", (unsigned long)link_type);
        }
    }

    /* Read Creation Order */
    if(flags & CREATE_ORDER_PRESENT_BIT)
    {
        if(H5_VERBOSE)
        {
            const uint64_t create_order = readField(8, &pos);
            print2term("Creation Order:                                                  %lX\n", (unsigned long)create_order);
        }
        else
        {
            pos += 8;
        }
    }

    /* Read Character Set */
    if(flags & CHAR_SET_PRESENT_BIT)
    {
        if(H5_VERBOSE)
        {
            const uint8_t char_set = readField(1, &pos);
            print2term("Character Set:                                                   %lu\n", (unsigned long)char_set);
        }
        else
        {
            pos += 1;
        }
    }

    /* Read Link Name */
    const int link_name_len_of_len = 1 << (flags & SIZE_OF_LEN_OF_NAME_MASK);
    if(H5_ERROR_CHECKING && (link_name_len_of_len > 8))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid link name length of length: %d", link_name_len_of_len);
    }

    const uint64_t link_name_len = readField(link_name_len_of_len, &pos);
    if(H5_VERBOSE)
    {
        print2term("Link Name Length:                                                %lu\n", (unsigned long)link_name_len);
    }

    uint8_t link_name[STR_BUFF_SIZE];
    readByteArray(link_name, link_name_len, &pos);
    link_name[link_name_len] = '\0';
    if(H5_VERBOSE)
    {
        print2term("Link Name:                                                       %s\n", link_name);
    }

    /* Process Link Type */
    if(link_type == 0) // hard link
    {
        const uint64_t object_header_addr = readField(metaData.offsetsize, &pos);
        if(H5_VERBOSE)
        {
            print2term("Hard Link - Object Header Address:                               0x%lx\n", object_header_addr);
        }

        if(dlvl < static_cast<int>(datasetPath.size()))
        {
            if(StringLib::match(reinterpret_cast<const char*>(link_name), datasetPath[dlvl]))
            {
                highestDataLevel = dlvl + 1;
                readObjHdr(object_header_addr, highestDataLevel);
            }
        }
    }
    else if(link_type == 1) // soft link
    {
        const uint16_t soft_link_len = readField(2, &pos);
        uint8_t soft_link[STR_BUFF_SIZE];
        readByteArray(soft_link, soft_link_len, &pos);
        soft_link[soft_link_len] = '\0';
        if(H5_VERBOSE)
        {
            print2term("Soft Link:                                                       %s\n", soft_link);
        }
    }
    else if(link_type == 64) // external link
    {
        const uint16_t ext_link_len = readField(2, &pos);
        uint8_t ext_link[STR_BUFF_SIZE];
        readByteArray(ext_link, ext_link_len, &pos);
        ext_link[ext_link_len] = '\0';
        if(H5_VERBOSE)
        {
            print2term("External Link:                                                   %s\n", ext_link);
        }
    }
    else if(H5_ERROR_CHECKING)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "invalid link type: %d", link_type);
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readDataLayoutMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readDataLayoutMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    const uint64_t starting_position = pos;

    /* Read Message Info */
    const uint64_t version = readField(1, &pos);
    metaData.layout = (layout_t)readField(1, &pos);

    if(H5_ERROR_CHECKING)
    {
        if(version != 3)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid data layout version: %d", (int)version);
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Data Layout Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        print2term("----------------\n");
        print2term("Version:                                                         %d\n", (int)version);
        print2term("Layout:                                                          %d, %s\n", (int)metaData.layout, layout2str(metaData.layout));
    }

    /* Read Layout Classes */
    switch(metaData.layout)
    {
        case COMPACT_LAYOUT:
        {
            metaData.size = (uint16_t)readField(2, &pos);
            metaData.address = pos;
            pos += metaData.size;
            break;
        }

        case CONTIGUOUS_LAYOUT:
        {
            metaData.address = readField(metaData.offsetsize, &pos);
            metaData.size = readField(metaData.lengthsize, &pos);
            break;
        }

        case CHUNKED_LAYOUT:
        {
            /* Read Number of Dimensions */
            int chunk_num_dim = (int)readField(1, &pos) - 1; // dimensionality is plus one over actual number of dimensions
            chunk_num_dim = MIN(chunk_num_dim, MAX_NDIMS);
            if(H5_ERROR_CHECKING)
            {
                if((metaData.ndims != UNKNOWN_VALUE) && (chunk_num_dim != metaData.ndims))
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "number of chunk dimensions does not match data dimensions: %d != %d", chunk_num_dim, metaData.ndims);
                }
            }

            /* Read Address of B-Tree */
            metaData.address = readField(metaData.offsetsize, &pos);

            /* Read Dimensions */
            if(chunk_num_dim > 0)
            {
                metaData.chunkelements = 1;
                for(int d = 0; d < chunk_num_dim; d++)
                {
                    metaData.chunkdims[d] = (uint32_t)readField(4, &pos);
                    metaData.chunkelements *= metaData.chunkdims[d];
                }
            }

            /* Read Size of Data Element */
            metaData.elementsize = (int)readField(4, &pos);

            /* Display Data Attributes */
            if(H5_VERBOSE)
            {
                print2term("Chunk Element Size:                                              %d\n", (int)metaData.elementsize);
                print2term("Number of Chunked Dimensions:                                    %d\n", (int)chunk_num_dim);
                for(int d = 0; d < chunk_num_dim; d++)
                {
                    print2term("Chunk Dimension %d:                                               %d\n", d, (int)metaData.chunkdims[d]);
                }
            }

            break;
        }

        default:
        {
            if(H5_ERROR_CHECKING)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid data layout: %d", (int)metaData.layout);
            }
        }
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readFilterMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readFilterMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    const uint64_t starting_position = pos;

    /* Read Message Info */
    const uint64_t version = readField(1, &pos);
    const uint32_t num_filters = (uint32_t)readField(1, &pos);

    if(H5_ERROR_CHECKING)
    {
        if((version != 1) && (version != 2))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid filter version: %d", (int)version);
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Filter Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        print2term("----------------\n");
        print2term("Version:                                                         %d\n", (int)version);
        print2term("Number of Filters:                                               %d\n", (int)num_filters);
    }

    /* Move past reserved bytes in version 1 */
    if(version == 1)
    {
        pos += 6;
    }

    /* Read Filters */
    for(int f = 0; f < (int)num_filters; f++)
    {
        /* Read Filter ID */
        const int filter = (int)readField(2, &pos);

        /* Read Filter Name Length */
        uint16_t name_len = 0;
        if(version == 1 || filter >= 256)
        {
            name_len = (uint16_t)readField(2, &pos);
        }

        /* Read Filter Parameters */
        const uint16_t flags              = (uint16_t)readField(2, &pos);
        const uint16_t num_parms          = (uint16_t)readField(2, &pos);

        /* Consistency Check Flags */
        if(H5_ERROR_CHECKING)
        {
            if((flags != 0) && (flags != 1))
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid flags in filter message: %02X", (int)flags);
            }
        }

        /* Read Name */
        uint8_t filter_name[STR_BUFF_SIZE];
        if(name_len > 0)
        {
            readByteArray(filter_name, name_len, &pos);
            const int name_padding = (8 - (name_len % 8)) % 8;
            pos += name_padding;
        }
        filter_name[name_len] = '\0';

        /* Display */
        if(H5_VERBOSE)
        {
            print2term("Filter Identification Value:                                     %d\n", filter);
            print2term("Flags:                                                           0x%x\n", (int)flags);
            print2term("Number Client Data Values:                                       %d\n", (int)num_parms);
            print2term("Filter Name:                                                     %s\n", filter_name);
        }

        /* Set Filter */
        if(filter < NUM_FILTERS)
        {
            metaData.filter[filter] = true;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid filter specified: %d", filter);
        }

        /* Client Data (unused) */
        pos += num_parms * 4;

        /* Handle Padding (version 1 only) */
        if(version == 1)
        {
            if(num_parms % 2 == 1)
            {
                pos += 4;
            }
        }
    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readAttributeMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readAttributeMsg (uint64_t pos, uint8_t hdr_flags, int dlvl, uint64_t size)
{
    (void)hdr_flags;

    const uint64_t starting_position = pos;

    /* Read Message Info */
    const uint64_t version = readField(1, &pos);

    /* Error Check Info */
    if(H5_ERROR_CHECKING)
    {
        const uint64_t reserved0 = readField(1, &pos);

        if(version != 1 && version != 3)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid attribute version: %d", (int)version);
        }
        if(reserved0 != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid reserved field: %d", (int)reserved0);
        }
    }
    else
    {
        pos += 1;
    }

    /* Get Sizes */
    const uint64_t name_size = readField(2, &pos);
    const uint64_t datatype_size = readField(2, &pos);
    const uint64_t dataspace_size = readField(2, &pos);

    /* Read Attribute Name */
    if(name_size > STR_BUFF_SIZE)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "attribute name string exceeded maximum length: %lu, 0x%lx\n", static_cast<unsigned long>(name_size), static_cast<unsigned long>(pos));
    }
    uint8_t attr_name[STR_BUFF_SIZE];

    /* Set attr_name: version 3 bumps by 4 bytes*/
    if (version == 1) {
        readByteArray(attr_name, name_size, &pos);
    }
    if (version == 3) {
        /* NOTE: did not extract encoding, assume ASCII; H5T_cset_t name_encoding; */
        pos += 1;
        readByteArray(attr_name, name_size, &pos);
    }

    mlog(CRITICAL, "received attr_name: %s", reinterpret_cast<const char*>(attr_name));

    if (version == 1) {
        // name padding, align to next 8-byte boundary
        pos += (8 - (name_size % 8)) % 8;
    }

    if(H5_ERROR_CHECKING)
    {
        if(attr_name[name_size - 1] != '\0')
        {
            attr_name[name_size -1] = '\0';
            throw RunTimeException(CRITICAL, RTE_ERROR, "attribute name string is not null terminated: %s, 0x%lx\n", attr_name, (unsigned long)pos);
        }
    }
    else
    {
        attr_name[name_size -1] = '\0';
    }

    /* Display Attribute Message Information */
    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Attribute Message [%d]: 0x%lx\n", dlvl, static_cast<unsigned long>(starting_position));
        print2term("----------------\n");
        print2term("Version:                                                         %d\n", static_cast<int>(version));
        print2term("Name:                                                            %s\n", reinterpret_cast<const char*>(attr_name));
        print2term("Message Size:                                                    %d\n", static_cast<int>(size));
        print2term("Datatype Message Bytes:                                          %d\n", static_cast<int>(datatype_size));
        print2term("Dataspace Message Bytes:                                         %d\n", static_cast<int>(dataspace_size));
    }

    /* Shortcut Out if Not Desired Attribute */
    if (((dlvl + 1) != static_cast<int>(datasetPath.size())) ||
        !StringLib::match(reinterpret_cast<const char*>(attr_name), datasetPath[dlvl]))
    {
        return size;
    }

    highestDataLevel = dlvl + 1;

    /* Read Datatype Message */
    const int datatype_bytes_read = readDatatypeMsg(pos, hdr_flags, dlvl);
    if(H5_ERROR_CHECKING && datatype_bytes_read > static_cast<int>(datatype_size))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read expected bytes for datatype message: %d > %d\n", datatype_bytes_read, static_cast<int>(datatype_size));
    }

    pos += datatype_bytes_read;
    if (version == 1) {
        // datatype padding align to next 8-byte boundary
        pos += (8 - (datatype_bytes_read % 8)) % 8;
    }

    /* Read Dataspace Message */
    const int dataspace_bytes_read = readDataspaceMsg(pos, hdr_flags, dlvl);
    if(H5_ERROR_CHECKING && dataspace_bytes_read > static_cast<int>(dataspace_size))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to read expected bytes for dataspace message: %d > %d\n", dataspace_bytes_read, static_cast<int>(dataspace_size));
    }

    pos += dataspace_bytes_read;
    if (version == 1) {
        // dataspace padding, align to next 8-byte boundary
        pos += (8 - (dataspace_bytes_read % 8)) % 8;
    }

    /* Calculate Meta Data */
    metaData.layout = CONTIGUOUS_LAYOUT;
    memset(metaData.filter, 0, sizeof(metaData.filter));
    metaData.address = pos;
    metaData.size = size - (pos - starting_position);

    /* Move to End of Data */
    pos += metaData.size;

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readAttributeInfoMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readAttributeInfoMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    static const int MAX_CREATE_PRESENT_BIT     = 0x01;
    static const int CREATE_ORDER_PRESENT_BIT   = 0x02;

    const uint64_t starting_position = pos;

    const uint64_t version = readField(1, &pos);
    const uint64_t flags = readField(1, &pos);

    if(H5_ERROR_CHECKING)
    {
        if(version != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid link info version: %d", static_cast<int>(version));
        }
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Attribute Information Message [%d], 0x%lx\n", dlvl, static_cast<unsigned long>(starting_position));
        print2term("----------------\n");
    }

    /* Read Maximum Creation Index (number of elements in group) */
    if(flags & MAX_CREATE_PRESENT_BIT)
    {
        if(H5_VERBOSE)
        {
            const uint16_t max_create_index = readField(2, &pos);
            print2term("Maximum Creation Index:                                          %u\n", static_cast<unsigned short>(max_create_index));
        }
        else
        {
            pos += 2;
        }
    }

    /* Read Heap and BTree Values */
    const uint64_t heap_address = readField(metaData.offsetsize, &pos);
    const uint64_t name_bt2_address = readField(metaData.offsetsize, &pos);

    if(H5_VERBOSE)
    {
        print2term("Heap Address:                                                    %lX\n", static_cast<unsigned long>(heap_address));
        print2term("Attribute Name v2 B-tree Address:                                %lX\n", static_cast<unsigned long>(name_bt2_address));
    }

    if(flags & CREATE_ORDER_PRESENT_BIT)
    {
        if(H5_VERBOSE)
        {
            const uint64_t create_order_index = readField(metaData.offsetsize, &pos);
            print2term("Creation Order Index:                                            %lX\n", static_cast<unsigned long>(create_order_index));
        }
        else
        {
            pos += metaData.offsetsize;
        }
    }

    /* Follow Heap Address if Provided */
    const uint64_t address_snapshot = metaData.address;
    heap_info_t heap_info_dense;

    /* Due to prev LinkInfo call, we can guarantee heap_address != -1 */
    readFractalHeap(ATTRIBUTE_MSG, heap_address, hdr_flags, dlvl, &heap_info_dense);

    /* Check if Attribute Located Non-Dense, Else Init Dense Search */
    if(address_snapshot == metaData.address && (int)name_bt2_address != -1)
    {
        const uint64_t heap_addr_snapshot = heap_address;
        const H5BTreeV2 curr_btreev2(heap_addr_snapshot, name_bt2_address, datasetPath[dlvl], &heap_info_dense, this);
        if (curr_btreev2.found_attr) {
            readAttributeMsg(curr_btreev2.pos_out, curr_btreev2.hdr_flags_out, curr_btreev2.hdr_dlvl_out, curr_btreev2.msg_size_out);
        }
        else {
            throw RunTimeException(CRITICAL, RTE_ERROR, "FAILED to locate attribute with dense btreeV2 reading");
        }

    }

    /* Return Bytes Read */
    const uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readHeaderContMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readHeaderContMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    const uint64_t starting_position = pos;

    /* Continuation Info */
    const uint64_t hc_offset = readField(metaData.offsetsize, &pos);
    const uint64_t hc_length = readField(metaData.lengthsize, &pos);

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Header Continuation Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        print2term("----------------\n");
        print2term("Offset:                                                          0x%lx\n", static_cast<unsigned long>(hc_offset));
        print2term("Length:                                                          %lu\n", static_cast<unsigned long>(hc_length));
    }

    /* Read Continuation Block */
    pos = hc_offset; // go to continuation block
    if(hdr_flags & H5CORO_CUSTOM_V1_FLAG)
    {
       const uint64_t end_of_chdr = hc_offset + hc_length;
       pos += readMessagesV1 (pos, end_of_chdr, hdr_flags, dlvl);
    }
    else
    {
        /* Read Continuation Header */
        if(H5_ERROR_CHECKING)
        {
            const uint64_t signature = readField(4, &pos);
            if(signature != H5_OCHK_SIGNATURE_LE)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid header continuation signature: 0x%llX", static_cast<unsigned long long>(signature));
            }
        }
        else
        {
            pos += 4;
        }

        /* Read Continuation Header Messages */
        const uint64_t end_of_chdr = hc_offset + hc_length - 4; // leave 4 bytes for checksum below
        pos += readMessages (pos, end_of_chdr, hdr_flags, dlvl);

        /* Verify Checksum */
        const uint64_t check_sum = readField(4, &pos);
        if(H5_ERROR_CHECKING)
        {
            (void)check_sum;
        }
    }

    /* Return Bytes Read */
    return metaData.offsetsize + metaData.lengthsize;
}

/*----------------------------------------------------------------------------
 * readSymbolTableMsg
 *----------------------------------------------------------------------------*/
int H5FileBuffer::readSymbolTableMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    const uint64_t starting_position = pos;

    /* Symbol Table Info */
    const uint64_t btree_addr = readField(metaData.offsetsize, &pos);
    const uint64_t heap_addr = readField(metaData.offsetsize, &pos);

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Symbol Table Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        print2term("----------------\n");
        print2term("B-Tree Address:                                                  0x%lx\n", static_cast<unsigned long>(btree_addr));
        print2term("Heap Address:                                                    0x%lx\n", static_cast<unsigned long>(heap_addr));
    }

    /* Read Heap Info */
    pos = heap_addr;
    if(!H5_ERROR_CHECKING)
    {
        pos += 24;
    }
    else
    {
        const uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_HEAP_SIGNATURE_LE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid heap signature: 0x%llX", static_cast<unsigned long long>(signature));
        }

        const uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "incorrect version of heap: %d", version);
        }

        pos += 19;
    }
    const uint64_t head_data_addr = readField(metaData.offsetsize, &pos);

    /* Go to Left-Most Node */
    pos = btree_addr;
    while(true)
    {
        /* Read Header Info */
        if(!H5_ERROR_CHECKING)
        {
            pos += 5;
        }
        else
        {
            const uint32_t signature = (uint32_t)readField(4, &pos);
            if(signature != H5_TREE_SIGNATURE_LE)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid group b-tree signature: 0x%llX", static_cast<unsigned long long>(signature));
            }

            const uint8_t node_type = (uint8_t)readField(1, &pos);
            if(node_type != 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "only group b-trees supported: %d", node_type);
            }
        }

        /* Read Branch Info */
        const uint8_t node_level = (uint8_t)readField(1, &pos);
        if(node_level == 0)
        {
            break;
        }

        pos += 2 + (2 * metaData.offsetsize) + metaData.lengthsize; // skip entries used, sibling addresses, and first key
        pos = readField(metaData.offsetsize, &pos); // read and go to first child
    }

    /* Traverse Children Left to Right */
    while(true)
    {
        const uint16_t entries_used = (uint16_t)readField(2, &pos);
        const uint64_t left_sibling = readField(metaData.offsetsize, &pos); (void)left_sibling;
        const uint64_t right_sibling = readField(metaData.offsetsize, &pos);
        const uint64_t key0 = readField(metaData.lengthsize, &pos); (void)key0;
        if(H5_VERBOSE && H5_EXTRA_DEBUG)
        {
            print2term("Entries Used:                                                    %d\n", static_cast<int>(entries_used));
            print2term("Left Sibling:                                                    0x%lx\n", static_cast<unsigned long>(left_sibling));
            print2term("Right Sibling:                                                   0x%lx\n", static_cast<unsigned long>(right_sibling));
            print2term("First Key:                                                       %lu\n", static_cast<unsigned long>(key0));
        }

        /* Loop Through Entries in Current Node */
        for(int entry = 0; entry < entries_used; entry++)
        {
            const uint64_t symbol_table_addr = readField(metaData.offsetsize, &pos);
            readSymbolTable(symbol_table_addr, head_data_addr, dlvl);
            pos += metaData.lengthsize; // skip next key;
            if(highestDataLevel > dlvl) break; // dataset found
        }

        /* Exit Loop or Go to Next Node */
        if(H5_INVALID(right_sibling) || (highestDataLevel > dlvl))
        {
            break;
        }
        pos = right_sibling;

        /* Read Header Info */
        if(!H5_ERROR_CHECKING)
        {
            pos += 6;
        }
        else
        {
            const uint32_t signature = (uint32_t)readField(4, &pos);
            if(signature != H5_TREE_SIGNATURE_LE)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid group b-tree signature: 0x%llX", static_cast<unsigned long long>(signature));
            }

            const uint8_t node_type = (uint8_t)readField(1, &pos);
            if(node_type != 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "only group b-trees supported: %d", node_type);
            }

            const uint8_t node_level = (uint8_t)readField(1, &pos);
            if(node_level != 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "traversed to non-leaf node: %d", node_level);
            }
        }
    }

    /* Return Bytes Read */
    return metaData.offsetsize + metaData.offsetsize;
}

/*----------------------------------------------------------------------------
 * parseDataset
 *----------------------------------------------------------------------------*/
void H5FileBuffer::parseDataset (void)
{
    assert(datasetName);

    /* Get Pointer to First Group in Dataset */
    const char* gptr; // group pointer
    if(datasetName[0] == '/')   gptr = &datasetName[1];
    else                        gptr = &datasetName[0];

    /* Build Path to Dataset */
    while(true)
    {
        datasetPath.push_back(gptr);                      // add group to dataset path
        char* nptr = StringLib::find(gptr, '/');    // look for next group marker
        if(nptr == NULL) break;                     // if not found, then exit
        *nptr = '\0';                               // terminate group string
        gptr = nptr + 1;                            // go to start of next group
    }

    if(H5_VERBOSE)
    {
        print2term("\n----------------\n");
        print2term("Dataset: ");
        for(unsigned g = 0; g < datasetPath.size(); g++)
        {
            print2term("/%s", datasetPath[g]);
        }
        print2term("\n----------------\n");
    }
}

/*----------------------------------------------------------------------------
 * type2str
 *----------------------------------------------------------------------------*/
const char* H5FileBuffer::type2str (data_type_t datatype)
{
    switch(datatype)
    {
        case FIXED_POINT_TYPE:      return "FIXED_POINT_TYPE";
        case FLOATING_POINT_TYPE:   return "FLOATING_POINT_TYPE";
        case TIME_TYPE:             return "TIME_TYPE";
        case STRING_TYPE:           return "STRING_TYPE";
        case BIT_FIELD_TYPE:        return "BIT_FIELD_TYPE";
        case OPAQUE_TYPE:           return "OPAQUE_TYPE";
        case COMPOUND_TYPE:         return "COMPOUND_TYPE";
        case REFERENCE_TYPE:        return "REFERENCE_TYPE";
        case ENUMERATED_TYPE:       return "ENUMERATED_TYPE";
        case VARIABLE_LENGTH_TYPE:  return "VARIABLE_LENGTH_TYPE";
        case ARRAY_TYPE:            return "ARRAY_TYPE";
        default:                    return "UNKNOWN_TYPE";
    }
}

/*----------------------------------------------------------------------------
 * layout2str
 *----------------------------------------------------------------------------*/
const char* H5FileBuffer::layout2str (layout_t layout)
{
    switch(layout)
    {
        case COMPACT_LAYOUT:    return "COMPACT_LAYOUT";
        case CONTIGUOUS_LAYOUT: return "CONTIGUOUS_LAYOUT";
        case CHUNKED_LAYOUT:    return "CHUNKED_LAYOUT";
        default:                return "UNKNOWN_LAYOUT";
    }
}

/*----------------------------------------------------------------------------
 * highestBit
 *----------------------------------------------------------------------------*/
int H5FileBuffer::highestBit (uint64_t value)
{
    int bit = 0;
    while(value >>= 1) bit++;
    return bit;
}

/*----------------------------------------------------------------------------
 * inflateChunk
 *----------------------------------------------------------------------------*/
int H5FileBuffer::inflateChunk (uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_size)
{
    int status;
    z_stream strm;

    /* Initialize z_stream State */
    strm.zalloc     = Z_NULL;
    strm.zfree      = Z_NULL;
    strm.opaque     = Z_NULL;
    strm.avail_in   = 0;
    strm.next_in    = Z_NULL;

    /* Initialize z_stream */
    status = inflateInit(&strm);
    if(status != Z_OK)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to initialize z_stream: %d", status);
    }

    /* Decompress Until Entire Chunk is Processed */
    strm.avail_in = input_size;
    strm.next_in = input;

    /* Decompress Chunk */
    do
    {
        strm.avail_out = output_size;
        strm.next_out = output;
        status = inflate(&strm, Z_NO_FLUSH);
        if(status != Z_OK) break;
    } while (strm.avail_out == 0);

    /* Clean Up z_stream */
    inflateEnd(&strm);

    /* Check Decompression Complete */
    if(status != Z_STREAM_END)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to inflate entire z_stream: %d", status);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * shuffleChunk
 *----------------------------------------------------------------------------*/
int H5FileBuffer::shuffleChunk (const uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_offset, uint32_t output_size, int type_size)
{
    if(H5_ERROR_CHECKING)
    {
        if(type_size <= 0 || type_size > 8)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid data size to perform shuffle on: %d", type_size);
        }
    }

    int64_t dst_index = 0;
    const int64_t shuffle_block_size = input_size / type_size;
    const int64_t num_elements = output_size / type_size;
    const int64_t start_element = output_offset / type_size;
    for(int64_t element_index = start_element; element_index < (start_element + num_elements); element_index++)
    {
        for(int64_t val_index = 0; val_index < type_size; val_index++)
        {
            const int64_t src_index = (val_index * shuffle_block_size) + element_index;
            output[dst_index++] = input[src_index];
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * metaGetKey
 *----------------------------------------------------------------------------*/
uint64_t H5FileBuffer::metaGetKey (const char* url)
{
    uint64_t key_value = 0;
    uint64_t* url_ptr = const_cast<uint64_t*>(reinterpret_cast<const uint64_t*>(url));
    for(int i = 0; i < MAX_META_NAME_SIZE; i+=sizeof(uint64_t))
    {
        key_value += *url_ptr;
        url_ptr++;
    }
    return key_value;
}

/*----------------------------------------------------------------------------
 * metaGetUrl
 *----------------------------------------------------------------------------*/
void H5FileBuffer::metaGetUrl (char* url, const char* resource, const char* dataset)
{
    /* Prepare File Name */
    const char* filename_ptr = resource;
    const char* char_ptr = resource;
    while(*char_ptr)
    {
        if(*char_ptr == '/')
        {
            filename_ptr = char_ptr + 1;
        }

        char_ptr++;
    }

    /* Prepare Dataset Name */
    const char* dataset_name_ptr = dataset;
    if(dataset[0] == '/') dataset_name_ptr++;

    /* Build URL */
    memset(url, 0, MAX_META_NAME_SIZE);
    StringLib::format(url, MAX_META_NAME_SIZE, "%s/%s", filename_ptr, dataset_name_ptr);

    /* Check URL Fits (at least 2 null terminators) */
    if(url[MAX_META_NAME_SIZE - 2] != '\0')
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "truncated meta repository url: %s", url);
    }
}

/******************************************************************************
 * HDF5 LITE LIBRARY
 ******************************************************************************/

Publisher*   H5Coro::rqstPub;
Subscriber*  H5Coro::rqstSub;
bool         H5Coro::readerActive;
Thread**     H5Coro::readerPids;
int          H5Coro::threadPoolSize;

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
    const H5FileBuffer h5file(&info, context, asset, resource, datasetname, startrow, numrows, _meta_only);
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
        info_t data_info;
        const H5FileBuffer h5file(&data_info, NULL, asset, resource, start_group, 0, 0);

        /* Free Data */
        delete [] data_info.data;
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

// NOLINTEND(misc-no-recursion)
