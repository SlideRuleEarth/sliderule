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

#ifndef __time_processor_module__
#define __time_processor_module__

#include "atlasdefines.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

#define SAMPLE_HISTORY 16

/*******************/
/* TIME STAT CLASS */
/*******************/

typedef enum {
    SC_1PPS_A = 0,
    SC_1PPS_B = 1
} sc1ppsSource_t; /* which S/C 1PPS to associate to TAT */

typedef enum {
    USO_A = 0,
    USO_B = 1
} usoSource_t; /* which USO is selected */

typedef enum {
    GPS_TIME = 0,
    SC_TIME  = 1
} gpsSyncSource_t; /* which time in TAT to use */

typedef enum {
    DISABLED_1PPS_SRC = 0,
    SC_1PPS_A_SRC     = 1,
    SC_1PPS_B_SRC     = 2,
    ASC_1PPS_SRC      = 3,
    UNK_1PPS_SRC      = 4
} int1ppsSource_t; /* which 1pps signal to distribute to MEB */

typedef struct {
    uint32_t              statcnt;
    uint32_t              errorcnt;
    uint32_t              simhk_cnt;
    uint32_t              sxphk_cnt;
    uint32_t              timekeeping_cnt[NUM_PCES];
    uint16_t              simhk_sample_index;
    uint16_t              sxphk_sample_index;
    uint16_t              timekeeping_sample_index[NUM_PCES];

    double              sc_1pps_freq;                       // as calculated against AMET
    double              asc_1pps_freq;                      // as calculated against GPS
    double              tq_freq;                            // as calculated against its own GPS time
    double              mf_freq[NUM_PCES];

    double              sc_1pps_time;                       // sim housekeeping
    double              asc_1pps_time;                      // sim housekeeping
    double              tq_time;                            // sxp housekeeping
    double              mf_time[NUM_PCES];

    uint64_t              sc_1pps_amet;
    uint64_t              asc_1pps_amet;
    int64_t               sc_to_asc_1pps_amet_delta;

    uint64_t              sc_1pps_amets[SAMPLE_HISTORY];
    double              sc_1pps_gps[SAMPLE_HISTORY];
    double              asc_1pps_gps[SAMPLE_HISTORY];
    uint64_t              asc_1pps_amets[SAMPLE_HISTORY];
    double              tq_gps[SAMPLE_HISTORY];             // currently only using current and previous
    double              mf_gps[NUM_PCES][SAMPLE_HISTORY];   // currently only using current and previous
    uint32_t              mf_ids[NUM_PCES][SAMPLE_HISTORY];   // currently only using current and previous
    uint32_t              mf_amets[NUM_PCES][SAMPLE_HISTORY]; // currently only using current and previous

    double              uso_freq;
    bool                uso_freq_calc;
    sc1ppsSource_t      sc_1pps_source;
    usoSource_t         uso_source;
    gpsSyncSource_t     gps_sync_source;
    int1ppsSource_t     int_1pps_source;
} timeStat_t;

class TimeStat: public StatisticRecord<timeStat_t>
{
    public:

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        TimeStat (CommandProcessor* cmd_proc, const char* rec_name);
};

/************************/
/* TIME DIAG STAT CLASS */
/************************/

typedef enum {
    TIME_REF_ASC_1PPS_GPS = 0,
    TIME_REF_ASC_1PPS_AMET = 1,
    NUM_TIME_REFS = 2
} timeRef_t;

typedef struct {
    timeRef_t           ref;
    double              asc_1pps_gps_ref;
    double              sc_1pps_delta;
    double              sc_tat_rx_delta;
    double              sc_att_rx_delta;
    double              sc_pos_rx_delta;
    double              sc_att_sol_delta;
    double              sc_pos_sol_delta;
    double              sxp_pce_time_rx_delta[NUM_PCES];
    double              sxp_1st_mf_extrap_delta[NUM_PCES];
    double              pce_1st_mf_1pps_delta[NUM_PCES];
    int                 sxp_status[NUM_PCES * NUM_SPOTS];
} timeDiagStat_t;

class TimeDiagStat: public StatisticRecord<timeDiagStat_t>
{
    public:

        static const char* rec_type;
        static RecordObject::fieldDef_t rec_def[];
        static int rec_elem;

        TimeDiagStat (CommandProcessor* cmd_proc, const char* rec_name);
};

/************************/
/* TIME PROCESSOR CLASS */
/************************/

class TimeProcessorModule: public CcsdsProcessorModule
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int INVALID_APID = CCSDS_NUM_APIDS;
        static const int MAX_STAT_NAME_SIZE = 128;

        static const double DEFAULT_10NS_PERIOD;

        static const char*  true10Key;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        TimeProcessorModule         (CommandProcessor* cmd_proc, const char* obj_name);
                        ~TimeProcessorModule        (void);

        static  CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        double          trueRulerClkPeriod;
        timeRef_t       diagTimeRef;

        TimeStat*       timeStat;
        TimeDiagStat*   timeDiagStat;

        uint16_t          simHkApid;
        uint16_t          sxpHkApid;
        uint16_t          timekeepingApid[NUM_PCES];
        uint16_t          sxpDiagApid;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool    processSegments          (List<CcsdsSpacePacket*>& segments, int numpkts);

        bool    parseSimHkPkt            (unsigned char* pktbuf);
        bool    parseSxpHkPkt            (unsigned char* pktbuf);
        bool    parseTimekeepingPkt      (unsigned char* pktbuf, int pce);
        bool    parseSxpDiagPkt          (unsigned char* pktbuf);

        int     attachSimHkApidCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int     attachSxpHkApidCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int     attachTimekeepingApidCmd (int argc, char argv[][MAX_CMD_SIZE]);
        int     attachSxpDiagApidCmd     (int argc, char argv[][MAX_CMD_SIZE]);
        int     setSxpDiagTimeRefCmd     (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __time_processor_module__ */
