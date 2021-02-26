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

#include <hdf5.h>
#ifdef H5_USE_REST_VOL
#include <rest_vol_public.h>
#endif

#ifdef __aws__
#include "S3Lib.h"
#endif

#include "H5Lib.h"
#include "core.h"

#include <assert.h>
#include <stdexcept>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#ifdef H5_USE_REST_VOL
#ifndef DEFAULT_HSDS_ENDPOINT
#define DEFAULT_HSDS_ENDPOINT   "http://localhost"
#endif

#ifndef DEFAULT_HSDS_USERNAME
#define DEFAULT_HSDS_USERNAME   "username"
#endif

#ifndef DEFAULT_HSDS_PASSWORD
#define DEFAULT_HSDS_PASSWORD   "password"
#endif
#endif

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

hid_t rest_vol_fapl = H5P_DEFAULT;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * parse_url
 *----------------------------------------------------------------------------*/
H5Lib::driver_t parse_url (const char* url, const char** resource)
{
    /* Sanity Check Input */
    if(!url) return H5Lib::UNKNOWN;

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
        return H5Lib::FILE;
    }
    else if(StringLib::find(url, "s3://"))
    {
        return H5Lib::S3;
    }
    else if(StringLib::find(url, "hsds://"))    
    {
        return H5Lib::HSDS;
    }
    else
    {
        return H5Lib::UNKNOWN;
    }
}

/*----------------------------------------------------------------------------
 * url2driver
 *----------------------------------------------------------------------------*/
H5Lib::driver_t url2driver (const char* url, const char** resource, hid_t* fapl)
{
    H5Lib::driver_t driver = parse_url(url, resource);

    switch(driver)
    {
        case H5Lib::FILE:
        {
            *fapl = H5P_DEFAULT;
            break;
        }
        case H5Lib::S3:
        {
            driver = H5Lib::UNKNOWN; // reset it back to unknown, conditioned on below
            #ifdef __aws__
                const char* key_ptr = StringLib::find(*resource, PATH_DELIMETER);
                if(key_ptr)
                {
                    SafeString bucket("%s", *resource);
                    bucket.setChar('\0', bucket.findChar(PATH_DELIMETER));
                    if(S3Lib::get(bucket.getString(), key_ptr + 1, resource))
                    {
                        *fapl = H5P_DEFAULT;
                        return H5Lib::S3;
                    }
                }
            #endif
            break;
        }
        case H5Lib::HSDS:
        {
            *fapl = rest_vol_fapl;
            break;
        }
        case H5Lib::UNKNOWN:
        {
            break;
        }
    }

    return driver;
}

/*----------------------------------------------------------------------------
 * hdf5_iter_op_func
 *----------------------------------------------------------------------------*/
herr_t hdf5_iter_op_func (hid_t loc_id, const char* name, const H5L_info_t* info, void* operator_data)
{
    (void)info;

    herr_t      retval = 0;
    rdepth_t    recurse = { .data = (uint64_t)operator_data };

    for(unsigned i = 0; i < recurse.curr.depth; i++) mlog(RAW, "  ");

    H5O_info_t object_info;
    H5Oget_info_by_name(loc_id, name, &object_info, H5P_DEFAULT);
    switch (object_info.type)
    {
        case H5O_TYPE_GROUP:
        {
            H5L_info_t link_info;
            H5Lget_info(loc_id, name, &link_info, H5P_DEFAULT);
            if(link_info.type == H5L_TYPE_HARD)
            {
                mlog(RAW, "%s: {", name);
                recurse.curr.depth++;
                if(recurse.curr.depth < recurse.curr.max)
                {
                    mlog(RAW, "\n");
                    retval = H5Literate_by_name(loc_id, name, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, hdf5_iter_op_func, (void*)recurse.data, H5P_DEFAULT);
                    for(unsigned i = 0; i < recurse.curr.depth - 1; i++) mlog(RAW, "  ");
                    mlog(RAW, "}\n");
                }
                else
                {
                    mlog(RAW, " }\n");
                }
            }
            else
            {
                mlog(RAW, "*%s\n", name);
            }
            break;
        }
        case H5O_TYPE_DATASET:
        {
            mlog(RAW, "%s\n", name);
            break;
        }
        case H5O_TYPE_NAMED_DATATYPE:
        {
            mlog(RAW, "%s (type)\n", name);
            break;
        }
        default:
        {
            mlog(RAW, "%s (unknown)\n", name);
        }
    }

    return retval;
}

