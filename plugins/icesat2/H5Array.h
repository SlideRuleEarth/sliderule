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
        size_t          size;
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
    (void)col; // TODO: support slicing multidimensional arrays

    /* Initialize Class Attributes */
    name = StringLib::duplicate(_name);
    size = 1;
    data = NULL;

    /* Start with Invalid Handles */
    bool status = false;
    hid_t dataset = INVALID_RC;
    hid_t space = INVALID_RC;
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
        space = H5Dget_space(dataset);
        if(space < 0)
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

        /* Read Data */
        int ndims = H5Sget_simple_extent_ndims(space);
        hsize_t* dims = new hsize_t[ndims];
        H5Sget_simple_extent_dims(space, dims, NULL);

        /* Get Size of Data Buffer */
        for(int d = 0; d < ndims; d++)
        {
            size *= dims[d];
        }

        /* Allocate Data Buffer */
        try
        {
            data = new T[size];
        }
        catch (const std::bad_alloc& e)
        {
            mlog(CRITICAL, "Failed to allocate space for dataset: %d\n", (int)size);
            break;
        }

        /* Read Dataset */
        mlog(INFO, "Reading %d elements from %s\n", (int)size, name);
        if(H5Dread(dataset, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) >= 0)
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
    if(space > 0) H5Sclose(space);
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
