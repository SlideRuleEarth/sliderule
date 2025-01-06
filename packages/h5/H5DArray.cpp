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

#include "OsApi.h"
#include "H5Coro.h"
#include "H5DArray.h"

/******************************************************************************
 * H5 DYNAMIC ARRAY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5DArray::init(void)
{
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5DArray::H5DArray(H5Coro::Context* context, const char* dataset, long col, long startrow, long numrows)
{
    H5Coro::range_t slice[2] = COLUMN_SLICE(col, startrow, numrows);
    if(context) h5f = H5Coro::readp(context, dataset, RecordObject::DYNAMIC, slice, 2);
    else        h5f = NULL;

    name    = StringLib::duplicate(dataset);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5DArray::~H5DArray(void)
{
    delete h5f;
    delete [] name;
}

/*----------------------------------------------------------------------------
 * join
 *----------------------------------------------------------------------------*/
bool H5DArray::join(int timeout, bool throw_exception) const
{
    bool status;

    if(h5f)
    {
        const H5Coro::Future::rc_t rc = h5f->wait(timeout);
        if(rc == H5Coro::Future::COMPLETE)
        {
            status = true;
        }
        else
        {
            status = false;
            if(throw_exception)
            {
                switch(rc)
                {
                    case H5Coro::Future::INVALID:   throw RunTimeException(ERROR, RTE_ERROR, "H5Coro::Future read failure on %s", name);
                    case H5Coro::Future::TIMEOUT:   throw RunTimeException(ERROR, RTE_TIMEOUT, "H5Coro::Future read timeout on %s", name);
                    default:                        throw RunTimeException(ERROR, RTE_ERROR, "H5Coro::Future unknown error on %s", name);
                }
            }
        }
    }
    else
    {
        status = false;
        if(throw_exception)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "H5Coro::Future null join on %s", name);
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * numDimensions
 *----------------------------------------------------------------------------*/
int H5DArray::numDimensions (void) const
{
    int ndims = 0;
    while(h5f->info.shape[ndims] != 0) ndims++;
    return ndims;
}

/*----------------------------------------------------------------------------
 * numElements
 *----------------------------------------------------------------------------*/
int H5DArray::numElements (void) const
{
    return h5f->info.elements;
}

/*----------------------------------------------------------------------------
 * elementSize
 *----------------------------------------------------------------------------*/
int H5DArray::elementSize (void) const
{
    return h5f->info.typesize;
}

/*----------------------------------------------------------------------------
 * elementType
 *----------------------------------------------------------------------------*/
H5DArray::type_t H5DArray::elementType (void) const
{
    return h5f->info.datatype;
}

/*----------------------------------------------------------------------------
 * rowSize
 *----------------------------------------------------------------------------*/
int64_t H5DArray::rowSize (void) const
{
    int64_t row_size = 1;
    for(int i = 1; i < H5Coro::MAX_NDIMS; i++)
    {
        if(h5f->info.shape[i] == 0)
        {
            break;
        }
        row_size *= h5f->info.shape[i];
    }
    return row_size;
}

/*----------------------------------------------------------------------------
 * serialize
 *----------------------------------------------------------------------------*/
uint64_t H5DArray::serialize (uint8_t* buffer, int64_t start_element, int64_t num_elements) const
{
    /* Serialize Elements of Array */
    if(h5f->info.typesize == 8)
    {
        const uint64_t* src = reinterpret_cast<const uint64_t*>(h5f->info.data);
        uint64_t* dst = reinterpret_cast<uint64_t*>(buffer);
        for(int64_t i = start_element; (i < h5f->info.elements) && (i < (start_element + num_elements)); i++)
        {
            *dst++ = src[i];
        }
    }
    else if(h5f->info.typesize == 4)
    {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(h5f->info.data);
        uint32_t* dst = reinterpret_cast<uint32_t*>(buffer);
        for(int64_t i = start_element; (i < h5f->info.elements) && (i < (start_element + num_elements)); i++)
        {
            *dst++ = src[i];
        }
    }
    else if(h5f->info.typesize == 2)
    {
        const uint16_t* src = reinterpret_cast<const uint16_t*>(h5f->info.data);
        uint16_t* dst = reinterpret_cast<uint16_t*>(buffer);
        for(int64_t i = start_element; (i < h5f->info.elements) && (i < (start_element + num_elements)); i++)
        {
            *dst++ = src[i];
        }
    }
    else if(h5f->info.typesize == 1)
    {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(h5f->info.data);
        uint8_t* dst = buffer;
        for(int64_t i = start_element; (i < h5f->info.elements) && (i < (start_element + num_elements)); i++)
        {
            *dst++ = src[i];
        }
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid typesize of %d for %s when trying to serialize", h5f->info.typesize, name);
    }

    /* Return Number of Bytes Serialized */
    const int64_t elements_available = h5f->info.elements - start_element;
    const int64_t elements_copied = MAX(MIN(elements_available, num_elements), 0);
    return elements_copied * h5f->info.typesize;
}

/*----------------------------------------------------------------------------
 * serializeRow
 *----------------------------------------------------------------------------*/
uint64_t H5DArray::serializeRow (uint8_t* buffer, int64_t row) const
{
    /* Get Elements to Read */
    const int64_t row_size = rowSize();
    const int64_t start_element = row_size * row;
    const int64_t num_elements = row_size;

    /* Read Elements */
    return serialize(buffer, start_element, num_elements);
}