/******************************************************************************
 * HDF5 LIBRARY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5Lib::init (void)
{
    #ifdef H5_USE_REST_VOL
        setenv("HSDS_ENDPOINT", DEFAULT_HSDS_ENDPOINT, 0);
        setenv("HSDS_USERNAME", DEFAULT_HSDS_USERNAME, 0);
        setenv("HSDS_PASSWORD", DEFAULT_HSDS_PASSWORD, 0);

        H5rest_init();
        rest_vol_fapl = H5Pcreate(H5P_FILE_ACCESS);
        H5Pset_fapl_rest_vol(rest_vol_fapl);
    #endif
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void H5Lib::deinit (void)
{
    #ifdef H5_USE_REST_VOL
        H5Pclose(rest_vol_fapl);
        H5rest_term();
    #endif
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
H5Lib::info_t H5Lib::read (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows, context_t* context)
{
    (void)context;
    
    info_t info;
    bool status = false;

    /* Start with Invalid Handles */
    const char* resource = NULL;
    hid_t fapl = H5P_DEFAULT;
    hid_t file = INVALID_RC;
    hid_t dataset = INVALID_RC;
    hid_t memspace = H5S_ALL;
    hid_t dataspace = H5S_ALL;
    hid_t datatype = INVALID_RC;
    bool datatype_allocated = false;

    do
    {
        /* Initialize Driver */
        driver_t driver = url2driver(url, &resource, &fapl);
        if(driver == UNKNOWN)
        {
            mlog(CRITICAL, "Invalid url: %s\n", url);
            break;
        }

        /* Open Resource */
        file = H5Fopen(resource, H5F_ACC_RDONLY, fapl);
        if(file < 0)
        {
            mlog(CRITICAL, "Failed to open resource: %s\n", url);
            break;
        }

        /* Open Dataset */
        dataset = H5Dopen(file, datasetname, H5P_DEFAULT);
        if(dataset < 0)
        {
            mlog(CRITICAL, "Failed to open dataset: %s\n", datasetname);
            break;
        }

        /* Open Dataspace */
        dataspace = H5Dget_space(dataset);
        if(dataspace < 0)
        {
            mlog(CRITICAL, "Failed to open dataspace on dataset: %s\n", datasetname);
            break;
        }

        /* Get Datatype */
        if(valtype == RecordObject::INTEGER)
        {
            datatype = H5T_NATIVE_INT;
        }
        else if(valtype == RecordObject::REAL)
        {
            datatype = H5T_NATIVE_DOUBLE;
        }
        else
        {
            datatype = H5Dget_type(dataset);
            datatype_allocated = true;
        }

        /* Get Type Size */
        size_t typesize = H5Tget_size(datatype);

        /* Get Dimensions of Data */
        int ndims = H5Sget_simple_extent_ndims(dataspace);
        hsize_t* dims = new hsize_t[ndims + 1];
        H5Sget_simple_extent_dims(dataspace, dims, NULL);

        /* Allocate Hyperspace Parameters */
        hsize_t* start = new hsize_t[ndims + 1];
        hsize_t* count = new hsize_t[ndims + 1];

        /* Readjust First Dimension */
        if(numrows != ALL_ROWS) dims[0] = MIN(numrows, (long)dims[0]);

        /* Create File Hyperspace to Read Selected Column */
        start[0] = startrow;
        start[1] = col;
        count[0] = dims[0];
        count[1] = 1;
        H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, start, NULL, count, NULL);

        /* Create Memory Hyperspace to Write Selected Column */
        dims[1] = 1; // readjust dimensions to reflect single column being read
        start[0] = 0; // readjust start to reflect writing from the beginning
        start[1] = 0; // readjust start to reflect writing to only a single column
        memspace = H5Screate_simple(ndims, dims, NULL);
        H5Sselect_hyperslab(memspace, H5S_SELECT_SET, start, NULL, count, NULL);

        /* Get Number of Elements */
        int elements = 1;
        for(int d = 0; d < ndims; d++)
        {
            elements *= (int32_t)dims[d];
        }

        /* Free Hyperspace Parameters */
        delete [] dims;
        delete [] start;
        delete [] count;

        /* Get Size of Data */
        long datasize = elements * typesize;

        /* Allocate Data Buffer */
        uint8_t* data = NULL;
        try
        {
            data = new uint8_t[datasize];
        }
        catch (const std::bad_alloc& e)
        {
            mlog(CRITICAL, "Failed to allocate space for dataset: %d\n", elements);
            break;
        }

        /* Start Trace */
        mlog(INFO, "Reading %d elements (%ld bytes) from %s %s\n", elements, datasize, url, datasetname);
        uint32_t parent_trace_id = TraceLib::grabId();
        uint32_t trace_id = start_trace_ext(parent_trace_id, "h5lib_read", "{\"url\":\"%s\", \"dataset\":\"%s\"}", url, datasetname);

        /* Read Dataset */
        if(H5Dread(dataset, datatype, memspace, dataspace, H5P_DEFAULT, data) >= 0)
        {
            /* Set Success */
            status = true;

            /* Increment Stats */
            context->bytes_read += datasize;
            context->read_rqsts++;

            /* Set Info Return Structure */
            info.elements = elements;
            info.typesize = typesize;
            info.datasize = datasize;
            info.data = data;
        }
        else
        {
            /* Free Data and Log Error */
            mlog(CRITICAL, "Failed to read data from %s\n", datasetname);
            delete [] data;
        }

        /* Stop Trace */
        stop_trace(trace_id);
    }
    while(false);

    /* Clean Up */
    if(file > 0) H5Fclose(file);
    if(datatype_allocated && datatype > 0) H5Tclose(datatype);
    if(dataspace != H5S_ALL) H5Sclose(dataspace);
    if(memspace != H5S_ALL) H5Sclose(memspace);
    if(dataset > 0) H5Dclose(dataset);

    /* Return Info */
    if(status)  return info;
    else        throw RunTimeException("H5Lib failed to read dataset");
}

