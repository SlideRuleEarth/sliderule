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

#ifndef __h5_array__
#define __h5_array__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "StringLib.h"
#include "EventLib.h"
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

                H5Array     (const char* url, const char* dataset, H5Api::context_t* context=NULL, long col=0, long startrow=0, long numrows=H5Api::ALL_ROWS);
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
};

/******************************************************************************
 * H5Array METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>::H5Array(const char* url, const char* dataset, H5Api::context_t* context, long col, long startrow, long numrows)
{
    name = StringLib::duplicate(dataset);
    H5Api::info_t info = H5Api::read(url, dataset, RecordObject::DYNAMIC, col, startrow, numrows, context);
    data = (T*)info.data;
    size = info.elements;
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
    /* Currently Unimplemented */
    return true;
}

#endif  /* __h5_array__ */
