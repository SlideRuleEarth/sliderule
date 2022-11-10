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

#ifndef __gt_dynamic_array__
#define __gt_dynamic_array__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "H5DArray.h"
#include "lua_parms.h"
#include "StringLib.h"
#include "Asset.h"

/******************************************************************************
 * GT DYNAMIC ARRAY CLASS
 ******************************************************************************/

class GTDArray
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const long DefaultStartRow[PAIR_TRACKS_PER_GROUND_TRACK];
        static const long DefaultNumRows[PAIR_TRACKS_PER_GROUND_TRACK];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    GTDArray        (const Asset* asset, const char* resource, int track, const char* gt_dataset, H5Coro::context_t* context, long col=0, const long* prt_startrow=DefaultStartRow, const long* prt_numrows=DefaultNumRows);
        virtual     ~GTDArray       (void);

        bool        join            (int timeout, bool throw_exception);
        uint64_t    serialize       (uint8_t* buffer, int32_t* start_element, uint32_t* num_elements);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        H5DArray gt[PAIR_TRACKS_PER_GROUND_TRACK];
};

#endif  /* __gt_dynamic_array__ */
