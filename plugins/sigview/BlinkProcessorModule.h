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

#ifndef __blink_processor_module__
#define __blink_processor_module__

#include "atlasdefines.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

typedef struct {
    uint64_t  mfc;
    uint8_t   shot;
    uint32_t  rxcnt;
    double  tx_sc_gps;      // calculated using s/c 1PPS gps and AMET
    double  tx_asc_gps;     // calculated using asc 1PPS gps and AMET
    double  tx_sxp_gps;     // calculated using SXP MF gps
    double  tx_pce_gps;     // calculated using PCE Timekeeping gps and AMET
} blinkStat_t;

class BlinkStat: public StatisticRecord<blinkStat_t>
{
    public:

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        BlinkStat (CommandProcessor* cmd_proc, const char* rec_name);
};

class BlinkProcessorModule: public CcsdsProcessorModule
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int TransmitPulseCoarseCorrection = -1;     // 100MHz clocks (this is a correction to T0 and does not take into account start pulse delay onto the board)
        static const long DARK_ZERO_THRESHOLD = 100;
        static const int MAX_STAT_NAME_SIZE = 128;

        static const double DEFAULT_10NS_PERIOD;

        static const char*  true10Key;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        BlinkProcessorModule         (CommandProcessor* cmd_proc, const char* obj_name, const char* time_proc_name);
        ~BlinkProcessorModule        (void);

        static  CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        double          trueRulerClkPeriod;
        const char*     timeStatName;
        BlinkStat*      blinkStat;
        long            darkZeroCount;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool    processSegments (List<CcsdsSpacePacket*>& segments, int numpkts);
};

#endif  /* __blink_processor_module__ */
