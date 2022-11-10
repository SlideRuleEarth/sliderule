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

#include "h5.h"
#include "core.h"

/******************************************************************************
 * H5 DYNAMIC ARRAY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5DArray::H5DArray(const Asset* asset, const char* resource, const char* dataset, H5Coro::context_t* context, long col, long startrow, long numrows)
{
    if(asset)   h5f = H5Coro::readp(asset, resource, dataset, RecordObject::DYNAMIC, col, startrow, numrows, context);
    else        h5f = NULL;

    name    = StringLib::duplicate(dataset);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5DArray::~H5DArray(void)
{
    if(h5f)  delete h5f;
    if(name) delete [] name;
}

/*----------------------------------------------------------------------------
 * join
 *----------------------------------------------------------------------------*/
bool H5DArray::join(int timeout, bool throw_exception)
{
    bool status;

    if(h5f)
    {
        H5Future::rc_t rc = h5f->wait(timeout);
        if(rc == H5Future::COMPLETE)
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
                    case H5Future::INVALID: throw RunTimeException(ERROR, RTE_ERROR, "H5Future read failure on %s", name);
                    case H5Future::TIMEOUT: throw RunTimeException(ERROR, RTE_TIMEOUT, "H5Future read timeout on %s", name);
                    default:                throw RunTimeException(ERROR, RTE_ERROR, "H5Future unknown error on %s", name);
                }
            }
        }
    }
    else
    {
        status = false;
        if(throw_exception)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "H5Future null join on %s", name);
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * numElements
 *----------------------------------------------------------------------------*/
int H5DArray::numElements (void)
{
    return h5f->info.elements;
}

/*----------------------------------------------------------------------------
 * elementSize
 *----------------------------------------------------------------------------*/
int H5DArray::elementSize (void)
{
    return h5f->info.typesize;
}

/*----------------------------------------------------------------------------
 * elementType
 *----------------------------------------------------------------------------*/
H5DArray::type_t H5DArray::elementType (void)
{
    return h5f->info.datatype;
}

/*----------------------------------------------------------------------------
 * serialize
 *----------------------------------------------------------------------------*/
uint64_t H5DArray::serialize (uint8_t* buffer, int32_t start_element, uint32_t num_elements)
{
    /* Serialize Elements of Array */
    if(h5f->info.typesize == 8)
    {
        uint64_t* src = (uint64_t*)h5f->info.data;
        uint64_t* dst = (uint64_t*)buffer;
        for(uint32_t i = start_element; (i < h5f->info.elements) && (i < (start_element + num_elements)); i++)
        {
            *dst++ = src[i];
        }
    }
    else if(h5f->info.typesize == 4)
    {
        uint32_t* src = (uint32_t*)h5f->info.data;
        uint32_t* dst = (uint32_t*)buffer;
        for(uint32_t i = start_element; (i < h5f->info.elements) && (i < (start_element + num_elements)); i++)
        {
            *dst++ = src[i];
        }
    }
    else if(h5f->info.typesize == 2)
    {
        uint16_t* src = (uint16_t*)h5f->info.data;
        uint16_t* dst = (uint16_t*)buffer;
        for(uint32_t i = start_element; (i < h5f->info.elements) && (i < (start_element + num_elements)); i++)
        {
            *dst++ = src[i];
        }
    }
    else if(h5f->info.typesize == 1)
    {
        uint8_t* src = (uint8_t*)h5f->info.data;
        uint8_t* dst = (uint8_t*)buffer;
        for(uint32_t i = start_element; (i < h5f->info.elements) && (i < (start_element + num_elements)); i++)
        {
            *dst++ = src[i];
        }
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid typesize of %d for %s when trying to serialize", h5f->info.typesize, name);
    }

    /* Return Number of Bytes Serialized */
    int64_t elements_available = h5f->info.elements - start_element;
    if(elements_available < 0) elements_available = 0;
    uint64_t elements_copied = MIN(elements_available, num_elements);
    return elements_copied * h5f->info.typesize;
}
