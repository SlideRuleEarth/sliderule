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
#include "Asset.h"
#include "H5Coro.h"

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

                H5Array     (const Asset* asset, const char* resource, const char* dataset, H5Coro::context_t* context=NULL, long col=0, long startrow=0, long numrows=H5Coro::ALL_ROWS);
        virtual ~H5Array    (void);

        bool    trim        (long offset);
        T&      operator[]  (long index);
        bool    join        (int timeout, bool throw_exception);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*         name;
        H5Future*           h5f;
        long                size;
        T*                  data;
        T*                  pointer;
};

/******************************************************************************
 * H5Array METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  Note 1: the "name" class member (along with everthing else) is initialized
 *  after the call to H5Coro::read(...).  This is on purpose because the H5Coro::read
 *  can throw an exception.  It will clean up all memory it uses, but any memory
 *  allocated by H5Array will not get cleaned up.  If any memory is allocated
 *  before the H5Coro::read call, then the call would need to be in a try-except
 *  block and would have to handle cleaning up that memory and rethrowing the
 *  exception; the ordering of the statements below is preferred over that.
 *
 *  Note 2: the asset parameter is used to indicate that this should be a null
 *  array; it is the repsonsibility of the calling function to make sure no
 *  further calls to this class are performed on a null array other than a join
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>::H5Array(const Asset* asset, const char* resource, const char* dataset, H5Coro::context_t* context, long col, long startrow, long numrows)
{
    if(asset)   h5f = H5Coro::readp(asset, resource, dataset, RecordObject::DYNAMIC, col, startrow, numrows, context);
    else        h5f = NULL;

    name    = StringLib::duplicate(dataset);
    size    = 0;
    data    = NULL;
    pointer = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>::~H5Array(void)
{
    if(h5f)  delete h5f;
    if(name) delete [] name;
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
        size = size - offset;
        return true;
    }
    else
    {
        return false;
    }
}

/*----------------------------------------------------------------------------
 * []
 *
 *  Note: intentionally left unsafe for performance reasons
 *----------------------------------------------------------------------------*/
template <class T>
T& H5Array<T>::operator[](long index)
{
    return pointer[index];
}

/*----------------------------------------------------------------------------
 * join
 *----------------------------------------------------------------------------*/
template <class T>
bool H5Array<T>::join(int timeout, bool throw_exception)
{
    bool status;

    if(h5f)
    {
        H5Future::rc_t rc = h5f->wait(timeout);
        if(rc == H5Future::COMPLETE)
        {
            status = true;
            size = h5f->info.elements;
            data = (T*)h5f->info.data;
            pointer = data;
        }
        else
        {
            status = false;
            if(throw_exception)
            {
                switch(rc)
                {
                    case H5Future::INVALID: throw RunTimeException(CRITICAL, "H5Future read failure on %s", name);
                    case H5Future::TIMEOUT: throw RunTimeException(CRITICAL, "H5Future read timeout on %s", name);
                    default:                throw RunTimeException(CRITICAL, "H5Future unknown error on %s", name);
                }
            }
        }
    }
    else
    {
        status = false;
        if(throw_exception)
        {
            throw RunTimeException(CRITICAL, "H5Future null join on %s", name);
        }
    }

    return status;
}

#endif  /* __h5_array__ */
