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

#ifndef __bce_processor_module__
#define __bce_processor_module__

#include "BceHistogram.h"
#include "atlasdefines.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

typedef struct {
    uint32_t  powercnt;
    uint32_t  attencnt;
    double  power[NUM_PCES * NUM_SPOTS];
    double  atten[NUM_PCES * NUM_SPOTS];
} bceStat_t;

class BceStat: public StatisticRecord<bceStat_t>
{
    public:

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        BceStat (CommandProcessor* cmd_proc, const char* rec_name);
};

class BceProcessorModule: public CcsdsProcessorModule
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int NUM_GRLS           = 7;
        static const int INVALID_APID       = CCSDS_NUM_APIDS;
        static const int MAX_STAT_NAME_SIZE = 128;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        BceProcessorModule  (CommandProcessor* cmd_proc, const char* obj_name, const char* histq_name);
                        ~BceProcessorModule (void);

        static  CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        BceStat*    bceStat;
        Publisher*  histQ;  // output histograms
        uint16_t    histApids[NUM_GRLS][BceHistogram::NUM_SUB_TYPES];
        uint16_t    attenApid;
        uint16_t    powerApid;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool    processSegments     (List<CcsdsSpacePacket*>& segments, int numpkts);
        bool    parseWaveforms      (unsigned char* pktbuf);
        bool    parseAttenuation    (unsigned char* pktbuf);
        bool    parsePower          (unsigned char* pktbuf);

        int     attachHistApidCmd   (int argc, char argv[][MAX_CMD_SIZE]);
        int     attachAttenApidCmd  (int argc, char argv[][MAX_CMD_SIZE]);
        int     attachPowerApidCmd  (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __bce_processor_module__ */
