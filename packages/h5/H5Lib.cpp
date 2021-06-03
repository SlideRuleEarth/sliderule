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

#include <hdf5.h>
#ifdef H5_USE_REST_VOL
#include <rest_vol_public.h>
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
 * get_file_property
 *----------------------------------------------------------------------------*/
hid_t get_file_property (const Asset* asset)
{
    if(StringLib::match(asset->getFormat(), "file"))        return H5P_DEFAULT;
    else if(StringLib::match(asset->getFormat(), "hsds"))   return rest_vol_fapl;
    else                                                    return H5P_DEFAULT;
}

/*----------------------------------------------------------------------------
 * hdf5_iter_op_func
 *----------------------------------------------------------------------------*/
herr_t hdf5_iter_op_func (hid_t loc_id, const char* name, const H5L_info_t* info, void* operator_data)
{
    (void)info;

    herr_t      retval = 0;
    rdepth_t    recurse = { .data = (uint64_t)operator_data };

    for(unsigned i = 0; i < recurse.curr.depth; i++) print2term("  ");

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
                print2term("%s: {", name);
                recurse.curr.depth++;
                if(recurse.curr.depth < recurse.curr.max)
                {
                    print2term("\n");
                    retval = H5Literate_by_name(loc_id, name, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, hdf5_iter_op_func, (void*)recurse.data, H5P_DEFAULT);
                    for(unsigned i = 0; i < recurse.curr.depth - 1; i++) print2term("  ");
                    print2term("}\n");
                }
                else
                {
                    print2term(" }\n");
                }
            }
            else
            {
                print2term("*%s\n", name);
            }
            break;
        }
        case H5O_TYPE_DATASET:
        {
            print2term("%s\n", name);
            break;
        }
        case H5O_TYPE_NAMED_DATATYPE:
        {
            print2term("%s (type)\n", name);
            break;
        }
        default:
        {
            print2term("%s (unknown)\n", name);
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
H5Lib::info_t H5Lib::read (const Asset* asset, const char* resource, const char* datasetname, valtype_t valtype, long col, long startrow, long numrows, context_t* context)
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
        /* Open Resource */
        fapl = get_file_property(asset);
        file = H5Fopen(resource, H5F_ACC_RDONLY, fapl);
        if(file < 0)
        {
            mlog(CRITICAL, "Failed to open resource: %s", url);
            break;
        }

        /* Open Dataset */
        dataset = H5Dopen(file, datasetname, H5P_DEFAULT);
        if(dataset < 0)
        {
            mlog(CRITICAL, "Failed to open dataset: %s", datasetname);
            break;
        }

        /* Open Dataspace */
        dataspace = H5Dget_space(dataset);
        if(dataspace < 0)
        {
            mlog(CRITICAL, "Failed to open dataspace on dataset: %s", datasetname);
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
            mlog(CRITICAL, "Failed to allocate space for dataset: %d", elements);
            break;
        }

        /* Start Trace */
        mlog(INFO, "Reading %d elements (%ld bytes) from %s %s", elements, datasize, resource, datasetname);
        uint32_t parent_trace_id = EventLib::grabId();
        uint32_t trace_id = start_trace(INFO, parent_trace_id, "h5lib_read", "{\"resource\":\"%s\", \"dataset\":\"%s\"}", resource, datasetname);

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
            info.datatype = h5type2datatype(datatype, typesize);
            info.data = data;
        }
        else
        {
            /* Free Data and Log Error */
            mlog(CRITICAL, "Failed to read data from %s", datasetname);
            delete [] data;
        }

        /* Stop Trace */
        stop_trace(INFO, trace_id);
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
    else        throw RunTimeException(CRITICAL, "H5Lib failed to read dataset");
}

/*----------------------------------------------------------------------------
 * traverse
 *----------------------------------------------------------------------------*/
bool H5Lib::traverse (const Asset* asset, const char* resource, int max_depth, const char* start_group)
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

        /* Open File */
        fapl = get_file_property(asset);
        file = H5Fopen(resource, H5F_ACC_RDONLY, fapl);
        if(file < 0)
        {
            mlog(CRITICAL, "Failed to open resource: %s", resource);
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

/*----------------------------------------------------------------------------
 * h5type2datatype
 *----------------------------------------------------------------------------*/
H5Lib::datatype_t H5Lib::h5type2datatype (int h5type, int typesize)
{
    if(h5type == FIXED_POINT_TYPE || h5type == H5T_NATIVE_INT)
    {
        if      (typesize == 1) return RecordObject::UINT8;
        else if (typesize == 2) return RecordObject::UINT16;
        else if (typesize == 4) return RecordObject::UINT32;
        else if (typesize == 8) return RecordObject::UINT64;
    }
    else if(h5type == FLOATING_POINT_TYPE || h5type == H5T_NATIVE_DOUBLE)
    {
        if      (typesize == 4) return RecordObject::FLOAT;
        else if (typesize == 8) return RecordObject::DOUBLE;
    }

    return RecordObject::INVALID_FIELD;
}