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

/******************************************************************************
 * DEFINES
 ******************************************************************************/


/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

typedef union {
    struct {
        uint32_t depth;
        uint32_t max;
    } curr;
    uint64_t data;
} rdepth_t;

/******************************************************************************
 * FILE DATA
 ******************************************************************************/


/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * get_field
 *----------------------------------------------------------------------------*/
uint64_t get_field(uint8_t* buffer, int buffer_size, int* field_offset, int field_size)
{
    if(*field_offset + field_size >= buffer_size)
    {
        return 0;
    }

    switch(field_size)
    {
        case 8:     
        {
            uint64_t val = *(uint64_t*)&buffer[*field_offset];
            *field_offset += field_size;
            #ifdef __BE__
                return LocalLib::swapll(val);
            #else
                return val; 
            #endif
        }

        case 4:     
        {
            uint32_t val = *(uint32_t*)&buffer[*field_offset];
            *field_offset += field_size;
            #ifdef __BE__
                return LocalLib::swapl(val);
            #else
                return val; 
            #endif
        }

        case 2:
        {
            uint16_t val = *(uint16_t*)&buffer[*field_offset];
            *field_offset += field_size;
            #ifdef __BE__
                return LocalLib::swaps(val);
            #else
                return val; 
            #endif
        }

        default:
        {
            return 0;
        }
    }
}

