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

#include "GTDArray.h"
#include "Icesat2Fields.h"
#include "OsApi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const long GTDArray::DefaultStartRow[Icesat2Fields::NUM_PAIR_TRACKS] = {0, 0};
const long GTDArray::DefaultNumRows[Icesat2Fields::NUM_PAIR_TRACKS] = {H5Coro::ALL_ROWS, H5Coro::ALL_ROWS};

/******************************************************************************
 * GT DYNAMIC ARRAY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GTDArray::GTDArray(H5Coro::Context* context, int track, const char* gt_dataset, long col, const long* prt_startrow, const long* prt_numrows):
    gt{ H5DArray(context, FString("/gt%dl/%s", track, gt_dataset).c_str(), col, prt_startrow[Icesat2Fields::RPT_L], prt_numrows[Icesat2Fields::RPT_L]),
        H5DArray(context, FString("/gt%dr/%s", track, gt_dataset).c_str(), col, prt_startrow[Icesat2Fields::RPT_R], prt_numrows[Icesat2Fields::RPT_R]) }
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GTDArray::~GTDArray(void) = default;

/*----------------------------------------------------------------------------
 * join
 *----------------------------------------------------------------------------*/
bool GTDArray::join(int timeout, bool throw_exception)
{
    return (gt[Icesat2Fields::RPT_L].join(timeout, throw_exception) && gt[Icesat2Fields::RPT_R].join(timeout, throw_exception));
}

/*----------------------------------------------------------------------------
 * serialize
 *----------------------------------------------------------------------------*/
uint64_t GTDArray::serialize (uint8_t* buffer, const int32_t* start_element, const uint32_t* num_elements)
{
    const uint64_t bytes_written = gt[Icesat2Fields::RPT_L].serialize(&buffer[0], start_element[Icesat2Fields::RPT_L], num_elements[Icesat2Fields::RPT_L]);
    return gt[Icesat2Fields::RPT_L].serialize(&buffer[bytes_written], start_element[Icesat2Fields::RPT_R], num_elements[Icesat2Fields::RPT_R]);
}
