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

#ifndef __gtarray__
#define __gtarray__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Icesat2Parms.h"
#include "H5Array.h"
#include "StringLib.h"
#include "Asset.h"

/******************************************************************************
 * GTArray TEMPLATE
 ******************************************************************************/

template <class T>
class GTArray
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const long DefaultStartRow[Icesat2Parms::NUM_PAIR_TRACKS];
        static const long DefaultNumRows[Icesat2Parms::NUM_PAIR_TRACKS];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    GTArray     (const Asset* asset, const char* resource, int track, const char* gt_dataset, H5Coro::context_t* context, long col=0, const long* prt_startrow=DefaultStartRow, const long* prt_numrows=DefaultNumRows);
        virtual     ~GTArray    (void);

        H5Array<T>& operator[]  (int t);
        bool        trim        (long* prt_offset);
        bool        join        (int timeout, bool throw_exception);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        H5Array<T> gt[Icesat2Parms::NUM_PAIR_TRACKS];
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/
template <class T>
const long GTArray<T>::DefaultStartRow[Icesat2Parms::NUM_PAIR_TRACKS] = {0, 0};

template <class T>
const long GTArray<T>::DefaultNumRows[Icesat2Parms::NUM_PAIR_TRACKS] = {H5Coro::ALL_ROWS, H5Coro::ALL_ROWS};

/******************************************************************************
 * GTArray METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
GTArray<T>::GTArray(const Asset* asset, const char* resource, int track, const char* gt_dataset, H5Coro::context_t* context, long col, const long* prt_startrow, const long* prt_numrows):
    gt{ H5Array<T>(asset, resource, SafeString("/gt%dl/%s", track, gt_dataset).getString(), context, col, prt_startrow[Icesat2Parms::RPT_L], prt_numrows[Icesat2Parms::RPT_L]),
        H5Array<T>(asset, resource, SafeString("/gt%dr/%s", track, gt_dataset).getString(), context, col, prt_startrow[Icesat2Parms::RPT_R], prt_numrows[Icesat2Parms::RPT_R]) }
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
GTArray<T>::~GTArray(void)
{
}

/*----------------------------------------------------------------------------
 * operator[]
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>& GTArray<T>::operator[] (int t)
{
    return gt[t];
}

/*----------------------------------------------------------------------------
 * trim
 *----------------------------------------------------------------------------*/
template <class T>
bool GTArray<T>::trim(long* prt_offset)
{
    if(!prt_offset) return false;
    else return (gt[Icesat2Parms::RPT_L].trim(prt_offset[Icesat2Parms::RPT_L]) && gt[Icesat2Parms::RPT_R].trim(prt_offset[Icesat2Parms::RPT_R]));
}

/*----------------------------------------------------------------------------
 * join
 *----------------------------------------------------------------------------*/
template <class T>
bool GTArray<T>::join(int timeout, bool throw_exception)
{
    return (gt[Icesat2Parms::RPT_L].join(timeout, throw_exception) && gt[Icesat2Parms::RPT_R].join(timeout, throw_exception));
}

#endif  /* __gtarray__ */
