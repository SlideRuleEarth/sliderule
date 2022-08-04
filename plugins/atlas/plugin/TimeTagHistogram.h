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

#ifndef __time_tag_histogram__
#define __time_tag_histogram__

#include "atlasdefines.h"
#include "TimeTagProcessorModule.h"
#include "AtlasHistogram.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class TimeTagHistogram: public AtlasHistogram
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* rec_type;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef TimeTagProcessorModule::rxPulse_t   tag_t;
        typedef TimeTagProcessorModule::dlb_t       band_t;
        typedef pktStat_t                           stat_t;

        typedef struct {
            AtlasHistogram::hist_t  hist;
            double                  channelBiases[NUM_CHANNELS];
            bool                    channelBiasSet[NUM_CHANNELS];
            int32_t                 channelCounts[NUM_CHANNELS];
            int32_t                 numDownlinkBands;
            band_t                  downlinkBands[MAX_NUM_DLBS];
            int32_t                 downlinkBandsTagCnt[MAX_NUM_DLBS];
            stat_t                  pktStats;
        } ttHist_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                TimeTagHistogram    (AtlasHistogram::type_t _type,
                                                     int                    _intperiod,
                                                     double                 _binsize,
                                                     int                    _pcenum,
                                                     long                   _mfc,
                                                     mfdata_t*              _mfdata,
                                                     double                 _gps,
                                                     double                 _rws,
                                                     double                 _rww,
                                                     band_t*                _bands,
                                                     int                    _numbands,
                                                     bool                   _deep_free=false    );
                                ~TimeTagHistogram   (void);

        bool                    binTag              (int bin, tag_t* tag);
        void                    setPktStats         (const stat_t* stats);
        void                    incChCount          (int index);
        tag_t*                  getTag              (int bin, int offset);
        List<tag_t*>*           getTagList          (int bin);
        void                    getChBiases         (double bias[], bool valid[], int start_ch, int stop_ch);
        void                    getChCounts         (int cnts[]);
        int                     getChCount          (int channel);
        int                     getNumDownlinkBands (void);
        const band_t*           getDownlinkBands    (void);
        const stat_t*           getPktStats         (void);
        static recordDefErr_t   defineHistogram     (void);

        bool                    calcAttributes      (double sigwidth, double bincal); // returns if signal is found

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        List<tag_t*>*   tags[MAX_HIST_SIZE];
        bool            deepFree;

        ttHist_t*       tt;
};

#endif  /* __time_tag_histogram__ */
