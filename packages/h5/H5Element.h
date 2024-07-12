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

#ifndef __h5_element__
#define __h5_element__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "StringLib.h"
#include "EventLib.h"
#include "Asset.h"
#include "H5Coro.h"

/******************************************************************************
 * FILE SCOPE FUNCTIONS
 ******************************************************************************/

template <class T>
void setDataIfPointer(const T* t, const uint8_t* buffer) { (void)t; (void)buffer; }

template <class T>
void setDataIfPointer(T** t, const uint8_t* buffer) { *t = reinterpret_cast<T*>(buffer); }

/******************************************************************************
 * H5Element TEMPLATE
 ******************************************************************************/

template <class T>
class H5Element
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                H5Element   (H5Coro::Context* context, const char* variable);
        virtual ~H5Element  (void);

        bool    join        (int timeout, bool throw_exception);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        H5Coro::Future*     h5f;
        T                   value;
        int                 size; // in bytes
};

/******************************************************************************
 * H5Element METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Element<T>::H5Element(H5Coro::Context* context, const char* variable)
{
    memset(&value, 0, sizeof(T));
    size = 0;
    H5Coro::range_t slice[1] = {{0, H5Coro::EOR}};
    if(context) h5f = H5Coro::readp(context, variable, RecordObject::DYNAMIC, slice, 1);
    else        h5f = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
H5Element<T>::~H5Element(void)
{
    delete h5f;
}

/*----------------------------------------------------------------------------
 * join
 *----------------------------------------------------------------------------*/
template <class T>
bool H5Element<T>::join(int timeout, bool throw_exception)
{
    bool status;

    if(h5f)
    {
        const H5Coro::Future::rc_t rc = h5f->wait(timeout);
        if(rc == H5Coro::Future::COMPLETE)
        {
            status = true;
            if(h5f->info.datatype == RecordObject::STRING)
            {
                /*
                 * T is here assumed to be `const char*` or something analogous.
                 * Casting the raw data blindly to a C-like string is provided as a
                 * convenience for working directly with HDF5 data in C/C++ but is
                 * admittedly very unsafe. It is the responsibility of the calling code
                 * to know that the element being read is a string.
                 */
                setDataIfPointer(&value, h5f->info.data);
            }
            else
            {
                /*
                * There is no way to do this in a portable "safe" way. The code
                * below works because our target architectures are x86_64 and aarch64.
                * The data pointed to by info.data is new'ed and therefore guaranteed
                * to be aligned to a 16 byte boundary.  The calling code is responsible
                * for knowing what the data being read out of the h5 file is and
                * providing the correct type to the template.
                */
                const T* value_ptr = reinterpret_cast<T*>(h5f->info.data);
                value = *value_ptr;
            }
            size = h5f->info.datasize;
        }
        else
        {
            status = false;
            if(throw_exception)
            {
                switch(rc)
                {
                    case H5Coro::Future::INVALID: throw RunTimeException(ERROR, RTE_ERROR, "H5Coro::Future read failure");
                    case H5Coro::Future::TIMEOUT: throw RunTimeException(ERROR, RTE_TIMEOUT, "H5Coro::Future read timeout");
                    default:                throw RunTimeException(ERROR, RTE_ERROR, "H5Coro::Future unknown error");
                }
            }
        }
    }
    else
    {
        status = false;
        if(throw_exception)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "H5Coro::Future null join");
        }
    }

    return status;
}

#endif  /* __h5_element__ */
