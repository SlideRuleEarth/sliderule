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
uint64_t get_field(uint8_t* buffer, int buffer_size, int field_offset, int field_size)
{
    if(field_offset + field_size >= buffer_size)
    {
        return 0;
    }

    switch(field_size)
    {
        case 8:     
        {
            uint64_t val = *(uint64_t*)&buffer[field_offset];
            return LocalLib::swapll(val);
        }

        case 4:     
        {
            uint32_t val = *(uint32_t*)&buffer[field_offset];
            return LocalLib::swapl(val);
        }

        case 2:
        {
            uint16_t val = *(uint16_t*)&buffer[field_offset];
            return LocalLib::swaps(val);
        }

        default:
        {
            return 0;
        }
    }
}

/******************************************************************************
 * HDF5 LIBRARY CLASS
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

        /* Read Dataset */
        status = true;
        mlog(CRITICAL, "Failed to read data from %s\n", datasetname);

        /* Stop Trace */
        stop_trace(trace_id);

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

    do
    {
        /* Initialize Recurse Data */
        rdepth_t recurse = {.data = 0};
        recurse.curr.max = max_depth;

        /* Initialize Driver */
        const char* resource = NULL;
        driver_t driver = H5Lite::parseUrl(url, &resource);
        if(driver == UNKNOWN)
        {
            mlog(CRITICAL, "Invalid url: %s\n", url);
            break;
        }

        /* Open File */
        fileptr_t file = fopen(resource, "r");
        if(file == NULL)
        {
            mlog(CRITICAL, "Failed to open resource: %s", url);
            break;
        }

        uint8_t buf[256];
        fread(buf, 1, 256, file);

        printf("Signature: ");
        for(int i = 0; i < 8; i++) printf("%02X", buf[i]);
        printf("\n\n");

        printf("Superblock Version #: %d\n", buf[8]);
        printf("File's Free Space Storage Version #: %d\n", buf[9]);
        printf("Root Group Symbol Table Entry Version #: %d\n", buf[10]);
        printf("Shared Header Message Version #: %d\n\n", buf[11]);

        uint64_t size_of_offsets = buf[12];
        uint64_t size_of_lengths = buf[13];
        printf("Size of Offsets: %lu\n", (unsigned long)size_of_offsets);
        printf("Size of Lengths: %lu\n\n", (unsigned long)size_of_lengths);

        printf("Group Leaf Node K: %d\n", buf[14]);
        printf("Group Internal Node K: %d\n\n", buf[15]);

        int curr_file_pos = 20;

        printf("Base Address: %lu\n", (unsigned long)get_field(buf, 256, curr_file_pos, size_of_offsets));
        curr_file_pos += size_of_offsets;

        printf("Address of File Free Space Info: %lu\n", (unsigned long)get_field(buf, 256, curr_file_pos, size_of_offsets));
        curr_file_pos += size_of_offsets;

        printf("End of File Address: %lu\n", (unsigned long)get_field(buf, 256, curr_file_pos, size_of_offsets));
        curr_file_pos += size_of_offsets;

        printf("Driver Info Block Address: %lu\n\n", (long unsigned)get_field(buf, 256, curr_file_pos, size_of_offsets));
        curr_file_pos += size_of_offsets;


        printf("Root Link Name Offset: %lu\n", (long unsigned)get_field(buf, 256, curr_file_pos, size_of_offsets));
        curr_file_pos += size_of_offsets;

        printf("Root Object Header Address: %lu\n", (long unsigned)get_field(buf, 256, curr_file_pos, size_of_offsets));
        curr_file_pos += size_of_offsets;

        printf("Root Object Header Address: %lu\n", (long unsigned)get_field(buf, 256, curr_file_pos, size_of_offsets));
        curr_file_pos += size_of_offsets;

        printf("Root Cache Type: %lu\n", (long unsigned)get_field(buf, 256, curr_file_pos, 4));
        curr_file_pos += 4;
    }
    while(false);

    /* Return Status */
    return status;
}
