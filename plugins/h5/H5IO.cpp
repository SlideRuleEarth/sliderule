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

#include "H5IO.h"
#include "core.h"

#include <assert.h>
#include <stdexcept>

/******************************************************************************
 * HDF5 I/O CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
int H5IO::read (const char* url, const char* datasetname, int col, size_t datatypesize, void* data)
{
    int size = 0;

    /* Start with Invalid Handles */
    bool status = false;
    hid_t file = INVALID_RC;
    hid_t dataset = INVALID_RC;
    hid_t filespace = H5S_ALL;
    hid_t memspace = H5S_ALL;
    hid_t datatype = INVALID_RC;
    do
    {
        mlog(INFO, "Opening resource: %s\n", url);
        file = H5Fopen(url, H5F_ACC_RDONLY, H5P_DEFAULT);
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
        filespace = H5Dget_space(dataset);
        if(filespace < 0)
        {
            mlog(CRITICAL, "Failed to open dataspace on dataset: %s\n", datasetname);
            break;
        }

        /* Get Datatype */
        datatype = H5Dget_type(dataset);
        size_t typesize = H5Tget_size(datatype);
        if(typesize != datatypesize)
        {
            mlog(CRITICAL, "Incompatible type provided (%d != %d) for dataset: %s\n", (int)typesize, (int)datatypesize, datasetname);
            break;
        }

        /* Get Dimensions of Data */
        int ndims = H5Sget_simple_extent_ndims(filespace);
        hsize_t* dims = new hsize_t[ndims];
        H5Sget_simple_extent_dims(filespace, dims, NULL);

        /* Select Specific Column */
        if(col >= 0)
        {
            if(ndims == 2)
            {
                /* Allocate Hyperspace Parameters */
                hsize_t* start = new hsize_t[ndims];
                hsize_t* count = new hsize_t[ndims];

                /* Create File Hyperspace to Read Selected Column */
                start[0] = 0;
                start[1] = col;
                count[0] = dims[0];
                count[1] = 1;
                H5Sselect_hyperslab(filespace, H5S_SELECT_SET, start, NULL, count, NULL);

                /* Create Memory Hyperspace to Write Selected Column */
                dims[1] = 1; // readjust dimensions to reflect single column being read
                start[1] = 0; // readjust start to reflect writing to only a single column
                memspace = H5Screate_simple(ndims, dims, NULL);
                H5Sselect_hyperslab(memspace, H5S_SELECT_SET, start, NULL, count, NULL);

                /* Free Hyperspace Parameters */
                delete [] start;
                delete [] count;
            }
            else
            {
                mlog(CRITICAL, "Unsupported column selection on dataset of rank: %d\n", ndims);
            }
        }

        /* Get Size of Data Buffer */
        for(int d = 0; d < ndims; d++)
        {
            size *= (int32_t)dims[d];
        }

        /* Allocate Data Buffer */
        try
        {
            data = new unsigned char[datatypesize * size];
        }
        catch (const std::bad_alloc& e)
        {
            mlog(CRITICAL, "Failed to allocate space for dataset: %d\n", size);
            break;
        }

        /* Read Dataset */
        mlog(INFO, "Reading %d elements from %s\n", size, datasetname);
        if(H5Dread(dataset, datatype, memspace, filespace, H5P_DEFAULT, data) >= 0)
        {
            status = true;
        }
        else
        {
            mlog(CRITICAL, "Failed to read data from %s\n", datasetname);
            break;
        }
    }
    while(false);

    /* Clean Up */
    if(file > 0) H5Fclose(file);
    if(datatype > 0) H5Tclose(datatype);
    if(filespace != H5S_ALL) H5Sclose(filespace);
    if(memspace != H5S_ALL) H5Sclose(memspace);
    if(dataset > 0) H5Dclose(dataset);

    /* Throw Exception on Failure */
    if(status == false)
    {
        throw std::runtime_error("failed to initialize hdf5 array");
    }

    /* Return Size of Data (number of elements) */
    return size;
}