/*----------------------------------------------------------------------------
 * traverse
 *----------------------------------------------------------------------------*/
bool H5Lib::traverse (const char* url, int max_depth, const char* start_group)
{
    bool        status = false;
    hid_t       file = INVALID_RC;
    hid_t       group = INVALID_RC;
    hid_t       fapl = H5P_DEFAULT;
    const char* resource = NULL;

    do
    {
        /* Initialize Recurse Data */
        rdepth_t recurse = {.data = 0};
        recurse.curr.max = max_depth;

        /* Initialize Driver */
        driver_t driver = url2driver(url, &resource, &fapl);
        if(driver == UNKNOWN)
        {
            mlog(CRITICAL, "Invalid url: %s\n", url);
            break;
        }

        /* Open File */
        file = H5Fopen(resource, H5F_ACC_RDONLY, fapl);
        if(file < 0)
        {
            mlog(CRITICAL, "Failed to open resource: %s", url);
            break;
        }

        /* Open Group */
        if(start_group)
        {
            group = H5Gopen(file, start_group, H5P_DEFAULT);
            if(group < 0)
            {
                mlog(CRITICAL, "Failed to open group: %s", start_group);
                break;
            }
        }

        /* Display File Structure */
        if(H5Literate(group > 0 ? group : file, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, hdf5_iter_op_func, (void*)recurse.data) >= 0)
        {
            status = true;
        }
    }
    while(false);

    /* Clean Up */
    if(file > 0) H5Fclose(file);
    if(group > 0) H5Gclose(group);

    /* Return Status */
    return status;
}
