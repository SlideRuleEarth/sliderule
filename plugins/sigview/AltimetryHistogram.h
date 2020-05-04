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

#ifndef __altimetry_histogram__
#define __altimetry_histogram__

#include "AtlasHistogram.h"
#include "atlasdefines.h"
#include "MajorFrameProcessorModule.h"

#include "core.h"
#include "legacy.h"

class AltimetryHistogram: public AtlasHistogram
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* rec_type[NUM_PCES];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            AtlasHistogram::hist_t  hist;
        } altHist_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                AltimetryHistogram  (AtlasHistogram::type_t _type,
                                                     int                    _intperiod,
                                                     double                 _binsize,
                                                     int                    _pcenum,
                                                     long                   _mfc,
                                                     mfdata_t*              _mfdata,
                                                     double                 _gps,
                                                     double                 _rws,
                                                     double                 _rww    );
        virtual                 ~AltimetryHistogram (void);
        static recordDefErr_t   defineHistogram     (void);
        bool                    calcAttributes      (double sigwidth, double bincal); // returns if signal is found

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        altHist_t* alt;
};

#endif  /* __altimetry_histogram__ */
