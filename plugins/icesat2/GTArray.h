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
         * Methods
         *--------------------------------------------------------------------*/

                    GTArray     (const char* url, int track, const char* gt_dataset, unsigned col=0);
        virtual     ~GTArray    (void);

        H5Array<T>& operator[]  (int index);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        H5Array<T> gt[PAIR_TRACKS_PER_GROUND_TRACK];
};

/******************************************************************************
 * GTArray METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
GTArray<T>::GTArray(const char* url, int track, const char* gt_dataset, unsigned col):
    gt{ H5Array<T>(url, SafeString("/gt%dl/%s", track, gt_dataset).getString(), col),
        H5Array<T>(url, SafeString("/gt%dr/%s", track, gt_dataset).getString(), col) }
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
 * []]
 *----------------------------------------------------------------------------*/
template <class T>
H5Array<T>& GTArray<T>::operator[](int index)
{
    return gt[index];
}

#endif  /* __gtarray__ */
