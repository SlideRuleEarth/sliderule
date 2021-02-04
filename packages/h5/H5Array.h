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

#ifndef __h5_array__
#define __h5_array__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "StringLib.h"
#include "LogLib.h"
#include "H5Proxy.h"
#include "H5Api.h"

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

                H5Array     (const char* url, const char* dataset, bool async=false, long col=0, long startrow=0, long numrows=H5Api::ALL_ROWS);
        virtual ~H5Array    (void);

        bool    trim        (long offset);
        T&      operator[]  (long index);
        bool    join        (int timeout);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*         name;
        long                size;
        T*                  data;
        T*                  pointer;
        H5Proxy::pending_t* pending;
};

/******************************************************************************
 * H5Array METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>::H5Array(const char* url, const char* dataset, bool async, long col, long startrow, long numrows)
{
    name = StringLib::duplicate(dataset);
    
    if(!async)
    {
        pending = NULL;
        H5Api::info_t info = H5Api::read(url, dataset, RecordObject::DYNAMIC, col, startrow, numrows);
        data = (T*)info.data;
        size = info.elements;
    }
    else /* async */
    {
        pending = H5Proxy::read(url, dataset, RecordObject::DYNAMIC, col, startrow, numrows);
        data = NULL;
        size = 0;
    }

    pointer = data;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>::~H5Array(void)
{
    if(name) delete [] name;
    if(data) delete [] data;
    if(pending)
    {
        delete pending->request;
        delete pending->response;
        delete pending;
    }
}

/*----------------------------------------------------------------------------
 * trim
 *----------------------------------------------------------------------------*/
template <class T>
bool H5Array<T>::trim(long offset)
{
    if((offset >= 0) && (offset < size))
    {
        pointer = data + offset;
        return true;
    }
    else
    {
        return false;
    }
}

/*----------------------------------------------------------------------------
 * []
 *----------------------------------------------------------------------------*/
template <class T>
T& H5Array<T>::operator[](long index)
{
    return pointer[index];
}

/*----------------------------------------------------------------------------
 * []
 *----------------------------------------------------------------------------*/
template <class T>
bool H5Array<T>::join(int timeout)
{
    /* Check if already joined */
    if(data != NULL)
    {
        return true;
    }

    /* Join */
    bool joined = join(pending, timeout);

    /* Populate */
    if(joined)
    {
        data = (T*)pending->response->data;
        size = pending->response->elements;
        pointer = data;
    }
}

#endif  /* __h5_array__ */
