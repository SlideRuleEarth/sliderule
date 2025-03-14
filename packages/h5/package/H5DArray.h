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

#ifndef __h5_dynamic_array__
#define __h5_dynamic_array__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Asset.h"
#include "RecordObject.h"
#include "Dictionary.h"
#include "H5Coro.h"

using H5Coro::ALL_ROWS;

/******************************************************************************
 * H5 DYNAMIC ARRAY CLASS
 ******************************************************************************/

class H5DArray
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef RecordObject::fieldType_t type_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void init                (void);

                    H5DArray            (H5Coro::Context* context, const char* dataset, long col=0, long startrow=0, long numrows=H5Coro::ALL_ROWS);
        virtual     ~H5DArray           (void);

        bool        join                (int timeout, bool throw_exception) const;

        int         numDimensions       (void) const;
        int         numElements         (void) const;
        int         elementSize         (void) const;
        type_t      elementType         (void) const;
        int64_t     rowSize             (void) const;
        uint64_t    serialize           (uint8_t* buffer, int64_t start_element, int64_t num_elements) const;
        uint64_t    serializeRow        (uint8_t* buffer, int64_t row) const;
        uint8_t*    referenceElement    (int64_t element) const;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*         name;
        H5Coro::Future*     h5f;
};

#endif  /* __h5_dynamic_array__ */
