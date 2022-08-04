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

        static const char* rec_type;

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
