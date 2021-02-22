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

#ifdef __aws__
#include "S3Lib.h"
#endif

#include "H5Lite.h"
#include "core.h"

#include <assert.h>
#include <stdexcept>
#include <zlib.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#ifndef H5_EXTRA_DEBUG
#define H5_EXTRA_DEBUG false
#endif

/******************************************************************************
 * MACROS
 ******************************************************************************/

#define H5_INVALID(var)  (var == (0xFFFFFFFFFFFFFFFFllu >> (64 - (sizeof(var) * 8))))

/******************************************************************************
 * HDF5 LITE LIBRARY
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5Lite::init (void)
{

}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void H5Lite::deinit (void)
{

}

/*----------------------------------------------------------------------------
 * parseUrl
 *----------------------------------------------------------------------------*/
H5Lite::driver_t H5Lite::parseUrl (const char* url, const char** resource)
{
    /* Sanity Check Input */
    if(!url) return UNKNOWN;

    /* Set Resource */
    if(resource) 
    {
        const char* rptr = StringLib::find(url, "//");
        if(rptr)
        {
            *resource = rptr + 2;
        }
    }

    /* Return Driver */
    if(StringLib::find(url, "file://"))
    {
        return FILE;
    }
    else if(StringLib::find(url, "s3://"))
    {
        return S3;
    }
    else if(StringLib::find(url, "hsds://"))    
    {
        return HSDS;
    }
    else
    {
        return UNKNOWN;
    }
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
H5Lite::info_t H5Lite::read (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows)
{
    (void)valtype;
    (void)col;

    info_t info;

    /* Initialize Driver */
    const char* resource = NULL;
    driver_t driver = H5Lite::parseUrl(url, &resource);    
    if(driver == UNKNOWN)
    {
        throw RunTimeException("Invalid url: %s", url);
    }

    /* Start Trace */
    uint32_t parent_trace_id = TraceLib::grabId();
    uint32_t trace_id = start_trace_ext(parent_trace_id, "h5lite_read", "{\"url\":\"%s\", \"dataset\":\"%s\"}", url, datasetname);

    /* Open Resource and Read Dataset */
    H5FileBuffer h5file(&info, resource, datasetname, startrow, numrows, true, false);
    if(info.data)
    {
        bool data_valid = true;

        /* Perform Column Translation */
        if(info.numcols > 1)
        {
            /* Allocate Column Buffer */
            int tbuf_size = info.datasize / info.numcols;
            uint8_t* tbuf = new uint8_t [tbuf_size]; 

            /* Copy Column into Buffer */
            int tbuf_row_size = info.datasize / info.numrows;
            int tbuf_col_size = tbuf_row_size / info.numcols;
            for(int row = 0; row < info.numrows; row++)
            {
                int tbuf_offset = (row * tbuf_col_size);
                int data_offset = (row * tbuf_row_size) + (col * tbuf_col_size);
                LocalLib::copy(&tbuf[tbuf_offset], &info.data[data_offset], tbuf_col_size);
            }

            /* Switch Buffers */
            delete [] info.data;
            info.data = tbuf;
            info.datasize = tbuf_size;
            info.elements = info.elements / info.numcols;
        }
        
        /* Perform Integer Type Transaltion */        
        if(valtype == RecordObject::INTEGER)
        {
            /* Allocate Buffer of Integers */
            int* tbuf = new int [info.elements];

            /* Float to Int */
            if(info.datatype == RecordObject::REAL && info.typesize == sizeof(float))
            {
                float* dptr = (float*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (int)dptr[i];
                }
            }
            /* Double to Int */
            else if(info.datatype == RecordObject::REAL && info.typesize == sizeof(double))
            {
                double* dptr = (double*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (int)dptr[i];
                }
            }
            /* Char to Int */
            else if(info.datatype == RecordObject::INTEGER && info.typesize == sizeof(uint8_t))
            {
                uint8_t* dptr = (uint8_t*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (int)dptr[i];
                }
            }
            /* Short to Int */
            else if(info.datatype == RecordObject::INTEGER && info.typesize == sizeof(uint16_t))
            {
                uint16_t* dptr = (uint16_t*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (int)dptr[i];
                }
            }
            /* Int to Int */
            else if(info.datatype == RecordObject::INTEGER && info.typesize == sizeof(uint32_t))
            {
                uint32_t* dptr = (uint32_t*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (int)dptr[i];
                }
            }
            /* Long to Int */
            else if(info.datatype == RecordObject::INTEGER && info.typesize == sizeof(uint64_t))
            {
                uint64_t* dptr = (uint64_t*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (int)dptr[i];
                }
            }
            else
            {
                data_valid = false;
            }

            /* Switch Buffers */
            delete [] info.data;
            info.data = (uint8_t*)tbuf;
            info.datasize = sizeof(int) * info.elements;
        }
        
        /* Perform Integer Type Transaltion */        
        if(valtype == RecordObject::REAL)
        {
            /* Allocate Buffer of Integers */
            double* tbuf = new double [info.elements];

            /* Float to Double */
            if(info.datatype == RecordObject::REAL && info.typesize == sizeof(float))
            {
                float* dptr = (float*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (double)dptr[i];
                }
            }
            /* Double to Double */
            else if(info.datatype == RecordObject::REAL && info.typesize == sizeof(double))
            {
                double* dptr = (double*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (double)dptr[i];
                }
            }
            /* Char to Double */
            else if(info.datatype == RecordObject::INTEGER && info.typesize == sizeof(uint8_t))
            {
                uint8_t* dptr = (uint8_t*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (double)dptr[i];
                }
            }
            /* Short to Double */
            else if(info.datatype == RecordObject::INTEGER && info.typesize == sizeof(uint16_t))
            {
                uint16_t* dptr = (uint16_t*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (double)dptr[i];
                }
            }
            /* Int to Double */
            else if(info.datatype == RecordObject::INTEGER && info.typesize == sizeof(uint32_t))
            {
                uint32_t* dptr = (uint32_t*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (double)dptr[i];
                }
            }
            /* Long to Double */
            else if(info.datatype == RecordObject::INTEGER && info.typesize == sizeof(uint64_t))
            {
                uint64_t* dptr = (uint64_t*)info.data;
                for(int i = 0; i < info.elements; i++)
                {
                    tbuf[i] = (double)dptr[i];
                }
            }
            else
            {
                data_valid = false;
            }

            /* Switch Buffers */
            delete [] info.data;
            info.data = (uint8_t*)tbuf;
            info.datasize = sizeof(double) * info.elements;
        }

        /* Check Data Valid */
        if(!data_valid)
        {
            delete [] info.data;
            info.data = NULL;
            info.datasize = 0;
            throw RunTimeException("data translation failed for %s: [%d,%d] %d --> %d", datasetname, info.numcols, info.typesize, (int)info.datatype, (int)valtype);
        }

    }
    else
    {
        throw RunTimeException("failed to read dataset: %s", datasetname);
    }

    /* Stop Trace */
    stop_trace(trace_id);

    /* Log Info Message */
    mlog(INFO, "Lite-read %d elements (%d bytes) from %s %s\n", info.elements, info.datasize, url, datasetname);

    /* Return Info */
    return info;
}

/*----------------------------------------------------------------------------
 * traverse
 *----------------------------------------------------------------------------*/
bool H5Lite::traverse (const char* url, int max_depth, const char* start_group)
{
    (void)max_depth;
 
    bool status = true;

    try
    {
        /* Initialize Driver */
        const char* resource = NULL;
        driver_t driver = H5Lite::parseUrl(url, &resource);
        if(driver == UNKNOWN)
        {
            throw RunTimeException("Invalid url: %s", url);
        }

        /* Open File */
        info_t data_info;
        H5FileBuffer h5file(&data_info, resource, start_group, 0, 0, true, true);

        /* Free Data */
        if(data_info.data) delete [] data_info.data;

    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to traverse resource: %s\n", e.what());
    }

    /* Return Status */
    return status;
}


/******************************************************************************
 * H5 FILE BUFFER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5Lite::H5FileBuffer::H5FileBuffer (info_t* data_info, const char* filename, const char* _dataset, long startrow, long numrows, bool _error_checking, bool _verbose):
    ioCache(IO_CACHE_SIZE, ioHash)
{
    assert(data_info);
    assert(filename);
    assert(_dataset);    

    datasetStartRow         = startrow;
    datasetNumRows          = numrows;
    errorChecking           = _error_checking;
    verbose                 = _verbose;

    /* Initialize */
    offsetSize              = 0;
    lengthSize              = 0;
    groupLeafNodeK          = 0;
    groupInternalNodeK      = 0;
    rootGroupOffset         = 0;
    
    /* Initialize Data */
    dataType                = UNKNOWN_TYPE;
    dataTypeSize            = 0;
    dataFill.fill_ll        = 0LL;
    dataFillSize            = 0;
    dataNumDimensions       = 0;
    dataLayout              = UNKNOWN_LAYOUT;
    dataAddress             = 0;
    dataSize                = 0;
    dataChunkElements       = 0;
    dataChunkElementSize    = 0;
    dataChunkBuffer         = NULL;
    dataChunkBufferSize     = 0;
    dataShuffleBuffer       = NULL;
    highestDataLevel        = 0;

    /* Initialize Filters */
    for(int f = 0; f < NUM_FILTERS; f++)
    {
        dataFilter[f]           = INVALID_FILTER;
        dataFilterParms[f]      = NULL;
        dataNumFilterParms[f]   = 0;
    }

    /* Open File */
    fp = fopen(filename, "r");
    if(fp == NULL)
    {
        throw RunTimeException("Failed to open filename: %s", filename);
    }

    /* Process File */
    try
    {
        /* Clear Data Info */
        LocalLib::set(data_info, 0, sizeof(info_t));

        /* Get Dataset Path */
        parseDataset(_dataset);

        /* Read Superblock */
        readSuperblock();

        /* Read Data Attributes (Start at Root Group) */
        readObjHdr(rootGroupOffset, 0);

        /* Read Dataset */
        readDataset(data_info);        
    }
    catch(const RunTimeException& e)
    {
        /* Clean Up Allocations */
        if(data_info->data) delete [] data_info->data;
        data_info->data = NULL;
        data_info->datasize = 0;

        /* Rethrow Error */
        throw RunTimeException("%s", e.what());
    }    
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5Lite::H5FileBuffer::~H5FileBuffer (void)
{
    fclose(fp);
    if(dataset)             delete [] dataset;
    if(dataChunkBuffer)     delete [] dataChunkBuffer;
    if(dataShuffleBuffer)   delete [] dataShuffleBuffer;
    for(int f = 0; f < NUM_FILTERS; f++)
    {
        if(dataFilterParms[f]) delete [] dataFilterParms[f];
    }

    cache_entry_t entry;
    uint64_t key = ioCache.first(&entry);
    while(key != INVALID_KEY)
    {
        if(entry.data) delete [] entry.data;
        key = ioCache.next(&entry);
    }
}

/*----------------------------------------------------------------------------
 * ioRequest
 *----------------------------------------------------------------------------*/
uint8_t* H5Lite::H5FileBuffer::ioRequest (int64_t size, uint64_t* pos)
{
//static int io_reads = 0;
    uint8_t* buffer = NULL;
    int64_t buffer_offset = -1;
    uint64_t file_position = *pos;

    try // data request can be fulfilled from I/O cache
    {
        cache_entry_t entry = ioCache.get(file_position, cache_t::MATCH_NEAREST_UNDER);
        if((file_position >= entry.pos) && ((file_position + size) <= (entry.pos + entry.size)))
        {
            /* Set Buffer and Offset to Start of Requested Data */
            buffer = entry.data;
            buffer_offset = file_position - entry.pos;
        }
        else
        {
            throw std::out_of_range("file position outside cache entry");
        }
    }
    catch(const std::out_of_range& e) // data request requires I/O read
    {
        (void)e;

        /* Seek to New Position */
        if(fseek(fp, file_position, SEEK_SET) != 0)
        {
            throw RunTimeException("failed to go to I/O position: 0x%lx", file_position);
        }

        /* Calculate Size of Read */
        int64_t read_size = MAX(size, IO_BUFFSIZE);

        /* Read into Cache */
        cache_entry_t entry;
        entry.data = new uint8_t [read_size];
        entry.pos = file_position;
        entry.size = fread(entry.data, 1, read_size, fp);
//printf("READ 0x%08lx [%ld] (%d)\n", file_position, size, ++io_reads);
        if(entry.size < size)
        {
            throw RunTimeException("failed to read at least %ld bytes of data: %ld", size, entry.size);
        }

        /* Ensure Room in Cache */
        if(ioCache.length() >= IO_CACHE_SIZE)
        {
            cache_entry_t oldest_entry;
            uint64_t oldest_pos = ioCache.first(&oldest_entry);
            delete [] oldest_entry.data;
            ioCache.remove(oldest_pos);
        }

        /* Add Cache Entry */
        ioCache.add(file_position, entry);

        /* Set Buffer and Offset to Start of I/O Cached Buffer */
        buffer = entry.data;
        buffer_offset = 0;
    }

    /* Update Position */
    *pos += size;

    /* Return Data to Caller */
    return &buffer[buffer_offset];
}

/*----------------------------------------------------------------------------
 * ioHash
 *----------------------------------------------------------------------------*/
uint64_t H5Lite::H5FileBuffer::ioHash (uint64_t key)
{
    return key & (~IO_MASK);
}

/*----------------------------------------------------------------------------
 * readField
 *----------------------------------------------------------------------------*/
void H5Lite::H5FileBuffer::readByteArray (uint8_t* data, int64_t size, uint64_t* pos)
{
    assert(data);

    uint8_t* byte_ptr = ioRequest(size, pos);
    LocalLib::copy(data, byte_ptr, size);
}

/*----------------------------------------------------------------------------
 * readField
 *----------------------------------------------------------------------------*/
uint64_t H5Lite::H5FileBuffer::readField (int64_t size, uint64_t* pos)
{
    assert(pos);
    assert(size > 0);

    /* Request Data from I/O */
    uint8_t* data_ptr = ioRequest(size, pos);

    /*  Read Field Value */
    uint64_t value;
    switch(size)
    {
        case 8:     
        {
            value = *(uint64_t*)data_ptr;
            #ifdef __BE__
                value = LocalLib::swapll(value);
            #endif
            break;
        }

        case 4:     
        {
            value = *(uint32_t*)data_ptr;
            #ifdef __BE__
                value = LocalLib::swapl(value);
            #endif
            break;
        }

        case 2:
        {
            value = *(uint16_t*)data_ptr;
            #ifdef __BE__
                value = LocalLib::swaps(value);
            #endif
            break;
        }

        case 1:
        {
            value = *(uint8_t*)data_ptr;
            break;
        }

        default:
        {
            throw RunTimeException("invalid field size: %d", size);
        }
    }

    /* Return Field Value */
    return value;
}

/*----------------------------------------------------------------------------
 * readDataset
 *----------------------------------------------------------------------------*/
void H5Lite::H5FileBuffer::readDataset (info_t* data_info)
{
    /* Populate Info Struct */
    data_info->typesize = dataTypeSize;
    data_info->elements = 0;
    data_info->datasize = 0;
    data_info->data     = NULL;
    data_info->datatype = RecordObject::DYNAMIC;
    data_info->numrows  = 0;
    data_info->numcols  = 0;

    /* Sanity Check Data Attributes */
    if(dataTypeSize <= 0)
    {
        throw RunTimeException("missing data type information");
    }
    
    /* Calculate Size of Data Row (note dimension starts at 1) */
    uint64_t row_size = dataTypeSize;
    for(int d = 1; d < dataNumDimensions; d++)
    {
        row_size *= dataDimensions[d];
    }

    /* Get Number of Rows */
    uint64_t first_dimension = (dataNumDimensions > 0) ? dataDimensions[0] : 0;
    datasetNumRows = (datasetNumRows == ALL_ROWS) ? first_dimension : datasetNumRows;
    if((datasetStartRow + datasetNumRows) > first_dimension)
    {
        throw RunTimeException("read exceeds number of rows: %d + %d > %d", (int)datasetStartRow, (int)datasetNumRows, (int)first_dimension);
    }

    /* Allocate Data Buffer */
    uint8_t* buffer = NULL;
    int64_t buffer_size = row_size * datasetNumRows;
    if(buffer_size > 0)
    {
        buffer = new uint8_t [buffer_size];

        /* Fill Buffer with Fill Value (if provided) */
        if(dataFillSize > 0)
        {
            for(int64_t i = 0; i < buffer_size; i += dataFillSize)
            {
                LocalLib::copy(&buffer[i], &dataFill.fill_ll, dataFillSize);
            }
        }
    }

    /* Populate Rest of Info Struct */
    data_info->elements = buffer_size / dataTypeSize;
    data_info->datasize = buffer_size;
    data_info->data     = buffer;
    data_info->numrows  = datasetNumRows; 

    if      (dataNumDimensions == 0)            data_info->numcols = 0;
    else if (dataNumDimensions == 1)            data_info->numcols = 1;
    else if (dataNumDimensions >= 2)            data_info->numcols = dataDimensions[1];

    if      (dataType == FIXED_POINT_TYPE)      data_info->datatype = RecordObject::INTEGER;
    else if (dataType == FLOATING_POINT_TYPE)   data_info->datatype = RecordObject::REAL;
    else if (dataType == STRING_TYPE)           data_info->datatype = RecordObject::TEXT;
    
    /* Calculate Buffer Start */
    uint64_t buffer_offset = row_size * datasetStartRow;

    /* Check if Data Address and Data Size is Valid */
    if(errorChecking)
    {
        if(H5_INVALID(dataAddress))
        {
            throw RunTimeException("data not allocated in contiguous layout");
        }
        else if(dataSize != 0 && dataSize < ((int64_t)buffer_offset + buffer_size))
        {
            throw RunTimeException("read exceeds available data: %d != %d", dataSize, buffer_size);
        }
        if((dataFilter[DEFLATE_FILTER] || dataFilter[SHUFFLE_FILTER]) && ((dataLayout == COMPACT_LAYOUT) || (dataLayout == CONTIGUOUS_LAYOUT)))
        {
            throw RunTimeException("filters unsupported on non-chunked layouts");
        }
    }

    /* Read Dataset */
    if(buffer_size > 0)
    {
        switch(dataLayout)
        {
            case COMPACT_LAYOUT:
            case CONTIGUOUS_LAYOUT:
            {
                uint64_t data_addr = dataAddress + buffer_offset;
                uint8_t* data_ptr = ioRequest(buffer_size, &data_addr);
                LocalLib::copy(buffer, data_ptr, buffer_size);
                break;
            }

            case CHUNKED_LAYOUT:
            {
                /* Chunk Layout Specific Error Checks */
                if(errorChecking)
                {
                    if(dataChunkElementSize != dataTypeSize)
                    {
                        throw RunTimeException("chunk element size does not match data element size: %d != %d", dataChunkElementSize, dataTypeSize);
                    }
                    else if(dataChunkElements <= 0)
                    {
                        throw RunTimeException("invalid number of chunk elements: %ld\n", (long)dataChunkElements);
                    }
                }

                /* Allocate Data Chunk Buffer */
                dataChunkBufferSize = dataChunkElements * dataTypeSize;
                dataChunkBuffer = new uint8_t [dataChunkBufferSize];
                dataShuffleBuffer = new uint8_t [dataChunkBufferSize];

                /* Read B-Tree */
                readBTreeV1(dataAddress, buffer, buffer_size, buffer_offset);
                break;
            }

            default:
            {
                if(errorChecking)
                {
                    throw RunTimeException("invalid data layout: %d", (int)dataLayout);
                }
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * readSuperblock
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readSuperblock (void)
{
    uint64_t pos = 0;

    /* Read and Verify Superblock Info */
    if(errorChecking)
    {
        uint64_t signature = readField(8, &pos);
        if(signature != H5_SIGNATURE_LE)
        {
            throw RunTimeException("invalid h5 file signature: 0x%llX", (unsigned long long)signature);
        }

        uint64_t superblock_version = readField(1, &pos);
        if(superblock_version != 0)
        {
            throw RunTimeException("invalid h5 file superblock version: %d", (int)superblock_version);
        }

        uint64_t freespace_version = readField(1, &pos);
        if(freespace_version != 0)
        {
            throw RunTimeException("invalid h5 file free space version: %d", (int)freespace_version);
        }

        uint64_t roottable_version = readField(1, &pos);
        if(roottable_version != 0)
        {
            throw RunTimeException("invalid h5 file root table version: %d", (int)roottable_version);
        }

        uint64_t headermsg_version = readField(1, &pos);
        if(headermsg_version != 0)
        {
            throw RunTimeException("invalid h5 file header message version: %d", (int)headermsg_version);
        }
    }

    /* Read Sizes */
    pos = 13;
    offsetSize          = readField(1, &pos);
    lengthSize          = readField(1, &pos);
    groupLeafNodeK      = readField(2, &pos);
    groupInternalNodeK  = readField(2, &pos);

    /* Read Group Offset */
    pos = 64;
    rootGroupOffset     = readField(offsetSize, &pos);

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "File Information\n");
        mlog(RAW, "----------------\n");
        mlog(RAW, "Size of Offsets:                                                 %lu\n",     (unsigned long)offsetSize);
        mlog(RAW, "Size of Lengths:                                                 %lu\n",     (unsigned long)lengthSize);
        mlog(RAW, "Group Leaf Node K:                                               %lu\n",     (unsigned long)groupLeafNodeK);
        mlog(RAW, "Group Internal Node K:                                           %lu\n",     (unsigned long)groupInternalNodeK);
        mlog(RAW, "Root Object Header Address:                                      0x%lX\n",   (long unsigned)rootGroupOffset);
    }

    /* Return Bytes Read */
    return pos;
}

/*----------------------------------------------------------------------------
 * readFractalHeap
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readFractalHeap (msg_type_t msg_type, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    static const int FRHP_CHECKSUM_DIRECT_BLOCKS = 0x02;

    uint64_t starting_position = pos;

    if(!errorChecking)
    {
        pos += 5;
    }
    else
    {
        uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_FRHP_SIGNATURE_LE)
        {
            throw RunTimeException("invalid heap signature: 0x%llX", (unsigned long long)signature);
        }

        uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            throw RunTimeException("invalid heap version: %d", (int)version);
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Fractal Heap [%d]: %d, 0x%lx\n", dlvl, (int)msg_type, starting_position);
        mlog(RAW, "----------------\n");
    }

    /*  Read Fractal Heap Header */
    uint16_t    heap_obj_id_len     = (uint16_t)readField(2, &pos); // Heap ID Length
    uint16_t    io_filter_len       = (uint16_t)readField(2, &pos); // I/O Filters' Encoded Length
    uint8_t     flags               =  (uint8_t)readField(1, &pos); // Flags
    uint32_t    max_size_mg_obj     = (uint32_t)readField(4, &pos); // Maximum Size of Managed Objects
    uint64_t    next_huge_obj_id    = (uint64_t)readField(lengthSize, &pos); // Next Huge Object ID
    uint64_t    btree_addr_huge_obj = (uint64_t)readField(offsetSize, &pos); // v2 B-tree Address of Huge Objects
    uint64_t    free_space_mg_blks  = (uint64_t)readField(lengthSize, &pos); // Amount of Free Space in Managed Blocks
    uint64_t    addr_free_space_mg  = (uint64_t)readField(offsetSize, &pos); // Address of Managed Block Free Space Manager
    uint64_t    mg_space            = (uint64_t)readField(lengthSize, &pos); // Amount of Manged Space in Heap
    uint64_t    alloc_mg_space      = (uint64_t)readField(lengthSize, &pos); // Amount of Allocated Managed Space in Heap
    uint64_t    dblk_alloc_iter     = (uint64_t)readField(lengthSize, &pos); // Offset of Direct Block Allocation Iterator in Managed Space
    uint64_t    mg_objs             = (uint64_t)readField(lengthSize, &pos); // Number of Managed Objects in Heap
    uint64_t    huge_obj_size       = (uint64_t)readField(lengthSize, &pos); // Size of Huge Objects in Heap
    uint64_t    huge_objs           = (uint64_t)readField(lengthSize, &pos); // Number of Huge Objects in Heap
    uint64_t    tiny_obj_size       = (uint64_t)readField(lengthSize, &pos); // Size of Tiny Objects in Heap
    uint64_t    tiny_objs           = (uint64_t)readField(lengthSize, &pos); // Number of Timing Objects in Heap
    uint16_t    table_width         = (uint16_t)readField(2, &pos); // Table Width
    uint64_t    starting_blk_size   = (uint64_t)readField(lengthSize, &pos); // Starting Block Size
    uint64_t    max_dblk_size       = (uint64_t)readField(lengthSize, &pos); // Maximum Direct Block Size
    uint16_t    max_heap_size       = (uint16_t)readField(2, &pos); // Maximum Heap Size
    uint16_t    start_num_rows      = (uint16_t)readField(2, &pos); // Starting # of Rows in Root Indirect Block
    uint64_t    root_blk_addr       = (uint64_t)readField(offsetSize, &pos); // Address of Root Block
    uint16_t    curr_num_rows       = (uint16_t)readField(2, &pos); // Current # of Rows in Root Indirect Block
    if(verbose)
    {
        mlog(RAW, "Heap ID Length:                                                  %lu\n", (unsigned long)heap_obj_id_len);
        mlog(RAW, "I/O Filters' Encoded Length:                                     %lu\n", (unsigned long)io_filter_len);
        mlog(RAW, "Flags:                                                           0x%lx\n", (unsigned long)flags);
        mlog(RAW, "Maximum Size of Managed Objects:                                 %lu\n", (unsigned long)max_size_mg_obj);
        mlog(RAW, "Next Huge Object ID:                                             %lu\n", (unsigned long)next_huge_obj_id);
        mlog(RAW, "v2 B-tree Address of Huge Objects:                               0x%lx\n", (unsigned long)btree_addr_huge_obj);
        mlog(RAW, "Amount of Free Space in Managed Blocks:                          %lu\n", (unsigned long)free_space_mg_blks);
        mlog(RAW, "Address of Managed Block Free Space Manager:                     0x%lx\n", (unsigned long)addr_free_space_mg);
        mlog(RAW, "Amount of Manged Space in Heap:                                  %lu\n", (unsigned long)mg_space);
        mlog(RAW, "Amount of Allocated Managed Space in Heap:                       %lu\n", (unsigned long)alloc_mg_space);
        mlog(RAW, "Offset of Direct Block Allocation Iterator in Managed Space:     %lu\n", (unsigned long)dblk_alloc_iter);
        mlog(RAW, "Number of Managed Objects in Heap:                               %lu\n", (unsigned long)mg_objs);
        mlog(RAW, "Size of Huge Objects in Heap:                                    %lu\n", (unsigned long)huge_obj_size);
        mlog(RAW, "Number of Huge Objects in Heap:                                  %lu\n", (unsigned long)huge_objs);
        mlog(RAW, "Size of Tiny Objects in Heap:                                    %lu\n", (unsigned long)tiny_obj_size);
        mlog(RAW, "Number of Timing Objects in Heap:                                %lu\n", (unsigned long)tiny_objs);
        mlog(RAW, "Table Width:                                                     %lu\n", (unsigned long)table_width);
        mlog(RAW, "Starting Block Size:                                             %lu\n", (unsigned long)starting_blk_size);
        mlog(RAW, "Maximum Direct Block Size:                                       %lu\n", (unsigned long)max_dblk_size);
        mlog(RAW, "Maximum Heap Size:                                               %lu\n", (unsigned long)max_heap_size);
        mlog(RAW, "Starting # of Rows in Root Indirect Block:                       %lu\n", (unsigned long)start_num_rows);
        mlog(RAW, "Address of Root Block:                                           0x%lx\n", (unsigned long)root_blk_addr);
        mlog(RAW, "Current # of Rows in Root Indirect Block:                        %lu\n", (unsigned long)curr_num_rows);
    }

    /* Read Filter Information */
    if(io_filter_len > 0)
    {
        uint64_t filter_root_dblk   = (uint64_t)readField(lengthSize, &pos); // Size of Filtered Root Direct Block
        uint32_t filter_mask        = (uint32_t)readField(4, &pos); // I/O Filter Mask
        mlog(RAW, "Size of Filtered Root Direct Block:                              %lu\n", (unsigned long)filter_root_dblk);
        mlog(RAW, "I/O Filter Mask:                                                 %lu\n", (unsigned long)filter_mask);

        throw RunTimeException("Filtering unsupported on fractal heap: %d", io_filter_len);
        // readMessage(FILTER_MSG, io_filter_len, pos, hdr_flags, dlvl); // this currently populates filter for dataset
    }

    /* Check Checksum */
    uint64_t check_sum = readField(4, &pos);
    if(errorChecking)
    {
        (void)check_sum;
    }

    /* Build Heap Info Structure */
    heap_info_t heap_info = {
        .table_width        = table_width,
        .curr_num_rows      = curr_num_rows,
        .starting_blk_size  = (int)starting_blk_size,
        .max_dblk_size      = (int)max_dblk_size,
        .blk_offset_size    = ((max_heap_size + 7) / 8),
        .dblk_checksum      = ((flags & FRHP_CHECKSUM_DIRECT_BLOCKS) != 0),
        .msg_type           = msg_type,
        .num_objects        = (int)mg_objs,
        .cur_objects        = 0 // updated as objects are read
    };

    /* Process Blocks */
    if(heap_info.curr_num_rows == 0)
    {
        /* Direct Blocks */
        int bytes_read = readDirectBlock(&heap_info, heap_info.starting_blk_size, root_blk_addr, hdr_flags, dlvl);
        if(errorChecking && (bytes_read > heap_info.starting_blk_size))
        {
            throw RunTimeException("direct block contianed more bytes than specified: %d > %d", bytes_read, heap_info.starting_blk_size);            
        }
        pos += heap_info.starting_blk_size;        
    }
    else
    {
        /* Indirect Blocks */
        int bytes_read = readIndirectBlock(&heap_info, 0, root_blk_addr, hdr_flags, dlvl);
        if(errorChecking && (bytes_read > heap_info.starting_blk_size))
        {
            throw RunTimeException("indirect block contianed more bytes than specified: %d > %d", bytes_read, heap_info.starting_blk_size);            
        }
        pos += bytes_read;        
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readDirectBlock
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readDirectBlock (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    uint64_t starting_position = pos;

    if(!errorChecking)
    {
        pos += 5;
    }
    else
    {
        uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_FHDB_SIGNATURE_LE)
        {
            throw RunTimeException("invalid direct block signature: 0x%llX", (unsigned long long)signature);
        }

        uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            throw RunTimeException("invalid direct block version: %d", (int)version);
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Direct Block [%d,%d,%d]: 0x%lx\n", dlvl, (int)heap_info->msg_type, block_size, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
    }
    
    /* Read Block Header */
    if(!verbose)
    {
        pos += offsetSize + heap_info->blk_offset_size;
    }
    else
    {
        uint64_t heap_hdr_addr = readField(offsetSize, &pos); // Heap Header Address
        uint64_t blk_offset    = readField(heap_info->blk_offset_size, &pos); // Block Offset
        mlog(RAW, "Heap Header Address:                                             0x%lx\n", heap_hdr_addr);
        mlog(RAW, "Block Offset:                                                    0x%lx\n", blk_offset);
    }

    if(heap_info->dblk_checksum)
    {
        uint64_t check_sum = readField(4, &pos);
        if(errorChecking)
        {
            (void)check_sum;
        }
    }

    /* Read Block Data */
    int data_left = block_size - (5 + offsetSize + heap_info->blk_offset_size + ((int)heap_info->dblk_checksum * 4));
    while((data_left > 0) && (heap_info->cur_objects < heap_info->num_objects))
    {
        /* Peak if More Messages */
        uint64_t peak_addr = pos;
        int peak_size = MIN((1 << highestBit(data_left)), 8);
        if(readField(peak_size, &peak_addr) == 0)
        {
            if(verbose)
            {
                mlog(RAW, "\nExiting direct block 0x%lx early at 0x%lx\n", starting_position, pos);
            }
            break;
        }

        /* Read Message */
        uint64_t data_read = readMessage(heap_info->msg_type, data_left, pos, hdr_flags, dlvl);
        pos += data_read;
        data_left -= data_read;

        /* Update Number of Objects Read */
        heap_info->cur_objects++;

        /* Check Reading Past Block */
        if(errorChecking)
        {
            if(data_left < 0)
            {
                throw RunTimeException("reading message exceeded end of direct block: 0x%x", starting_position);
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
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readIndirectBlock
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readIndirectBlock (heap_info_t* heap_info, int block_size, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    uint64_t starting_position = pos;

    if(!errorChecking)
    {
        pos += 5;
    }
    else
    {
        uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_FHIB_SIGNATURE_LE)
        {
            throw RunTimeException("invalid direct block signature: 0x%llX", (unsigned long long)signature);
        }

        uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            throw RunTimeException("invalid direct block version: %d", (int)version);
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Indirect Block [%d,%d]: 0x%lx\n", dlvl, (int)heap_info->msg_type, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
    }
    
    /* Read Block Header */
    if(!verbose)
    {
        pos += offsetSize + heap_info->blk_offset_size;
    }
    else
    {
        uint64_t heap_hdr_addr = readField(offsetSize, &pos); // Heap Header Address
        uint64_t blk_offset    = readField(heap_info->blk_offset_size, &pos); // Block Offset
        mlog(RAW, "Heap Header Address:                                             0x%lx\n", heap_hdr_addr);
        mlog(RAW, "Block Offset:                                                    0x%lx\n", blk_offset);
    }

    /* Calculate Number of Direct and Indirect Blocks (see III.G. Disk Format: Level 1G - Fractal Heap) */
    int nrows = heap_info->curr_num_rows; // used for "root" indirect block only
    if(block_size > 0) nrows = (highestBit(block_size) - highestBit(heap_info->starting_blk_size * heap_info->table_width)) + 1;
    int max_dblock_rows = (highestBit(heap_info->max_dblk_size) - highestBit(heap_info->starting_blk_size)) + 2;
    int K = MIN(nrows, max_dblock_rows) * heap_info->table_width;
    int N = K - (max_dblock_rows * heap_info->table_width);
    if(verbose)
    {
        mlog(RAW, "Number of Rows:                                                  %d\n", nrows);
        mlog(RAW, "Maximum Direct Block Rows:                                       %d\n", max_dblock_rows);
        mlog(RAW, "Number of Direct Blocks (K):                                     %d\n", K);
        mlog(RAW, "Number of Indirect Blocks (N):                                   %d\n", N);
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
                if(errorChecking)
                {
                    if(row >= K)
                    {
                        throw RunTimeException("unexpected direct block row: %d, %d >= %d\n", row_block_size, row, K);
                    }
                }

                /* Read Direct Block Address */
                uint64_t direct_block_addr = readField(offsetSize, &pos);
                // note: filters are unsupported, but if present would be read here
                if(!H5_INVALID(direct_block_addr) && (dlvl <= highestDataLevel))
                {
                    /* Read Direct Block */
                    int bytes_read = readDirectBlock(heap_info, row_block_size, direct_block_addr, hdr_flags, dlvl);
                    if(errorChecking && (bytes_read > row_block_size))
                    {
                        throw RunTimeException("direct block contained more bytes than specified: %d > %d", bytes_read, row_block_size);            
                    }
                }
            }
            else /* Indirect Block Entry */
            {
                if(errorChecking)
                {
                    if(row < K || row >= N)
                    {
                        throw RunTimeException("unexpected indirect block row: %d, %d, %d\n", row_block_size, row, N);
                    }
                }

                /* Read Indirect Block Address */
                uint64_t indirect_block_addr = readField(offsetSize, &pos);
                if(!H5_INVALID(indirect_block_addr) && (dlvl <= highestDataLevel))
                {
                    /* Read Direct Block */
                    int bytes_read = readIndirectBlock(heap_info, row_block_size, indirect_block_addr, hdr_flags, dlvl);
                    if(errorChecking && (bytes_read > row_block_size))
                    {
                        throw RunTimeException("indirect block contained more bytes than specified: %d > %d", bytes_read, row_block_size);            
                    }
                }
            }
        }
    }

    /* Read Checksum */
    uint64_t check_sum = readField(4, &pos);
    if(errorChecking)
    {
        (void)check_sum;
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readBTreeV1
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readBTreeV1 (uint64_t pos, uint8_t* buffer, uint64_t buffer_size, uint64_t buffer_offset)
{
    uint64_t starting_position = pos;
    uint64_t data_key1 = datasetStartRow;
    uint64_t data_key2 = datasetStartRow + datasetNumRows - 1;

    /* Check Signature and Node Type */
    if(!errorChecking)
    {
        pos += 5;
    }
    else
    {
        uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_TREE_SIGNATURE_LE)
        {
            throw RunTimeException("invalid b-tree signature: 0x%llX", (unsigned long long)signature);
        }
        
        uint8_t node_type = (uint8_t)readField(1, &pos);
        if(node_type != 1)
        {
            throw RunTimeException("only raw data chunk b-trees supported: %d", node_type);
        }
    }

    /* Read Node Level and Number of Entries */
    uint8_t node_level = (uint8_t)readField(1, &pos);
    uint16_t entries_used = (uint16_t)readField(2, &pos);

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "B-Tree Node: 0x%lx\n", (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Node Level:                                                      %d\n", (int)node_level);
        mlog(RAW, "Entries Used:                                                    %d\n", (int)entries_used);
    }

    /* Skip Sibling Addresses */
    pos += offsetSize * 2;

    /* Read First Key */
    btree_node_t curr_node = readBTreeNodeV1(dataNumDimensions, &pos);

    /* Read Children */
    for(int e = 0; e < entries_used; e++)
    {
        /* Read Child Address */
        uint64_t child_addr = readField(offsetSize, &pos);

        /* Read Next Key */
        btree_node_t next_node = readBTreeNodeV1(dataNumDimensions, &pos);

        /*  Get Child Keys */
        uint64_t child_key1 = curr_node.row_key;
        uint64_t child_key2 = next_node.row_key; // there is always +1 keys
        if(next_node.chunk_size == 0 && dataNumDimensions > 0)
        {
            child_key2 = dataDimensions[0];
        }

        /* Display */
        if(verbose && H5_EXTRA_DEBUG)
        {
            mlog(RAW, "\nEntry:                                                           %d[%d]\n", (int)node_level, e);
            mlog(RAW, "Chunk Size:                                                      %u | %u\n", (unsigned int)curr_node.chunk_size, (unsigned int)next_node.chunk_size);
            mlog(RAW, "Filter Mask:                                                     0x%x | 0x%x\n", (unsigned int)curr_node.filter_mask, (unsigned int)next_node.filter_mask);
            mlog(RAW, "Chunk Key:                                                       %lu | %lu\n", (unsigned long)child_key1, (unsigned long)child_key2);
            mlog(RAW, "Data Key:                                                        %lu | %lu\n", (unsigned long)data_key1, (unsigned long)data_key2);
            mlog(RAW, "Child Address:                                                   0x%lx\n", (unsigned long)child_addr);
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
                for(int i = 0; i < dataNumDimensions; i++)
                {
                    uint64_t slice_size = curr_node.slice[i] * dataTypeSize;
                    for(int j = i + 1; j < dataNumDimensions; j++)
                    {
                        slice_size *= dataDimensions[j];
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
                        throw RunTimeException("invalid location to read data: %ld, %lu", (unsigned long)chunk_offset, (unsigned long)buffer_offset);
                    }
                }
                
                /* Calculate Chunk Index - offset into chunk buffer to read from */
                uint64_t chunk_index = 0;
                if(buffer_offset > chunk_offset)
                {
                    chunk_index = buffer_offset - chunk_offset;
                    if((int64_t)chunk_index >= dataChunkBufferSize)
                    {
                        throw RunTimeException("invalid location to read chunk: %ld, %lu", (unsigned long)chunk_offset, (unsigned long)buffer_offset);
                    }
                }

                /* Calculate Chunk Bytes - number of bytes to read from chunk buffer */
                int64_t chunk_bytes = dataChunkBufferSize - chunk_index;
                if(chunk_bytes < 0)
                {
                    throw RunTimeException("no bytes of chunk data to read: %ld, %lu", (long)chunk_bytes, (unsigned long)chunk_index);
                }
                else if((buffer_index + chunk_bytes) > buffer_size)
                {
                    chunk_bytes = buffer_size - buffer_index;
                }

                /* Display Info */
                if(verbose && H5_EXTRA_DEBUG)
                {
                    mlog(RAW, "Buffer Index:                                                    %ld (%ld)\n", (unsigned long)buffer_index, (unsigned long)(buffer_index/dataTypeSize));
                    mlog(RAW, "Buffer Bytes:                                                    %ld (%ld)\n", (unsigned long)chunk_bytes, (unsigned long)(chunk_bytes/dataTypeSize));
                }

                /* Read Chunk */
                if(dataFilter[DEFLATE_FILTER])
                {
                    /* Read Data into Chunk Buffer */
                    uint8_t* chunk_ptr = ioRequest(curr_node.chunk_size, &child_addr);

                    if((chunk_bytes == dataChunkBufferSize) && (!dataFilter[SHUFFLE_FILTER]))
                    {
                        /* Inflate Directly into Data Buffer */
                        inflateChunk(chunk_ptr, curr_node.chunk_size, &buffer[buffer_index], chunk_bytes);
                    }
                    else
                    {
                        /* Inflate into Data Chunk Buffer */
                        inflateChunk(chunk_ptr, curr_node.chunk_size, dataChunkBuffer, dataChunkBufferSize);

                        if(dataFilter[SHUFFLE_FILTER])
                        {
                            /* Shuffle Data Chunk Buffer into Data Buffer */
                            shuffleChunk(dataChunkBuffer, dataChunkBufferSize, dataShuffleBuffer, dataChunkBufferSize, dataTypeSize);

                            /* Copy Unshuffled Data into Data Buffer */
                            LocalLib::copy(&buffer[buffer_index], &dataShuffleBuffer[chunk_index], chunk_bytes);
                        }
                        else
                        {
                            /* Copy Data Chunk Buffer into Data Buffer */
                            LocalLib::copy(&buffer[buffer_index], &dataChunkBuffer[chunk_index], chunk_bytes);
                        }
                    }
                }
                else /* no supported filters */
                {
                    if(errorChecking)
                    {
                        if(dataFilter[SHUFFLE_FILTER])
                        {
                            throw RunTimeException("shuffle filter unsupported on uncompressed chunk");
                        }
                        else if((chunk_bytes == dataChunkBufferSize) && (curr_node.chunk_size != chunk_bytes))
                        {
                            throw RunTimeException("mismatch in chunk size: %lu, %lu", (unsigned long)curr_node.chunk_size, (unsigned long)chunk_bytes);
                        }
                    }

                    /* Read Data into Data Buffer */
                    uint8_t* chunk_ptr = ioRequest(curr_node.chunk_size, &child_addr);
                    LocalLib::copy(&buffer[buffer_index], &chunk_ptr[chunk_index], chunk_bytes);
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
H5Lite::H5FileBuffer::btree_node_t H5Lite::H5FileBuffer::readBTreeNodeV1 (int ndims, uint64_t* pos)
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
    uint64_t trailing_zero = readField(8, pos);
    if(errorChecking)
    {
        if(trailing_zero % dataTypeSize != 0)
        {
            throw RunTimeException("key did not include a trailing zero: %d", trailing_zero);
        }
        else if(verbose && H5_EXTRA_DEBUG)
        {
            mlog(RAW, "Trailing Zero:                                                   %d\n", (int)trailing_zero);
        }
    }

    /* Set Node Key */
    node.row_key = node.slice[0];

    /* Return Copy of Node */
    return node;    
}

/*----------------------------------------------------------------------------
 * readSymbolTable
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readSymbolTable (uint64_t pos, uint64_t heap_data_addr, int dlvl)
{
    uint64_t starting_position = pos;

    /* Check Signature and Version */
    if(!errorChecking)
    {
        pos += 6;
    }
    else
    {
        uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_SNOD_SIGNATURE_LE)
        {
            throw RunTimeException("invalid symbol table signature: 0x%llX", (unsigned long long)signature);
        }
        
        uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 1)
        {
            throw RunTimeException("incorrect version of symbole table: %d", (int)version);
        }

        uint8_t reserved0 = (uint8_t)readField(1, &pos);
        if(reserved0 != 0)
        {
            throw RunTimeException("incorrect reserved value: %d", (int)reserved0);
        }
    }

    /* Read Number of Symbols */
    uint16_t num_symbols = (uint16_t)readField(2, &pos);
    if(errorChecking)
    {
        if(num_symbols > groupLeafNodeK)
        {
            throw RunTimeException("number of symbols exceeds group leaf node K: %d > %d\n", (int)num_symbols, (int)groupLeafNodeK);
        }
    }

    /* Read Symbols */
    for(int s = 0; s < num_symbols; s++)
    {
        /* Read Symbol Entry */
        uint64_t link_name_offset = readField(offsetSize, &pos);
        uint64_t obj_hdr_addr = readField(offsetSize, &pos);
        uint32_t cache_type = (uint32_t)readField(4, &pos);
        pos += 20; // reserved + scratch pad
        if(errorChecking)
        {
            if(cache_type == 2)
            {
                throw RunTimeException("symbolic links are unsupported");
            }
        }

        /* Read Link Name */
        uint64_t link_name_addr = heap_data_addr + link_name_offset;
        uint8_t link_name[STR_BUFF_SIZE];
        int i = 0;
        while(true)
        {
            if(i >= STR_BUFF_SIZE)
            {
                throw RunTimeException("link name string exceeded maximum length: %d, 0x%lx\n", i, (unsigned long)pos);
            }

            uint8_t c = (uint8_t)readField(1, &link_name_addr);
            link_name[i++] = c;

            if(c == 0)
            {
                break;
            }
        }
        link_name[i] = '\0';
        if(verbose)
        {
            mlog(RAW, "Link Name:                                                       %s\n", link_name);
            mlog(RAW, "Object Header Address:                                           0x%lx\n", obj_hdr_addr);
        }

        /* Process Link */
        if(dlvl < datasetPath.length())
        {
            if(StringLib::match((const char*)link_name, datasetPath[dlvl]))
            {
                highestDataLevel = dlvl + 1;
                readObjHdr(obj_hdr_addr, highestDataLevel);
                break; // dataset found
            }
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readObjHdr
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readObjHdr (uint64_t pos, int dlvl)
{
    static const int SIZE_OF_CHUNK_0_MASK      = 0x03;
    static const int STORE_CHANGE_PHASE_BIT    = 0x10;
    static const int FILE_STATS_BIT            = 0x20;

    uint64_t starting_position = pos;

    /* Peek at Version / Process Version 1 */
    uint64_t peeking_position = pos;
    uint8_t peek = readField(1, &peeking_position);
    if(peek == 1) return readObjHdrV1(starting_position, dlvl);

    /* Read Object Header */
    if(!errorChecking)
    {
        pos += 5; // move past signature and version
    }
    else
    {
        uint64_t signature = readField(4, &pos);
        if(signature != H5_OHDR_SIGNATURE_LE)
        {
            throw RunTimeException("invalid header signature: 0x%llX", (unsigned long long)signature);
        }

        uint64_t version = readField(1, &pos);
        if(version != 2)
        {
            throw RunTimeException("invalid header version: %d", (int)version);
        }
    }

    /* Read Option Time Fields */
    uint8_t obj_hdr_flags = (uint8_t)readField(1, &pos);
    if(obj_hdr_flags & FILE_STATS_BIT)
    {        
        if(!verbose)
        {
            pos += 16; // move past time fields
        }
        else
        {
            uint64_t access_time         = readField(4, &pos);
            uint64_t modification_time   = readField(4, &pos);
            uint64_t change_time         = readField(4, &pos);
            uint64_t birth_time          = readField(4, &pos);

            mlog(RAW, "\n----------------\n");
            mlog(RAW, "Object Information [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
            mlog(RAW, "----------------\n");

            TimeLib::gmt_time_t access_gmt = TimeLib::gettime(access_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Access Time:                                                     %d:%d:%d:%d:%d\n", access_gmt.year, access_gmt.day, access_gmt.hour, access_gmt.minute, access_gmt.second);

            TimeLib::gmt_time_t modification_gmt = TimeLib::gettime(modification_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Modification Time:                                               %d:%d:%d:%d:%d\n", modification_gmt.year, modification_gmt.day, modification_gmt.hour, modification_gmt.minute, modification_gmt.second);

            TimeLib::gmt_time_t change_gmt = TimeLib::gettime(change_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Change Time:                                                     %d:%d:%d:%d:%d\n", change_gmt.year, change_gmt.day, change_gmt.hour, change_gmt.minute, change_gmt.second);

            TimeLib::gmt_time_t birth_gmt = TimeLib::gettime(birth_time * TIME_MILLISECS_IN_A_SECOND);
            mlog(RAW, "Birth Time:                                                      %d:%d:%d:%d:%d\n", birth_gmt.year, birth_gmt.day, birth_gmt.hour, birth_gmt.minute, birth_gmt.second);
        }
    }

    /* Optional Phase Attributes */
    if(obj_hdr_flags & STORE_CHANGE_PHASE_BIT)
    {
        if(!verbose)
        {
            pos += 4; // move past phase attributes
        }
        else
        {
            uint64_t max_compact_attr = readField(2, &pos); (void)max_compact_attr;
            uint64_t max_dense_attr = readField(2, &pos); (void)max_dense_attr;
        }
    }

    /* Read Header Messages */
    uint64_t size_of_chunk0 = readField(1 << (obj_hdr_flags & SIZE_OF_CHUNK_0_MASK), &pos);
    uint64_t end_of_hdr = pos + size_of_chunk0;
    pos += readMessages (pos, end_of_hdr, obj_hdr_flags, dlvl);    

    /* Verify Checksum */
    uint64_t check_sum = readField(4, &pos);
    if(errorChecking)
    {
        (void)check_sum;
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readMessages
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readMessages (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl)
{
    static const int ATTR_CREATION_TRACK_BIT   = 0x04;

    uint64_t starting_position = pos;

    while(pos < end) 
    {
        /* Read Message Info */
        uint8_t     msg_type    = (uint8_t)readField(1, &pos);
        uint16_t    msg_size    = (uint16_t)readField(2, &pos);
        uint8_t     msg_flags   = (uint8_t)readField(1, &pos); (void)msg_flags;

        if(hdr_flags & ATTR_CREATION_TRACK_BIT)
        {
            uint64_t msg_order = (uint8_t)readField(2, &pos); (void)msg_order;
        }

        /* Read Each Message */
        int bytes_read = readMessage((msg_type_t)msg_type, msg_size, pos, hdr_flags, dlvl);
        if(errorChecking && (bytes_read != msg_size))
        {
            throw RunTimeException("header continuation message different size than specified: %d != %d", bytes_read, msg_size);            
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
    if(errorChecking)
    {
        if(pos != end)
        {
            throw RunTimeException("did not read correct number of bytes: %lu != %lu", (unsigned long)pos, (unsigned long)end);            
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readObjHdrV1
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readObjHdrV1 (uint64_t pos, int dlvl)
{
    uint64_t starting_position = pos;

    /* Read Version */
    if(!errorChecking)
    {
        pos += 2;
    }
    else
    {
        uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 1)
        {
            throw RunTimeException("invalid header version: %d", (int)version);
        }

        uint8_t reserved0 = (uint8_t)readField(1, &pos); 
        if(reserved0 != 0)
        {
            throw RunTimeException("invalid reserved field: %d", (int)reserved0);
        }
    }

    /* Read Number of Header Messages */
    if(!verbose)
    {
        pos += 2;
    }
    else
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Object Information V1 [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");

        uint16_t num_hdr_msgs = (uint16_t)readField(2, &pos);
        mlog(RAW, "Number of Header Messages:                                       %d\n", (int)num_hdr_msgs);
    }

    /* Read Object Reference Count */
    if(!verbose)
    {
        pos += 4;
    }
    else
    {
        uint32_t obj_ref_count = (uint32_t)readField(4, &pos);
        mlog(RAW, "Object Reference Count:                                          %d\n", (int)obj_ref_count);
    }

    /* Read Object Header Size */
    uint64_t obj_hdr_size = readField(lengthSize, &pos);
    uint64_t end_of_hdr = pos + obj_hdr_size;
    if(verbose)
    {
        mlog(RAW, "Object Header Size:                                              %d\n", (int)obj_hdr_size);
        mlog(RAW, "End of Header:                                                   0x%lx\n", (unsigned long)end_of_hdr);
    }

    /* Read Header Messages */
    pos += readMessagesV1(pos, end_of_hdr, H5LITE_CUSTOM_V1_FLAG, dlvl);

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readMessagesV1
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readMessagesV1 (uint64_t pos, uint64_t end, uint8_t hdr_flags, int dlvl)
{
    static const int SIZE_OF_V1_PREFIX = 8;

    uint64_t starting_position = pos;

    while(pos < (end - SIZE_OF_V1_PREFIX))
    {
        uint16_t    msg_type    = (uint16_t)readField(2, &pos);
        uint16_t    msg_size    = (uint16_t)readField(2, &pos);
        uint8_t     msg_flags   = (uint8_t)readField(1, &pos); (void)msg_flags;
        
        /* Reserved Bytes */
        if(!errorChecking)
        {
            pos += 3;
        }
        else
        {
            uint8_t  reserved1 = (uint8_t)readField(1, &pos);
            uint16_t reserved2 = (uint16_t)readField(2, &pos);
            if((reserved1 != 0) && (reserved2 != 0))
            {
                throw RunTimeException("invalid reserved fields: %d, %d", (int)reserved1, (int)reserved2);
            }
        }

        /* Read Each Message */
        int bytes_read = readMessage((msg_type_t)msg_type, msg_size, pos, hdr_flags, dlvl);

        /* Handle 8-byte Alignment of Messages */
        if((bytes_read % 8) > 0) bytes_read += 8 - (bytes_read % 8);
        if(errorChecking && (bytes_read != msg_size))
        {
            throw RunTimeException("message of type %d at position 0x%lx different size than specified: %d != %d", (int)msg_type, (unsigned long)pos, bytes_read, msg_size);            
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
    if(errorChecking)
    {
        if(pos != end)
        {
            throw RunTimeException("did not read correct number of bytes: %lu != %lu", (unsigned long)pos, (unsigned long)end);            
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readMessage
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readMessage (msg_type_t msg_type, uint64_t size, uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    switch(msg_type)
    {
        case DATASPACE_MSG:     return readDataspaceMsg(pos, hdr_flags, dlvl);
        case LINK_INFO_MSG:     return readLinkInfoMsg(pos, hdr_flags, dlvl);
        case DATATYPE_MSG:      return readDatatypeMsg(pos, hdr_flags, dlvl);
        case FILL_VALUE_MSG:    return readFillValueMsg(pos, hdr_flags, dlvl);
        case LINK_MSG:          return readLinkMsg(pos, hdr_flags, dlvl);
        case DATA_LAYOUT_MSG:   return readDataLayoutMsg(pos, hdr_flags, dlvl);
        case FILTER_MSG:        return readFilterMsg(pos, hdr_flags, dlvl);
        case HEADER_CONT_MSG:   return readHeaderContMsg(pos, hdr_flags, dlvl);
        case SYMBOL_TABLE_MSG:  return readSymbolTableMsg(pos, hdr_flags, dlvl);

        default:
        {
            if(verbose)
            {
                mlog(RAW, "Skipped Message [%d]: 0x%x, %d, 0x%lx\n", dlvl, (int)msg_type, (int)size, (unsigned long)pos);
            }
            
            return size;
        }
    }
}

/*----------------------------------------------------------------------------
 * readDataspaceMsg
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readDataspaceMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    static const int MAX_DIM_PRESENT    = 0x1;
    static const int PERM_INDEX_PRESENT = 0x2;

    uint64_t starting_position = pos;

    uint8_t version         = (uint8_t)readField(1, &pos);
    uint8_t dimensionality  = (uint8_t)readField(1, &pos);
    uint8_t flags           = (uint8_t)readField(1, &pos);
    pos += 5; // go past reserved bytes

    if(errorChecking)
    {
        if(version != 1)
        {
            throw RunTimeException("invalid dataspace version: %d", (int)version);
        }

        if(flags & PERM_INDEX_PRESENT)
        {
            throw RunTimeException("unsupported permutation indexes");
        }

        if(dimensionality > MAX_NDIMS)
        {
            throw RunTimeException("unsupported number of dimensions: %d", dimensionality);
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Dataspace Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Version:                                                         %d\n", (int)version);
        mlog(RAW, "Dimensionality:                                                  %d\n", (int)dimensionality);
        mlog(RAW, "Flags:                                                           0x%x\n", (int)flags);
    }

    /* Read and Populate Data Dimensions */
    uint64_t num_elements = 0;
    dataNumDimensions = MIN(dimensionality, MAX_NDIMS);
    if(dataNumDimensions > 0)
    {
        num_elements = 1;
        for(int d = 0; d < dataNumDimensions; d++)
        {
            dataDimensions[d] = readField(lengthSize, &pos);
            num_elements *= dataDimensions[d];
            if(verbose)
            {
                mlog(RAW, "Dimension %d:                                                     %lu\n", (int)dataNumDimensions, (unsigned long)dataDimensions[d]);
            }
        }

        /* Skip Over Maximum Dimensions */
        if(flags & MAX_DIM_PRESENT)
        {
            pos += dataNumDimensions * lengthSize;
        }
    }

    if(verbose)
    {
        mlog(RAW, "Number of Elements:                                              %lu\n", (unsigned long)num_elements);
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readLinkInfoMsg
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readLinkInfoMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    static const int MAX_CREATE_PRESENT_BIT     = 0x01;
    static const int CREATE_ORDER_PRESENT_BIT   = 0x02;

    uint64_t starting_position = pos;

    uint64_t version = readField(1, &pos);
    uint64_t flags = readField(1, &pos);

    if(errorChecking)
    {
        if(version != 0)
        {
            throw RunTimeException("invalid link info version: %d", (int)version);
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Link Information Message [%d], 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
    }

    /* Read Maximum Creation Index (number of elements in group) */
    if(flags & MAX_CREATE_PRESENT_BIT)
    {
        uint64_t max_create_index = readField(8, &pos);
        if(verbose)
        {
            mlog(RAW, "Maximum Creation Index:                                          %lu\n", (unsigned long)max_create_index);
        }
    }

    /* Read Heap and Name Offsets */
    uint64_t heap_address = readField(offsetSize, &pos);
    uint64_t name_index = readField(offsetSize, &pos);
    if(verbose)
    {
        mlog(RAW, "Heap Address:                                                    %lX\n", (unsigned long)heap_address);
        mlog(RAW, "Name Index:                                                      %lX\n", (unsigned long)name_index);
    }

    if(flags & CREATE_ORDER_PRESENT_BIT)
    {
        uint64_t create_order_index = readField(8, &pos);
        if(verbose)
        {
            mlog(RAW, "Creation Order Index:                                            %lX\n", (unsigned long)create_order_index);
        }
    }

    /* Follow Heap Address if Provided */
    if((int)heap_address != -1)
    {
        readFractalHeap(LINK_MSG, heap_address, hdr_flags, dlvl);
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readDatatypeMsg
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readDatatypeMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    uint64_t starting_position = pos;

    /* Read Message Info */
    uint64_t version_class = readField(4, &pos);
    dataTypeSize = (int)readField(4, &pos);
    uint64_t version = (version_class & 0xF0) >> 4;
    uint64_t databits = version_class >> 8;

    if(errorChecking)
    {
        if(version != 1)
        {
            throw RunTimeException("invalid datatype version: %d", (int)version);
        }
    }

    /* Set Data Type */
    dataType = (data_type_t)(version_class & 0x0F);
    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Datatype Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Version:                                                         %d\n", (int)version);
        mlog(RAW, "Data Class:                                                      %d, %s\n", (int)dataType, type2str(dataType));
        mlog(RAW, "Data Size:                                                       %d\n", dataTypeSize);
    }

    /* Read Data Class Properties */
    switch(dataType)
    {
        case FIXED_POINT_TYPE:
        {
            if(!verbose)
            {
                pos += 4;
            }
            else
            {
                unsigned int byte_order = databits & 0x1;
                unsigned int pad_type = (databits & 0x06) >> 1;
                unsigned int sign_loc = (databits & 0x08) >> 3;

                uint16_t bit_offset     = (uint16_t)readField(2, &pos);
                uint16_t bit_precision  = (uint16_t)readField(2, &pos);

                mlog(RAW, "Byte Order:                                                      %d\n", (int)byte_order);
                mlog(RAW, "Pading Type:                                                     %d\n", (int)pad_type);
                mlog(RAW, "Sign Location:                                                   %d\n", (int)sign_loc);
                mlog(RAW, "Bit Offset:                                                      %d\n", (int)bit_offset);
                mlog(RAW, "Bit Precision:                                                   %d\n", (int)bit_precision);
            }
            break;
        }

        case FLOATING_POINT_TYPE:
        {
            if(!verbose)
            {
                pos += 12;
            }
            else
            {
                unsigned int byte_order = ((databits & 0x40) >> 5) | (databits & 0x1);
                unsigned int pad_type = (databits & 0x0E) >> 1;
                unsigned int mant_norm = (databits & 0x30) >> 4;
                unsigned int sign_loc = (databits & 0xFF00) >> 8;

                uint16_t bit_offset     = (uint16_t)readField(2, &pos);
                uint16_t bit_precision  = (uint16_t)readField(2, &pos);
                uint8_t  exp_location   =  (uint8_t)readField(1, &pos);
                uint8_t  exp_size       =  (uint8_t)readField(1, &pos);
                uint8_t  mant_location  =  (uint8_t)readField(1, &pos);
                uint8_t  mant_size      =  (uint8_t)readField(1, &pos);
                uint32_t exp_bias       = (uint32_t)readField(4, &pos);

                mlog(RAW, "Byte Order:                                                      %d\n", (int)byte_order);
                mlog(RAW, "Pading Type:                                                     %d\n", (int)pad_type);
                mlog(RAW, "Mantissa Normalization:                                          %d\n", (int)mant_norm);
                mlog(RAW, "Sign Location:                                                   %d\n", (int)sign_loc);
                mlog(RAW, "Bit Offset:                                                      %d\n", (int)bit_offset);
                mlog(RAW, "Bit Precision:                                                   %d\n", (int)bit_precision);
                mlog(RAW, "Exponent Location:                                               %d\n", (int)exp_location);
                mlog(RAW, "Exponent Size:                                                   %d\n", (int)exp_size);
                mlog(RAW, "Mantissa Location:                                               %d\n", (int)mant_location);
                mlog(RAW, "Mantissa Size:                                                   %d\n", (int)mant_size);
                mlog(RAW, "Exponent Bias:                                                   %d\n", (int)exp_bias);
            }
            break;
        }

        default: 
        {
            if(errorChecking)
            {
                throw RunTimeException("unsupported datatype: %d", (int)dataType);
            }
            break;
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readFillValueMsg
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readFillValueMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    uint64_t starting_position = pos;

    uint64_t version = readField(1, &pos);

    if(errorChecking)
    {
        if(version != 2)
        {
            throw RunTimeException("invalid fill value version: %d", (int)version);
        }
    }

    if(!verbose)
    {
        pos += 2;
    }
    else
    {
        uint8_t space_allocation_time = (uint8_t)readField(1, &pos);
        uint8_t fill_value_write_time = (uint8_t)readField(1, &pos);

        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Fill Value Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Space Allocation Time:                                           %d\n", (int)space_allocation_time);
        mlog(RAW, "Fill Value Write Time:                                           %d\n", (int)fill_value_write_time);
    }

    uint8_t fill_value_defined = (uint8_t)readField(1, &pos);
    if(fill_value_defined)
    {
        dataFillSize = (int)readField(4, &pos);
        if(verbose)
        {
            mlog(RAW, "Fill Value Size:                                                 %d\n", dataFillSize);
        }

        if(dataFillSize > 0)
        {
            uint64_t fill_value = readField(dataFillSize, &pos);
            dataFill.fill_ll = fill_value;
            if(verbose)
            {
                mlog(RAW, "Fill Value:                                                      0x%llX\n", (unsigned long long)fill_value);
            }
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readLinkMsg
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readLinkMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    static const int SIZE_OF_LEN_OF_NAME_MASK   = 0x03;
    static const int CREATE_ORDER_PRESENT_BIT   = 0x04;
    static const int LINK_TYPE_PRESENT_BIT      = 0x08;
    static const int CHAR_SET_PRESENT_BIT       = 0x10;

    uint64_t starting_position = pos;

    uint64_t version = readField(1, &pos);
    uint64_t flags = readField(1, &pos);

    if(errorChecking)
    {
        if(version != 1)
        {
            throw RunTimeException("invalid link version: %d", (int)version);
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Link Message [%d]: 0x%x, 0x%lx\n", dlvl, (unsigned)flags, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
    }

    /* Read Link Type */
    uint8_t link_type = 0; // default to hard link
    if(flags & LINK_TYPE_PRESENT_BIT)
    {
        link_type = readField(1, &pos);
        if(verbose)
        {
            mlog(RAW, "Link Type:                                                       %lu\n", (unsigned long)link_type);
        }
    }

    /* Read Creation Order */
    if(flags & CREATE_ORDER_PRESENT_BIT)
    {
        uint64_t create_order = readField(8, &pos);
        if(verbose)
        {
            mlog(RAW, "Creation Order:                                                  %lX\n", (unsigned long)create_order);
        }
    }

    /* Read Character Set */
    if(flags & CHAR_SET_PRESENT_BIT)
    {
        uint8_t char_set = readField(1, &pos);
        if(verbose)
        {
            mlog(RAW, "Character Set:                                                   %lu\n", (unsigned long)char_set);
        }
    }

    /* Read Link Name */
    int link_name_len_of_len = 1 << (flags & SIZE_OF_LEN_OF_NAME_MASK);
    if(errorChecking && (link_name_len_of_len > 8))
    {
        throw RunTimeException("invalid link name length of length: %d", (int)link_name_len_of_len);
    }
    
    uint64_t link_name_len = readField(link_name_len_of_len, &pos);
    if(verbose)
    {
        mlog(RAW, "Link Name Length:                                                %lu\n", (unsigned long)link_name_len);
    }

    uint8_t link_name[STR_BUFF_SIZE];
    readByteArray(link_name, link_name_len, &pos);
    link_name[link_name_len] = '\0';
    if(verbose)
    {
        mlog(RAW, "Link Name:                                                       %s\n", link_name);
    }

    /* Process Link Type */
    if(link_type == 0) // hard link
    {
        uint64_t object_header_addr = readField(offsetSize, &pos);
        if(verbose)
        {
            mlog(RAW, "Hard Link - Object Header Address:                               0x%lx\n", object_header_addr);
        }

        if(dlvl < datasetPath.length())
        {
            if(StringLib::match((const char*)link_name, datasetPath[dlvl]))
            {
                highestDataLevel = dlvl + 1;
                readObjHdr(object_header_addr, highestDataLevel);
            }
        }
    }
    else if(link_type == 1) // soft link
    {
        uint16_t soft_link_len = readField(2, &pos);
        uint8_t soft_link[STR_BUFF_SIZE];
        readByteArray(soft_link, soft_link_len, &pos);
        soft_link[soft_link_len] = '\0';
        if(verbose)
        {
            mlog(RAW, "Soft Link:                                                       %s\n", soft_link);
        }
    }
    else if(link_type == 64) // external link
    {
        uint16_t ext_link_len = readField(2, &pos);
        uint8_t ext_link[STR_BUFF_SIZE];
        readByteArray(ext_link, ext_link_len, &pos);
        ext_link[ext_link_len] = '\0';
        if(verbose)
        {
            mlog(RAW, "External Link:                                                   %s\n", ext_link);
        }
    }
    else if(errorChecking)
    {
        throw RunTimeException("invalid link type: %d", link_type);
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readDataLayoutMsg
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readDataLayoutMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    uint64_t starting_position = pos;

    /* Read Message Info */
    uint64_t version = readField(1, &pos);
    dataLayout = (layout_t)readField(1, &pos);

    if(errorChecking)
    {
        if(version != 3)
        {
            throw RunTimeException("invalid data layout version: %d", (int)version);
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Data Layout Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Version:                                                         %d\n", (int)version);
        mlog(RAW, "Layout:                                                          %d, %s\n", (int)dataLayout, layout2str(dataLayout));
    }

    /* Read Layout Classes */
    switch(dataLayout)
    {
        case COMPACT_LAYOUT:
        {
            dataSize = (uint16_t)readField(2, &pos);
            dataAddress = pos;
            pos += dataSize;
            break;
        }

        case CONTIGUOUS_LAYOUT:
        {
            dataAddress = readField(offsetSize, &pos);
            dataSize = readField(lengthSize, &pos);
            break;
        }

        case CHUNKED_LAYOUT:
        {
            /* Read Number of Dimensions */
            int chunk_num_dim = (int)readField(1, &pos) - 1; // dimensionality is plus one over actual number of dimensions
            chunk_num_dim = MIN(chunk_num_dim, MAX_NDIMS);
            if(errorChecking)
            {
                if(chunk_num_dim != dataNumDimensions)
                {
                    throw RunTimeException("number of chunk dimensions does not match data dimensions: %d != %d", chunk_num_dim, dataNumDimensions);
                }
            }

            /* Read Address of B-Tree */
            dataAddress = readField(offsetSize, &pos);

            /* Read Dimensions */
            uint64_t chunk_dim[MAX_NDIMS];
            if(chunk_num_dim > 0)
            {
                dataChunkElements = 1;
                for(int d = 0; d < chunk_num_dim; d++)
                {
                    chunk_dim[d] = (uint32_t)readField(4, &pos);
                    dataChunkElements *= chunk_dim[d];
                }
            }

            /* Read Size of Data Element */
            dataChunkElementSize = (int)readField(4, &pos);

            /* Display Data Attributes */
            if(verbose)
            {
                mlog(RAW, "Chunk Element Size:                                              %d\n", (int)dataChunkElementSize);
                mlog(RAW, "Number of Chunked Dimensions:                                    %d\n", (int)chunk_num_dim);
                for(int d = 0; d < dataNumDimensions; d++)
                {
                    mlog(RAW, "Chunk Dimension %d:                                               %d\n", d, (int)chunk_dim[d]);
                }
            }

            break;
        }

        default:
        {
            if(errorChecking)
            {
                throw RunTimeException("invalid data layout: %d", (int)dataLayout);
            }
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readFilterMsg
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readFilterMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    uint64_t starting_position = pos;

    /* Read Message Info */
    uint64_t version = readField(1, &pos);
    uint32_t num_filters = (uint32_t)readField(1, &pos);
    pos += 6; // move past reserved bytes

    if(errorChecking)
    {
        if(version != 1)
        {
            throw RunTimeException("invalid filter version: %d", (int)version);
        }
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Filter Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Version:                                                         %d\n", (int)version);
        mlog(RAW, "Number of Filters:                                               %d\n", (int)num_filters);
    }

    /* Read Filters */
    for(int f = 0; f < (int)num_filters; f++)
    {
        /* Read Filter Description */
        filter_t filter             = (filter_t)readField(2, &pos);
        uint16_t name_len           = (uint16_t)readField(2, &pos);
        uint16_t flags              = (uint16_t)readField(2, &pos);
        uint16_t num_parms          = (uint16_t)readField(2, &pos);

        /* Read Name */
        uint8_t filter_name[STR_BUFF_SIZE];
        readByteArray(filter_name, name_len, &pos);
        filter_name[name_len] = '\0';

        /* Display */
        if(verbose)
        {
            mlog(RAW, "Filter Identification Value:                                     %d\n", (int)filter);
            mlog(RAW, "Flags:                                                           0x%x\n", (int)flags);
            mlog(RAW, "Number Client Data Values:                                       %d\n", (int)num_parms);
            mlog(RAW, "Filter Name:                                                     %s\n", filter_name);
        }

        /* Set Filter */
        if(filter < NUM_FILTERS)
        {
            dataFilter[filter] = true;
            dataNumFilterParms[filter] = num_parms;
        }
        else
        {
            throw RunTimeException("invalid filter specified: %d\n", (int)filter);
        }

        /* Client Data */
        uint32_t client_data_size = dataNumFilterParms[filter] * 4;
        if(client_data_size)
        {
            dataFilterParms[filter] = new uint32_t [dataNumFilterParms[filter]];
            readByteArray((uint8_t*)dataFilterParms[filter], client_data_size, &pos);
        }
        else
        {
            pos += client_data_size;
        }

        /* Handle Padding */
        if(dataNumFilterParms[filter] % 2 == 1)
        {
            pos += 4;
        }
    }

    /* Return Bytes Read */
    uint64_t ending_position = pos;    
    return ending_position - starting_position;
}

/*----------------------------------------------------------------------------
 * readHeaderContMsg
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readHeaderContMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    uint64_t starting_position = pos;

    /* Continuation Info */
    uint64_t hc_offset = readField(offsetSize, &pos);
    uint64_t hc_length = readField(lengthSize, &pos);

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Header Continuation Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "Offset:                                                          0x%lx\n", (unsigned long)hc_offset);
        mlog(RAW, "Length:                                                          %lu\n", (unsigned long)hc_length);
    }

    /* Read Continuation Block */
    pos = hc_offset; // go to continuation block
    if(hdr_flags & H5LITE_CUSTOM_V1_FLAG)
    {
       uint64_t end_of_chdr = hc_offset + hc_length;
       pos += readMessagesV1 (pos, end_of_chdr, hdr_flags, dlvl);
    }
    else
    {
        /* Read Continuation Header */
        if(errorChecking)
        {
            uint64_t signature = readField(4, &pos);
            if(signature != H5_OCHK_SIGNATURE_LE)
            {
                throw RunTimeException("invalid header continuation signature: 0x%llX", (unsigned long long)signature);
            }
        }

        /* Read Continuation Header Messages */
        uint64_t end_of_chdr = hc_offset + hc_length - 4; // leave 4 bytes for checksum below
        pos += readMessages (pos, end_of_chdr, hdr_flags, dlvl);

        /* Verify Checksum */
        uint64_t check_sum = readField(4, &pos);
        if(errorChecking)
        {
            (void)check_sum;
        }
    }

    /* Return Bytes Read */
    return offsetSize + lengthSize;
}

/*----------------------------------------------------------------------------
 * readSymbolTableMsg
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::readSymbolTableMsg (uint64_t pos, uint8_t hdr_flags, int dlvl)
{
    (void)hdr_flags;

    uint64_t starting_position = pos;

    /* Symbol Table Info */
    uint64_t btree_addr = readField(offsetSize, &pos);
    uint64_t heap_addr = readField(offsetSize, &pos);

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Symbol Table Message [%d]: 0x%lx\n", dlvl, (unsigned long)starting_position);
        mlog(RAW, "----------------\n");
        mlog(RAW, "B-Tree Address:                                                  0x%lx\n", (unsigned long)btree_addr);
        mlog(RAW, "Heap Address:                                                    0x%lx\n", (unsigned long)heap_addr);
    }

    /* Read Heap Info */
    pos = heap_addr;
    if(!errorChecking)
    {
        pos += 24;
    }
    else
    {
        uint32_t signature = (uint32_t)readField(4, &pos);
        if(signature != H5_HEAP_SIGNATURE_LE)
        {
            throw RunTimeException("invalid heap signature: 0x%llX", (unsigned long long)signature);
        }            

        uint8_t version = (uint8_t)readField(1, &pos);
        if(version != 0)
        {
            throw RunTimeException("incorrect version of heap: %d", version);
        }

        pos += 19;
    }
    uint64_t head_data_addr = readField(offsetSize, &pos);

    /* Go to Left-Most Node */
    pos = btree_addr;
    while(true)
    {
        /* Read Header Info */
        if(!errorChecking)
        {
            pos += 5;
        }
        else
        {
            uint32_t signature = (uint32_t)readField(4, &pos);
            if(signature != H5_TREE_SIGNATURE_LE)
            {
                throw RunTimeException("invalid group b-tree signature: 0x%llX", (unsigned long long)signature);
            }            

            uint8_t node_type = (uint8_t)readField(1, &pos);
            if(node_type != 0)
            {
                throw RunTimeException("only group b-trees supported: %d", node_type);
            }
        }

        /* Read Branch Info */
        uint8_t node_level = (uint8_t)readField(1, &pos);
        if(node_level == 0)
        {
            break;
        }
        else
        {
            pos += 2 + (2 * offsetSize) + lengthSize; // skip entries used, sibling addresses, and first key
            pos = readField(offsetSize, &pos); // read and go to first child
        }
    }

    /* Traverse Children Left to Right */
    while(true)
    {
        uint16_t entries_used = (uint16_t)readField(2, &pos);
        uint64_t left_sibling = readField(offsetSize, &pos);
        uint64_t right_sibling = readField(offsetSize, &pos);
        uint64_t key0 = readField(lengthSize, &pos);
        if(verbose && H5_EXTRA_DEBUG)
        {
            mlog(RAW, "Entries Used:                                                    %d\n", (int)entries_used);
            mlog(RAW, "Left Sibling:                                                    0x%lx\n", (unsigned long)left_sibling);
            mlog(RAW, "Right Sibling:                                                   0x%lx\n", (unsigned long)right_sibling);
            mlog(RAW, "First Key:                                                       %ld\n", (unsigned long)key0);
        }

        /* Loop Through Entries in Current Node */
        for(int entry = 0; entry < entries_used; entry++)
        {
            uint64_t symbol_table_addr = readField(offsetSize, &pos);
            readSymbolTable(symbol_table_addr, head_data_addr, dlvl);
            pos += lengthSize; // skip next key;
            if(highestDataLevel > dlvl) break; // dataset found
        }

        /* Exit Loop or Go to Next Node */
        if(H5_INVALID(right_sibling))
        {
            break;
        }
        else
        {
            pos = right_sibling;
        }
    }

    /* Return Bytes Read */
    return offsetSize + offsetSize;
}

/*----------------------------------------------------------------------------
 * parseDataset
 *----------------------------------------------------------------------------*/
void H5Lite::H5FileBuffer::parseDataset (const char* _dataset)
{
    /* Create Copy of Dataset */
    dataset = StringLib::duplicate(_dataset);

    /* Get Pointer to First Group in Dataset */
    const char* gptr; // group pointer
    if(dataset[0] == '/')   gptr = &dataset[1];
    else                    gptr = &dataset[0];

    /* Build Path to Dataset */
    while(true)
    {        
        datasetPath.add(gptr);                      // add group to dataset path
        char* nptr = StringLib::find(gptr, '/');    // look for next group marker
        if(nptr == NULL) break;                     // if not found, then exit
        *nptr = '\0';                               // terminate group string
        gptr = nptr + 1;                            // go to start of next group
    }

    if(verbose)
    {
        mlog(RAW, "\n----------------\n");
        mlog(RAW, "Dataset: ");
        for(int g = 0; g < datasetPath.length(); g++)
        {
            mlog(RAW, "/%s", datasetPath[g]);
        }
        mlog(RAW, "\n----------------\n");
    }
}

/*----------------------------------------------------------------------------
 * type2str
 *----------------------------------------------------------------------------*/
const char* H5Lite::H5FileBuffer::type2str (data_type_t datatype)
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
const char* H5Lite::H5FileBuffer::layout2str (layout_t layout)
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
int H5Lite::H5FileBuffer::highestBit (uint64_t value)
{
    int bit = 0;
    while(value >>= 1) bit++;
    return bit;
}

/*----------------------------------------------------------------------------
 * inflateChunk
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::inflateChunk (uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_size)
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
        throw RunTimeException("failed to initialize z_stream: %d", status);
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
        throw RunTimeException("failed to inflate entire z_stream: %d", status);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * shuffleChunk
 *----------------------------------------------------------------------------*/
int H5Lite::H5FileBuffer::shuffleChunk (uint8_t* input, uint32_t input_size, uint8_t* output, uint32_t output_size, int type_size)
{
    if(errorChecking)
    {
        if(type_size < 0 || type_size > 8)
        {
            throw RunTimeException("invalid data size to perform shuffle on: %d\n", type_size);
        }
    }

    int64_t dst_index = 0;
    int64_t num_elements = input_size / type_size;
    int64_t shuffle_block_size = output_size / type_size;
    for(int element_index = 0; element_index < num_elements; element_index++)
    {
        for(int val_index = 0; val_index < type_size; val_index++)
        {
            int64_t src_index = (val_index * shuffle_block_size) + element_index;
            output[dst_index++] = input[src_index];
        }
    }

    return 0;
}
