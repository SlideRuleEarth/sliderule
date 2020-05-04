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

#ifndef __bce_histogram__
#define __bce_histogram__

#include "AtlasHistogram.h"

class BceHistogram: public AtlasHistogram
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double BINSIZE;
        static const char* rec_type;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            INVALID = -1,
            WAV = 0,
            TOF = 1,
            NUM_SUB_TYPES = 2,
        } subtype_t;

        typedef struct {
            AtlasHistogram::hist_t  hist;
            int                     grl;
            int                     spot;
            int                     oscId;
            int                     oscCh;
            int                     subtype;
        } bceHist_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                BceHistogram        (AtlasHistogram::type_t _type, int _intperiod, double _binsize, double _gps, int _grl, int _spot, int _oscid, int _oscch, int _subtype);
        virtual                 ~BceHistogram       (void);

        int                     getOscId            (void);
        int                     getOscCh            (void);

        bool                    calcAttributes      (double sigwidth, double bincal); // returns if signal is found

        static recordDefErr_t   defineHistogram     (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        bceHist_t* bce;
};

#endif  /* __bce_histogram__ */
