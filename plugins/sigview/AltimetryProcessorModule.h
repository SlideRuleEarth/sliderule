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
