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
