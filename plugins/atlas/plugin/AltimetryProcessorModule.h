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

#ifndef __altimetry_processor_module__
#define __altimetry_processor_module__

#include "atlasdefines.h"
#include "AltimetryHistogram.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class AltimetryProcessorModule: public CcsdsProcessorModule
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int    NUM_ALT_BINS_PER_PKT    = 500;
        static const int    NUM_ATM_BINS_PER_PKT    = 467;
        static const int    NUM_ALT_SEGS_PER_PKT    = 4;

        static const double ALT_BINSIZE;
        static const double ATM_BINSIZE;
        static const double DEFAULT_10NS_PERIOD;

        static const char*  fullColumnIntegrationKey;
        static const char*  alignHistKey;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        AltimetryProcessorModule  (CommandProcessor* cmd_proc, const char* obj_name, int _pce, AltimetryHistogram::type_t _type, const char* histq_name);
                        ~AltimetryProcessorModule (void);

        static  CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Publisher*                  histQ;  // output histograms
        int                         pce;
        AltimetryHistogram::type_t  type;

        bool                        alignHistograms;
        bool                        fullColumnIntegration;
        double                      trueRulerClkPeriod;
        const char*                 majorFrameProcName;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool    processSegments     (List<CcsdsSpacePacket*>& segments, int numpkts);
        bool    parseAltHist        (List<CcsdsSpacePacket*>& segments, int numpkts);
        bool    parseAtmHist        (List<CcsdsSpacePacket*>& segments, int numpkts);

        int     alignHistsCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int     fullColumnModeCmd   (int argc, char argv[][MAX_CMD_SIZE]);
        int     setClkPeriodCmd     (int argc, char argv[][MAX_CMD_SIZE]);
        int     attachMFProcCmd     (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __altimetry_processor_module__ */
