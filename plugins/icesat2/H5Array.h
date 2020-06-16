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

#ifndef __h5array__
#define __h5array__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <hdf5.h>
#include "StringLib.h"
#include "LogLib.h"

#include <stdlib.h>
#include <assert.h>
#include <stdexcept>

/******************************************************************************
 * H5Array TEMPLATE
 ******************************************************************************/

template <class T>
class H5Array
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                H5Array     (hid_t file, const char* _name, int col=-1);
        virtual ~H5Array    (void);

        T&      operator[]  (int index);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*     name;
        int32_t         size;
        T*              data;
};

/******************************************************************************
 * H5Array METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>::H5Array(hid_t file, const char* _name, int col)
{
    /* Initialize Class Attributes */
    name = StringLib::duplicate(_name);
    size = 1;
    data = NULL;

    /* Start with Invalid Handles */
    bool status = false;
    hid_t dataset = INVALID_RC;
    hid_t filespace = H5S_ALL;
    hid_t memspace = H5S_ALL;
    hid_t datatype = INVALID_RC;
    do
    {
        /* Open Dataset */
        dataset = H5Dopen(file, name, H5P_DEFAULT);
        if(dataset < 0)
        {
            mlog(CRITICAL, "Failed to open dataset: %s\n", name);
            break;
        }

        /* Open Dataspace */
        filespace = H5Dget_space(dataset);
        if(filespace < 0)
        {
            mlog(CRITICAL, "Failed to open dataspace on dataset: %s\n", name);
            break;
        }

        /* Get Datatype */
        datatype = H5Dget_type(dataset);
        size_t typesize = H5Tget_size(datatype);
        if(typesize != sizeof(T))
        {
            mlog(CRITICAL, "Incompatible type provided (%d != %d) for dataset: %s\n", (int)typesize, (int)sizeof(T), name);
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
            data = new T[size];
        }
        catch (const std::bad_alloc& e)
        {
            mlog(CRITICAL, "Failed to allocate space for dataset: %d\n", size);
            break;
        }

        /* Read Dataset */
        mlog(INFO, "Reading %d elements from %s\n", size, name);
        if(H5Dread(dataset, datatype, memspace, filespace, H5P_DEFAULT, data) >= 0)
        {
            status = true;
        }
        else
        {
            mlog(CRITICAL, "Failed to read data from %s\n", name);
            break;
        }
    }
    while(false);

    /* Clean Up */
    if(datatype > 0) H5Tclose(datatype);
    if(filespace != H5S_ALL) H5Sclose(filespace);
    if(memspace != H5S_ALL) H5Sclose(memspace);
    if(dataset > 0) H5Dclose(dataset);

    /* Throw Exception on Failure */
    if(status == false)
    {
        throw std::runtime_error("failed to initialize hdf5 array");
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>::~H5Array(void)
{
    if(name) delete [] name;
    if(data) delete [] data;
}

/*----------------------------------------------------------------------------
 * []]
 *----------------------------------------------------------------------------*/
template <class T>
T& H5Array<T>::operator[](int index)
{
    return data[index];
}

#endif  /* __h5array__ */