/******************************************************************************
 * H5 FILE BUFFER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::H5FileBuffer (const char* filename, bool error_checking)
{
    /* Initialize */
    buffSize = 0;
    currFilePos = 0;
    currBuffPos = 0;
    offsetSize = 0;
    lengthSize = 0;
    groupLeafNodeK = 0;
    groupInternalNodeK = 0;
    rootGroupOffset = 0;

    /* Open File */
    fp = fopen(filename, "r");
    if(fp == NULL)
    {
        mlog(CRITICAL, "Failed to open filename: %s", filename);
        throw std::runtime_error("H5FileBuffer failed to open file");
    }

    /* Read and Verify Superblock Info */
    if(error_checking)
    {
        uint64_t signature = readNextField(8);
        if(signature != H5_SIGNATURE_LE)
        {
            mlog(CRITICAL, "Invalid h5 file signature: %llX\n", (unsigned long long)signature);
            throw std::runtime_error("H5FileBuffer invalid signature");
        }

        uint64_t superblock_version = readNextField(1);
        if(superblock_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file superblock version: %d\n", (int)superblock_version);
            throw std::runtime_error("H5FileBuffer invalid superblock version");
        }

        uint64_t freespace_version = readNextField(1);
        if(freespace_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file free space version: %d\n", (int)freespace_version);
            throw std::runtime_error("H5FileBuffer invalid free space version");
        }

        uint64_t roottable_version = readNextField(1);
        if(roottable_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file root table version: %d\n", (int)roottable_version);
            throw std::runtime_error("H5FileBuffer invalid root table version");
        }

        uint64_t headermsg_version = readNextField(1);
        if(headermsg_version != 0)
        {
            mlog(CRITICAL, "Invalid h5 file header message version: %d\n", (int)headermsg_version);
            throw std::runtime_error("H5FileBuffer invalid header message version");
        }
    }

    /* Read Size and Root Info */
    offsetSize          = readNextField(1, 13);
    lengthSize          = readNextField(1, 14);
    groupLeafNodeK      = readNextField(2, 16);
    groupInternalNodeK  = readNextField(2, 18);
    rootGroupOffset     = readNextField(USE_OFFSET_SIZE, 56);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5FileBuffer::~H5FileBuffer (void)
{
    fclose(fp);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
uint64_t H5FileBuffer::readNextField (int size, int64_t pos)
{
    /* Set Field Size */
    int field_size;
    if      (size == USE_OFFSET_SIZE)   field_size = offsetSize;
    else if (size == USE_LENGTH_SIZE)   field_size = lengthSize;
    else if (size > 0)                  field_size = size;
    else throw std::runtime_error("H5FileBuffer size of field cannot be negative");

    /* Set Field Position */
    int64_t field_position;
    if      (pos == USE_CURRENT_POSITION)   field_position = currFilePos + currBuffPos;
    else if (pos > 0)                       field_position = pos;
    else throw std::runtime_error("H5FileBuffer position of field cannot be negative");

    /* Check if Different Data Needs to be Buffered */
    if((field_position < currFilePos) || ((field_position + field_size) > (currFilePos + buffSize)))
    {
        if(fseek(fp, field_position, SEEK_SET) == 0)
        {
            buffSize = fread(buffer, 1, READ_BUFSIZE, fp);
            currBuffPos = 0;
            currFilePos = field_position;
        }
        else
        {
            throw std::runtime_error("H5FileBuffer failed to go to field position");
        }
    }

    /* Set Buffer Position */
    currBuffPos = field_position - currFilePos;

    /*  Read Field Value */
    uint64_t value;
    switch(field_size)
    {
        case 8:     
        {
            value = *(uint64_t*)&buffer[currBuffPos];
            #ifdef __BE__
                value = LocalLib::swapll(value);
            #endif
            break;
        }

        case 4:     
        {
            value = *(uint32_t*)&buffer[currBuffPos];
            #ifdef __BE__
                value = LocalLib::swapl(value);
            #endif
            break;
        }

        case 2:
        {
            value = *(uint16_t*)&buffer[currBuffPos];
            #ifdef __BE__
                value = LocalLib::swaps(value);
            #endif
            break;
        }

        case 1:
        {
            value = buffer[currBuffPos];
            break;
        }

        default:
        {
            throw std::runtime_error("H5FileBuffer invalid field size");;
        }
    }

    /* Increment Current Buffer Position */
    currBuffPos += field_size;

    /* Return Field Value */
    return value;
}

/*----------------------------------------------------------------------------
 * displayFileInfo
 *----------------------------------------------------------------------------*/
void H5FileBuffer::displayFileInfo (void)
{
    printf("Size of Offsets:                            %lu\n",     (unsigned long)offsetSize);
    printf("Size of Lengths:                            %lu\n",     (unsigned long)lengthSize);
    printf("Group Leaf Node K:                          %lu\n",     (unsigned long)groupLeafNodeK);
    printf("Group Internal Node K:                      %lu\n\n",   (unsigned long)groupInternalNodeK);
    printf("Root Object Header Address:                 0x%lX\n",   (long unsigned)rootGroupOffset);
}

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
    info_t info;
    bool status = false;

    do
    {
        /* Initialize Driver */
        const char* resource = NULL;
        driver_t driver = H5Lite::parseUrl(url, &resource);
        if(driver == UNKNOWN)
        {
            mlog(CRITICAL, "Invalid url: %s\n", url);
            break;
        }

        /* Open Resource */
        fileptr_t file = NULL; //H5Fopen(resource, H5F_ACC_RDONLY, fapl);
        if(file == NULL)
        {
            mlog(CRITICAL, "Failed to open resource: %s\n", url);
            break;
        }

        /* Open Dataset */
        int dataset = 0; // H5Dopen(file, datasetname, H5P_DEFAULT);
        if(dataset < 0)
        {
            mlog(CRITICAL, "Failed to open dataset: %s\n", datasetname);
            break;
        }

        /* Set Info */
        uint8_t* data = NULL;
        int elements = 0;
        int typesize = 0;
        long datasize = 0;

        /* Start Trace */
        mlog(INFO, "Reading %d elements (%ld bytes) from %s %s\n", elements, datasize, url, datasetname);
        uint32_t parent_trace_id = TraceLib::grabId();
        uint32_t trace_id = start_trace_ext(parent_trace_id, "h5lite_read", "{\"url\":\"%s\", \"dataset\":\"%s\"}", url, datasetname);

        /* Stop Trace */
        stop_trace(trace_id);

        /* Read Dataset */
        status = true;
        mlog(CRITICAL, "Failed to read data from %s\n", datasetname);


        /* Return Info */
        info.elements = elements;
        info.typesize = typesize;
        info.datasize = datasize;
        info.data = data;
    }
    while(false);


    /* Return Info */
    if(status)  return info;
    else        throw std::runtime_error("H5Lite");
}

/*----------------------------------------------------------------------------
 * traverse
 *----------------------------------------------------------------------------*/
bool H5Lite::traverse (const char* url, int max_depth, const char* start_group)
{
    bool status = false;

    try
    {
        /* Initialize Recurse Data */
        rdepth_t recurse = {.data = 0};
        recurse.curr.max = max_depth;

        /* Initialize Driver */
        const char* resource = NULL;
        driver_t driver = H5Lite::parseUrl(url, &resource);
        if(driver == UNKNOWN)
        {
            throw std::runtime_error("Invalid url");
        }

        /* Open File */
        H5FileBuffer h5file(resource);
        h5file.displayFileInfo();
    }
    catch (const std::exception &e)
    {
        mlog(CRITICAL, "Failed to traverse resource: %s\n", e.what());
    }

    /* Return Status */
    return status;
}
