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

#ifndef __gtarray__
#define __gtarray__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "H5Array.h"
#include "StringLib.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define PAIR_TRACKS_PER_GROUND_TRACK    2
#define PRT_LEFT                        0
#define PRT_RIGHT                       1

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

        static const long DefaultStartRow[PAIR_TRACKS_PER_GROUND_TRACK];
        static const long DefaultNumRows[PAIR_TRACKS_PER_GROUND_TRACK];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    GTArray     (const char* url, int track, const char* gt_dataset, H5Api::context_t* context, unsigned col=0, const long* prt_startrow=DefaultStartRow, const long* prt_numrows=DefaultNumRows);
        virtual     ~GTArray    (void);

        bool        trim        (long* prt_offset);
        bool        join        (int timeout);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        H5Array<T> gt[PAIR_TRACKS_PER_GROUND_TRACK];
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/
template <class T>
const long GTArray<T>::DefaultStartRow[PAIR_TRACKS_PER_GROUND_TRACK] = {0, 0};

template <class T>
const long GTArray<T>::DefaultNumRows[PAIR_TRACKS_PER_GROUND_TRACK] = {H5Api::ALL_ROWS, H5Api::ALL_ROWS};

/******************************************************************************
 * GTArray METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
GTArray<T>::GTArray(const char* url, int track, const char* gt_dataset, H5Api::context_t* context, unsigned col, const long* prt_startrow, const long* prt_numrows):
    gt{ H5Array<T>(url, SafeString("/gt%dl/%s", track, gt_dataset).getString(), context, col, prt_startrow[PRT_LEFT], prt_numrows[PRT_LEFT]),
        H5Array<T>(url, SafeString("/gt%dr/%s", track, gt_dataset).getString(), context, col, prt_startrow[PRT_RIGHT], prt_numrows[PRT_RIGHT]) }
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
 * trim
 *----------------------------------------------------------------------------*/
template <class T>
bool GTArray<T>::trim(long* prt_offset)
{
    if(!prt_offset) return false;
    else return (gt[PRT_LEFT].trim(prt_offset[PRT_LEFT]) && gt[PRT_RIGHT].trim(prt_offset[PRT_RIGHT]));
}

/*----------------------------------------------------------------------------
 * join
 *----------------------------------------------------------------------------*/
template <class T>
bool GTArray<T>::join(int timeout)
{
    return (gt[PRT_LEFT].join(timeout) && gt[PRT_RIGHT].join(timeout));
}

#endif  /* __gtarray__ */
