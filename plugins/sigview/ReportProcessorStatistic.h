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

#ifndef __report_processor_statistic__
#define __report_processor_statistic__

#include "atlasdefines.h"
#include "TimeTagProcessorModule.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/************/
/* TYPEDEFS */
/************/

typedef struct {
    double  rws;
    double  rww;
    double  sigrng;
    double  bkgnd;
    double  sigpes;
    double  bceatten;
    double  bcepower;
    double  teppe;
} sigSpotStat_t;

typedef struct {
    uint32_t          statcnt;
    double          prilaserenergy;
    double          redlaserenergy;
    sigSpotStat_t   spot[NUM_PCES * NUM_SPOTS];
} reportStat_t;

/*******************/
/* STATISTIC CLASS */
/*******************/

class ReportProcessorStatistic: public StatisticRecord<reportStat_t>
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* rec_type;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void  prepost (void); // overloaded from StatisticRecord

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        char*               liveFilename;
        Mutex               liveFilenameMut;

        const char*         chName[NUM_PCES];
        const char*         txName[NUM_PCES];
        const char*         sigName[NUM_PCES];
        const char*         timeProcName;
        const char*         bceProcName;
        const char*         laserProcName;
        const char*         bceStatName;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                ReportProcessorStatistic    (CommandProcessor* cmd_proc, const char* obj_name,
                                             const char* ttproc_name[NUM_PCES],
                                             const char* timeproc_name,
                                             const char* bceproc_name,
                                             const char* laserproc_name);
                ~ReportProcessorStatistic   (void);

        bool    writeLiveFile               (FILE* fp);

        int     generateReportCmd           (int argc, char argv[][MAX_CMD_SIZE]);
        int     generateFullReportCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int     startLiveFileCmd            (int argc, char argv[][MAX_CMD_SIZE]);
        int     stopLiveFileCmd             (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __report_processor_statistic__ */
