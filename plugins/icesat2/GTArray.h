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

        static const unsigned DefaultStartRow[PAIR_TRACKS_PER_GROUND_TRACK];
        static const unsigned DefaultNumRows[PAIR_TRACKS_PER_GROUND_TRACK];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    GTArray     (const char* url, int track, const char* gt_dataset, unsigned col=0, const unsigned* prt_startrow=DefaultStartRow, const unsigned* prt_numrows=DefaultNumRows);
        virtual     ~GTArray    (void);

        bool        trim        (unsigned* prt_offset);
        H5Array<T>& operator[]  (int index);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        H5Array<T> gt[PAIR_TRACKS_PER_GROUND_TRACK];
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/
template <class T>
const unsigned GTArray<T>::DefaultStartRow[PAIR_TRACKS_PER_GROUND_TRACK] = {0, 0};

template <class T>
const unsigned GTArray<T>::DefaultNumRows[PAIR_TRACKS_PER_GROUND_TRACK] = {0, 0};

/******************************************************************************
 * GTArray METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
GTArray<T>::GTArray(const char* url, int track, const char* gt_dataset, unsigned col, const unsigned* prt_startrow, const unsigned* prt_numrows):
    gt{ H5Array<T>(url, SafeString("/gt%dl/%s", track, gt_dataset).getString(), col, prt_startrow[PRT_LEFT], prt_numrows[PRT_LEFT]),
        H5Array<T>(url, SafeString("/gt%dr/%s", track, gt_dataset).getString(), col, prt_startrow[PRT_RIGHT], prt_numrows[PRT_RIGHT]) }
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
bool GTArray<T>::trim(unsigned* prt_offset)
{
    if(!prt_offset) return false;
    else return (gt[PRT_LEFT].trim(prt_offset[PRT_LEFT]) && gt[PRT_RIGHT].trim(prt_offset[PRT_RIGHT]));
}

/*----------------------------------------------------------------------------
 * []
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>& GTArray<T>::operator[](int index)
{
    return gt[index];
}

#endif  /* __gtarray__ */
