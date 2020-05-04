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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <math.h>

#include "TimeProcessorModule.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double TimeProcessorModule::DEFAULT_10NS_PERIOD = 10.0;

const char* TimeProcessorModule::true10Key = "true10ns";

/******************************************************************************
 * TIME STATISTIC
 ******************************************************************************/

const char* TimeStat::rec_type = "TimeStat";

RecordObject::fieldDef_t TimeStat::rec_def[] =
{
    {"ERRORCNT",          UINT32, offsetof(timeStat_t, errorcnt),                   sizeof(((timeStat_t*)0)->errorcnt),                  NATIVE_FLAGS},
    {"SC_1PPS_FREQ",      DOUBLE, offsetof(timeStat_t, sc_1pps_freq),               sizeof(((timeStat_t*)0)->sc_1pps_freq),              NATIVE_FLAGS},
    {"ASC_1PPS_FREQ",     DOUBLE, offsetof(timeStat_t, asc_1pps_freq),              sizeof(((timeStat_t*)0)->asc_1pps_freq),             NATIVE_FLAGS},
    {"TQ_FREQ",           DOUBLE, offsetof(timeStat_t, tq_freq),                    sizeof(((timeStat_t*)0)->tq_freq),                   NATIVE_FLAGS},
    {"MF_FREQ[1]",        DOUBLE, offsetof(timeStat_t, mf_freq[0]),                 sizeof(((timeStat_t*)0)->mf_freq[0]),                NATIVE_FLAGS},
    {"MF_FREQ[2]",        DOUBLE, offsetof(timeStat_t, mf_freq[1]),                 sizeof(((timeStat_t*)0)->mf_freq[1]),                NATIVE_FLAGS},
    {"MF_FREQ[3]",        DOUBLE, offsetof(timeStat_t, mf_freq[2]),                 sizeof(((timeStat_t*)0)->mf_freq[2]),                NATIVE_FLAGS},
    {"SC_1PPS_TIME",      DOUBLE, offsetof(timeStat_t, sc_1pps_time),               sizeof(((timeStat_t*)0)->sc_1pps_time),              NATIVE_FLAGS},
    {"ASC_1PPS_TIME",     DOUBLE, offsetof(timeStat_t, asc_1pps_time),              sizeof(((timeStat_t*)0)->asc_1pps_time),             NATIVE_FLAGS},
    {"TQ_TIME",           DOUBLE, offsetof(timeStat_t, tq_time),                    sizeof(((timeStat_t*)0)->tq_time),                   NATIVE_FLAGS},
    {"MF_TIME[1]",        DOUBLE, offsetof(timeStat_t, mf_time[0]),                 sizeof(((timeStat_t*)0)->mf_time[0]),                NATIVE_FLAGS},
    {"MF_TIME[2]",        DOUBLE, offsetof(timeStat_t, mf_time[1]),                 sizeof(((timeStat_t*)0)->mf_time[1]),                NATIVE_FLAGS},
    {"MF_TIME[3]",        DOUBLE, offsetof(timeStat_t, mf_time[2]),                 sizeof(((timeStat_t*)0)->mf_time[2]),                NATIVE_FLAGS},
    {"SC_1PPS_AMET",      UINT64, offsetof(timeStat_t, sc_1pps_amet),               sizeof(((timeStat_t*)0)->sc_1pps_amet),              NATIVE_FLAGS},
    {"ASC_1PPS_AMET",     UINT64, offsetof(timeStat_t, asc_1pps_amet),              sizeof(((timeStat_t*)0)->asc_1pps_amet),             NATIVE_FLAGS},
    {"SC2ASC_AMET_DELTA", INT64,  offsetof(timeStat_t, sc_to_asc_1pps_amet_delta),  sizeof(((timeStat_t*)0)->sc_to_asc_1pps_amet_delta), NATIVE_FLAGS},
    {"USO_FREQ",          DOUBLE, offsetof(timeStat_t, uso_freq),                   sizeof(((timeStat_t*)0)->uso_freq),                  NATIVE_FLAGS},
    {"USO_FREQ_CALC",     UINT8,  offsetof(timeStat_t, uso_freq_calc),              sizeof(((timeStat_t*)0)->uso_freq_calc),             NATIVE_FLAGS},
    {"SC_1PPS_SRC",       INT32,  offsetof(timeStat_t, sc_1pps_source),             sizeof(((timeStat_t*)0)->sc_1pps_source),            NATIVE_FLAGS},
    {"USO_SRC",           INT32,  offsetof(timeStat_t, uso_source),                 sizeof(((timeStat_t*)0)->uso_source),                NATIVE_FLAGS},
    {"GPS_SYNC_SRC",      INT32,  offsetof(timeStat_t, gps_sync_source),            sizeof(((timeStat_t*)0)->gps_sync_source),           NATIVE_FLAGS},
    {"INT_1PPS_SRC",      INT32,  offsetof(timeStat_t, int_1pps_source),            sizeof(((timeStat_t*)0)->int_1pps_source),           NATIVE_FLAGS}
};

int TimeStat::rec_elem = sizeof(TimeStat::rec_def) / sizeof(RecordObject::fieldDef_t);

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
TimeStat::TimeStat(CommandProcessor* cmd_proc, const char* stat_name): StatisticRecord<timeStat_t>(cmd_proc, stat_name, rec_type, false)
{
    cmdProc->registerObject(stat_name, this);
}

/******************************************************************************
 TIME DIAGNOSTIC STATISTIC
 ******************************************************************************/

const char* TimeDiagStat::rec_type = "TimeDiagStat";

RecordObject::fieldDef_t TimeDiagStat::rec_def[] =
{
    {"REF",                       UINT32, offsetof(timeDiagStat_t, ref),                          sizeof(((timeDiagStat_t*)0)->ref),                         NATIVE_FLAGS},
    {"ASC_1PPS_GPS_REF",          DOUBLE, offsetof(timeDiagStat_t, asc_1pps_gps_ref),             sizeof(((timeDiagStat_t*)0)->asc_1pps_gps_ref),            NATIVE_FLAGS},
    {"SC_1PPS_DELTA",             DOUBLE, offsetof(timeDiagStat_t, sc_1pps_delta),                sizeof(((timeDiagStat_t*)0)->sc_1pps_delta),               NATIVE_FLAGS},
    {"SC_TAT_RX_DELTA",           DOUBLE, offsetof(timeDiagStat_t, sc_tat_rx_delta),              sizeof(((timeDiagStat_t*)0)->sc_tat_rx_delta),             NATIVE_FLAGS},
    {"SC_ATT_RX_DELTA",           DOUBLE, offsetof(timeDiagStat_t, sc_att_rx_delta),              sizeof(((timeDiagStat_t*)0)->sc_att_rx_delta),             NATIVE_FLAGS},
    {"SC_POS_RX_DELTA",           DOUBLE, offsetof(timeDiagStat_t, sc_pos_rx_delta),              sizeof(((timeDiagStat_t*)0)->sc_pos_rx_delta),             NATIVE_FLAGS},
    {"SC_ATT_SOL_DELTA",          DOUBLE, offsetof(timeDiagStat_t, sc_att_sol_delta),             sizeof(((timeDiagStat_t*)0)->sc_att_sol_delta),            NATIVE_FLAGS},
    {"SC_POS_SOL_DELTA",          DOUBLE, offsetof(timeDiagStat_t, sc_pos_sol_delta),             sizeof(((timeDiagStat_t*)0)->sc_pos_sol_delta),            NATIVE_FLAGS},
    {"SXP_PCE_TIME_RX_DELTA[1]",  DOUBLE, offsetof(timeDiagStat_t, sxp_pce_time_rx_delta[0]),     sizeof(((timeDiagStat_t*)0)->sxp_pce_time_rx_delta[0]),    NATIVE_FLAGS},
    {"SXP_PCE_TIME_RX_DELTA[2]",  DOUBLE, offsetof(timeDiagStat_t, sxp_pce_time_rx_delta[1]),     sizeof(((timeDiagStat_t*)0)->sxp_pce_time_rx_delta[1]),    NATIVE_FLAGS},
    {"SXP_PCE_TIME_RX_DELTA[3]",  DOUBLE, offsetof(timeDiagStat_t, sxp_pce_time_rx_delta[2]),     sizeof(((timeDiagStat_t*)0)->sxp_pce_time_rx_delta[2]),    NATIVE_FLAGS},
    {"SXP_1ST_MF_EXTRAP_DELTA[1]",DOUBLE, offsetof(timeDiagStat_t, sxp_1st_mf_extrap_delta[0]),   sizeof(((timeDiagStat_t*)0)->sxp_1st_mf_extrap_delta[0]),  NATIVE_FLAGS},
    {"SXP_1ST_MF_EXTRAP_DELTA[2]",DOUBLE, offsetof(timeDiagStat_t, sxp_1st_mf_extrap_delta[1]),   sizeof(((timeDiagStat_t*)0)->sxp_1st_mf_extrap_delta[1]),  NATIVE_FLAGS},
    {"SXP_1ST_MF_EXTRAP_DELTA[3]",DOUBLE, offsetof(timeDiagStat_t, sxp_1st_mf_extrap_delta[2]),   sizeof(((timeDiagStat_t*)0)->sxp_1st_mf_extrap_delta[2]),  NATIVE_FLAGS},
    {"PCE_1ST_MF_1PPS_DELTA[1]",  DOUBLE, offsetof(timeDiagStat_t, pce_1st_mf_1pps_delta[0]),     sizeof(((timeDiagStat_t*)0)->pce_1st_mf_1pps_delta[0]),    NATIVE_FLAGS},
    {"PCE_1ST_MF_1PPS_DELTA[2]",  DOUBLE, offsetof(timeDiagStat_t, pce_1st_mf_1pps_delta[1]),     sizeof(((timeDiagStat_t*)0)->pce_1st_mf_1pps_delta[1]),    NATIVE_FLAGS},
    {"PCE_1ST_MF_1PPS_DELTA[3]",  DOUBLE, offsetof(timeDiagStat_t, pce_1st_mf_1pps_delta[2]),     sizeof(((timeDiagStat_t*)0)->pce_1st_mf_1pps_delta[2]),    NATIVE_FLAGS},
    {"SXP_STATUS[0]",             DOUBLE, offsetof(timeDiagStat_t, sxp_status[0]),                sizeof(((timeDiagStat_t*)0)->sxp_status[0]),               NATIVE_FLAGS},
    {"SXP_STATUS[1]",             DOUBLE, offsetof(timeDiagStat_t, sxp_status[1]),                sizeof(((timeDiagStat_t*)0)->sxp_status[1]),               NATIVE_FLAGS},
    {"SXP_STATUS[2]",             DOUBLE, offsetof(timeDiagStat_t, sxp_status[2]),                sizeof(((timeDiagStat_t*)0)->sxp_status[2]),               NATIVE_FLAGS},
    {"SXP_STATUS[3]",             DOUBLE, offsetof(timeDiagStat_t, sxp_status[3]),                sizeof(((timeDiagStat_t*)0)->sxp_status[3]),               NATIVE_FLAGS},
    {"SXP_STATUS[4]",             DOUBLE, offsetof(timeDiagStat_t, sxp_status[4]),                sizeof(((timeDiagStat_t*)0)->sxp_status[4]),               NATIVE_FLAGS},
    {"SXP_STATUS[5]",             DOUBLE, offsetof(timeDiagStat_t, sxp_status[5]),                sizeof(((timeDiagStat_t*)0)->sxp_status[5]),               NATIVE_FLAGS}
};

int TimeDiagStat::rec_elem = sizeof(TimeDiagStat::rec_def) / sizeof(RecordObject::fieldDef_t);

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
TimeDiagStat::TimeDiagStat(CommandProcessor* cmd_proc, const char* stat_name): StatisticRecord<timeDiagStat_t>(cmd_proc, stat_name, rec_type, false)
{
    cmdProc->registerObject(stat_name, this);
}

/******************************************************************************
 * TIME PROCESSOR PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
TimeProcessorModule::TimeProcessorModule(CommandProcessor* cmd_proc, const char* obj_name):
    CcsdsProcessorModule(cmd_proc, obj_name)
{
    trueRulerClkPeriod = DEFAULT_10NS_PERIOD;
    diagTimeRef = TIME_REF_ASC_1PPS_GPS;

    /* Post Initial Values to Current Value Table */
    cmdProc->setCurrentValue(getName(), true10Key, (void*)&trueRulerClkPeriod,  sizeof(trueRulerClkPeriod));

    /* Define Statistic Records */
    TimeStat::defineRecord(TimeStat::rec_type, NULL, sizeof(timeStat_t), TimeStat::rec_def, TimeStat::rec_elem, 32);
    TimeDiagStat::defineRecord(TimeDiagStat::rec_type, "REF", sizeof(timeDiagStat_t), TimeDiagStat::rec_def, TimeDiagStat::rec_elem, 32);

    /* Initialize Statistics */
    char stat_name[MAX_STAT_NAME_SIZE];
    timeStat = new TimeStat(cmd_proc, StringLib::format(stat_name, MAX_STAT_NAME_SIZE, "%s.%s", obj_name, TimeStat::rec_type));
    timeDiagStat = new TimeDiagStat(cmd_proc, StringLib::format(stat_name, MAX_STAT_NAME_SIZE, "%s.%s", obj_name, TimeDiagStat::rec_type));

    /* Initialize APIDs */
    simHkApid           = 0x402;
    sxpHkApid           = 0x409;
    timekeepingApid[0]  = 0x473;
    timekeepingApid[1]  = 0x474;
    timekeepingApid[2]  = 0x475;
    sxpDiagApid         = 0x486;

    /* Register Commands */
    registerCommand("ATTACH_SIM_HK_APID",        (cmdFunc_t)&TimeProcessorModule::attachSimHkApidCmd,         1, "<apid>");
    registerCommand("ATTACH_SXP_HK_APID",        (cmdFunc_t)&TimeProcessorModule::attachSxpHkApidCmd,         1, "<apid>");
    registerCommand("ATTACH_TIMEKEEPING_APIDS",  (cmdFunc_t)&TimeProcessorModule::attachTimekeepingApidCmd,   3, "<pce 1 apid> <pce 2 apid> <pce 3 apid>");
    registerCommand("ATTACH_SXP_DIAG_APID",      (cmdFunc_t)&TimeProcessorModule::attachSxpDiagApidCmd,       1, "<apid>");
    registerCommand("SET_SXP_DIAG_TIME_REF",     (cmdFunc_t)&TimeProcessorModule::setSxpDiagTimeRefCmd,       1, "<GPS|AMET>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
TimeProcessorModule::~TimeProcessorModule(void)
{
    // --- these must be deleted via CommandableObject
    //  delete timeStat;
    //  delete timeDiagStat;
}

/******************************************************************************
 * TIME PROCESSOR PUBLIC STATIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* TimeProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    return new TimeProcessorModule(cmd_proc, name);
}

/******************************************************************************
 * TIME PROCESSOR PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processSegments  - Parser for Science Time Tag Telemetry Packet
 *
 *   Notes: ALT
 *----------------------------------------------------------------------------*/
bool TimeProcessorModule::processSegments(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    (void)numpkts;

    int numsegs = segments.length();
    for(int i = 0; i < numsegs; i++)
    {
        unsigned char* pktbuf = segments[i]->getBuffer();
        uint16_t apid = segments[i]->getAPID();

             if(apid == simHkApid)          parseSimHkPkt(pktbuf);
        else if(apid == sxpHkApid)          parseSxpHkPkt(pktbuf);
        else if(apid == timekeepingApid[0]) parseTimekeepingPkt(pktbuf, 0);
        else if(apid == timekeepingApid[1]) parseTimekeepingPkt(pktbuf, 1);
        else if(apid == timekeepingApid[2]) parseTimekeepingPkt(pktbuf, 2);
        else if(apid == sxpDiagApid)        parseSxpDiagPkt(pktbuf);
        else
        {
            mlog(CRITICAL, "Invalid APID presented: %04X\n", apid);
            return false;
        }
    }

    return true;
}


/*----------------------------------------------------------------------------
 * parseSimHkPkt  - Parse SIM Housekeeping Packet
 *
 *   Notes: POSTS timeStat
 *----------------------------------------------------------------------------*/
bool TimeProcessorModule::parseSimHkPkt(unsigned char* pktbuf)
{
    /* Parse Packet */
    uint64_t          curr_amet_hi            = parseInt(pktbuf + 20, 4);
    uint64_t          curr_amet_lo            = parseInt(pktbuf + 24, 4);
    unsigned int    asc_1pps_amet           = parseInt(pktbuf + 28, 4);
    unsigned int    sc_a_1pps_amet          = parseInt(pktbuf + 32, 4);
    unsigned int    sc_b_1pps_amet          = parseInt(pktbuf + 36, 4);
    unsigned int    asc_1pps_gps_sec        = parseInt(pktbuf + 40, 4);
    unsigned int    asc_1pps_gps_sub        = parseInt(pktbuf + 44, 4);
    unsigned int    sc_u_1pps_gps_sec       = parseInt(pktbuf + 48, 4);
    unsigned int    sc_u_1pps_gps_sub       = parseInt(pktbuf + 52, 4);
    unsigned int    atlas_config_mask       = parseInt(pktbuf + 64, 2);
    unsigned int        sc_1pps_source      = (atlas_config_mask & 0x4000) >> 14;
    unsigned int        uso_source          = (atlas_config_mask & 0x1000) >> 12;
    unsigned int        gps_sync_source     = (atlas_config_mask & 0x0080) >> 7;
    unsigned int        int_1pps_source     = (atlas_config_mask & 0x001C) >> 2;

    timeStat->lock();
    {
        /* Sanity Check Data */
        if( (curr_amet_lo == 0 && curr_amet_hi == 0) ||
            (asc_1pps_amet == 0) ||
            (sc_a_1pps_amet == 0 && sc_b_1pps_amet == 0) ||
            (asc_1pps_gps_sec == 0 && asc_1pps_gps_sub == 0) ||
            (sc_u_1pps_gps_sec == 0 && sc_u_1pps_gps_sub == 0) )
        {
            mlog(WARNING, "Unable to process SIM Housekeeping packet - invalid data\n");
            mlog(WARNING, "\n%20s%20llu\n%20s%20u\n%20s%20u\n%20s%20u\n%20s%20u\n%20s%20u\n%20s%20u\n%20s%20u\n",
                    "curr_amet_lo:", (unsigned long long)curr_amet_lo,
                    "asc_1pps_amet:", asc_1pps_amet,
                    "sc_a_1pps_amet:", sc_a_1pps_amet,
                    "sc_b_1pps_amet:", sc_b_1pps_amet,
                    "asc_1pps_gps_sec:", asc_1pps_gps_sec,
                    "asc_1pps_gps_sub:", asc_1pps_gps_sub,
                    "sc_u_1pps_gps_sec:", sc_u_1pps_gps_sec,
                    "sc_u_1pps_gps_sub:", sc_u_1pps_gps_sub);
            timeStat->rec->errorcnt++;
        }
        else
        {
            unsigned int findex = timeStat->rec->simhk_sample_index;

            /* Save Off S/C 1PPS GPS Time */
            timeStat->rec->sc_1pps_time        = (double)sc_u_1pps_gps_sec + ((double)sc_u_1pps_gps_sub / TIME_32BIT_FLOAT_MAX_VALUE);
            timeStat->rec->sc_1pps_gps[findex] = timeStat->rec->sc_1pps_time;

            /* Save Off S/C 1PPS AMETs */
            if      (sc_1pps_source == SC_1PPS_A)   timeStat->rec->sc_1pps_amet = sc_a_1pps_amet < curr_amet_lo ? sc_a_1pps_amet + (curr_amet_hi << 32) : sc_a_1pps_amet + ((curr_amet_hi - 1) << 32);
            else if (sc_1pps_source == SC_1PPS_B)   timeStat->rec->sc_1pps_amet = sc_b_1pps_amet < curr_amet_lo ? sc_b_1pps_amet + (curr_amet_hi << 32) : sc_b_1pps_amet + ((curr_amet_hi - 1) << 32);
            timeStat->rec->sc_1pps_amets[findex] = timeStat->rec->sc_1pps_amet;

            /* Calculate ASC 1PPS Time */
            timeStat->rec->asc_1pps_time             = (double)asc_1pps_gps_sec + ((double)asc_1pps_gps_sub / TIME_32BIT_FLOAT_MAX_VALUE);
            timeStat->rec->asc_1pps_gps[findex]      = timeStat->rec->asc_1pps_time;
            timeStat->rec->asc_1pps_amet             = asc_1pps_amet < curr_amet_lo ? asc_1pps_amet + (curr_amet_hi << 32) : asc_1pps_amet + ((curr_amet_hi - 1) << 32);
            timeStat->rec->asc_1pps_amets[findex]    = timeStat->rec->asc_1pps_amet;

            /* Calculate Delta */
            timeStat->rec->sc_to_asc_1pps_amet_delta = timeStat->rec->asc_1pps_amet - timeStat->rec->sc_1pps_amet;

            if(timeStat->rec->simhk_cnt > SAMPLE_HISTORY)
            {
                /* Calculate SC 1PPS Frequency (against USO) */
                double sc_1pps_amet_time_delta = (double)(timeStat->rec->sc_1pps_amets[findex] - timeStat->rec->sc_1pps_amets[(findex + 1) % SAMPLE_HISTORY]);
                double sc_1pps_gps_time_delta = timeStat->rec->sc_1pps_gps[findex] - timeStat->rec->sc_1pps_gps[(findex + 1) % SAMPLE_HISTORY];
                timeStat->rec->sc_1pps_freq = sc_1pps_amet_time_delta / (sc_1pps_gps_time_delta * (double)USO_CNTS_PER_SEC);

                /* Calculate ASC 1PPS Frequency (against GPS) */
                double asc_1pps_gps_time_delta = timeStat->rec->asc_1pps_gps[findex] - timeStat->rec->asc_1pps_gps[(findex + 1) % SAMPLE_HISTORY];
                double asc_1pps_amet_delta = (double)(timeStat->rec->asc_1pps_amets[findex] - timeStat->rec->asc_1pps_amets[(findex + 1) % SAMPLE_HISTORY]);
                timeStat->rec->asc_1pps_freq = asc_1pps_gps_time_delta / (asc_1pps_amet_delta * ((double)1.0 / (double)USO_CNTS_PER_SEC));

                /* Error Check Frequency */
                double gps_seconds = timeStat->rec->sc_1pps_gps[findex] - timeStat->rec->sc_1pps_gps[(findex + 1) % SAMPLE_HISTORY];
                double cnts_per_sec = USO_CNTS_PER_SEC;
                if((gps_seconds < ((double)SAMPLE_HISTORY * .50)) || (gps_seconds > ((double)SAMPLE_HISTORY * 1.50)))
                {
                    mlog(WARNING, "GPS is unstable, cumulated time over %d samples: %lf\n", SAMPLE_HISTORY, gps_seconds);
                    timeStat->rec->errorcnt++;
                    timeStat->rec->uso_freq_calc = false;
                }
                else
                {
                    cnts_per_sec = (asc_1pps_gps_time_delta / gps_seconds) * USO_CNTS_PER_SEC;
                    if(fabs(cnts_per_sec - USO_CNTS_PER_SEC) > 1000.0)
                    {
                        mlog(ERROR, "Unstable measurement of USO... unable to use AMETs; counts per second = %lf\n", cnts_per_sec);
                        timeStat->rec->errorcnt++;
                        timeStat->rec->uso_freq_calc = false;
                    }
                    else
                    {
                        timeStat->rec->uso_freq_calc = true;
                    }
                }

                /* Set Ruler Clock Period and Frequency */
                timeStat->rec->uso_freq = cnts_per_sec; // Hz
                trueRulerClkPeriod = 1000000000.0 / cnts_per_sec; // ns
            }

            /* Set Sources */
            timeStat->rec->sc_1pps_source    = (sc1ppsSource_t)sc_1pps_source;
            timeStat->rec->uso_source        = (usoSource_t)uso_source;
            timeStat->rec->gps_sync_source   = (gpsSyncSource_t)gps_sync_source;
            timeStat->rec->int_1pps_source   = (int1ppsSource_t)int_1pps_source;

            /* Bump Counts */
            timeStat->rec->simhk_sample_index = (timeStat->rec->simhk_sample_index + 1) % SAMPLE_HISTORY;
            timeStat->rec->simhk_cnt++;
            timeStat->rec->statcnt++;
        }
    }
    timeStat->unlock();

    timeStat->post(); // manual post

    return true;
}

/*----------------------------------------------------------------------------
 * parseSxpHkPkt  - Parse SXP Housekeeping Packet
 *
 *   Notes: does not post
 *----------------------------------------------------------------------------*/
bool TimeProcessorModule::parseSxpHkPkt(unsigned char* pktbuf)
{
    /* Parse Packet */
    unsigned int    tq_gps_sec  = parseInt(pktbuf + 72, 4);
    unsigned int    tq_gps_sub  = parseInt(pktbuf + 76, 4);

    timeStat->lock();
    {
        /* Sanity Check Data */
        if(tq_gps_sec == 0)
        {
            mlog(WARNING, "Unable to process SXP Housekeeping packet - invalid data\n");
            mlog(WARNING, "\n%20s%20u\n%20s%20u\n",
                    "tq_gps_sec:", tq_gps_sec,
                    "tq_gps_sub:", tq_gps_sub);
            timeStat->rec->errorcnt++;
        }
        else
        {
            unsigned int findex = timeStat->rec->sxphk_sample_index;

            timeStat->rec->tq_time = (double)tq_gps_sec + ((double)tq_gps_sub / TIME_32BIT_FLOAT_MAX_VALUE);
            timeStat->rec->tq_gps[findex] = timeStat->rec->tq_time;

            if(timeStat->rec->sxphk_cnt > 1)
            {
                double tq_delta =  timeStat->rec->tq_gps[findex] - timeStat->rec->tq_gps[(findex + (SAMPLE_HISTORY - 1)) % SAMPLE_HISTORY];
                timeStat->rec->tq_freq = 1.0 / tq_delta;
            }

            timeStat->rec->sxphk_sample_index = (timeStat->rec->sxphk_sample_index + 1) % SAMPLE_HISTORY;
            timeStat->rec->sxphk_cnt++;
            timeStat->rec->statcnt++;
        }
    }
    timeStat->unlock();

    return true;
}

/*----------------------------------------------------------------------------
 * parseTimekeepingPkt  - Parse PCE Timekeeping Packet
 *
 *   Notes: does not post
 *----------------------------------------------------------------------------*/
bool TimeProcessorModule::parseTimekeepingPkt(unsigned char* pktbuf, int pce)
{
    /* Parse Packet */
    unsigned int    mf_gps_sec  = parseInt(pktbuf + 24, 4);
    unsigned int    mf_gps_sub  = parseInt(pktbuf + 28, 4);
    unsigned int    mf_gps_cnt  = parseInt(pktbuf + 40, 4);

    timeStat->lock();
    {
        /* Sanity Check Data */
        if(mf_gps_sec == 0)
        {
            mlog(WARNING, "Unable to process PCE Timekeeping packet - invalid data\n");
            mlog(WARNING, "\n%20s%20u\n%20s%20u\n%20s%20u\n",
                    "mf_gps_sec:", mf_gps_sec,
                    "mf_gps_sub:", mf_gps_sub,
                    "mf_gps_cnt:", mf_gps_cnt);
            timeStat->rec->errorcnt++;
        }
        else
        {
            unsigned int findex = timeStat->rec->timekeeping_sample_index[pce];

            timeStat->rec->mf_time[pce] = (double)mf_gps_sec + ((double)mf_gps_sub / TIME_32BIT_FLOAT_MAX_VALUE);
            timeStat->rec->mf_gps[pce][findex] = timeStat->rec->mf_time[pce];
            timeStat->rec->mf_ids[pce][findex] = mf_gps_cnt;

            if(timeStat->rec->timekeeping_cnt[pce] > 1)
            {
                double mf_gps_delta = timeStat->rec->mf_gps[pce][findex] - timeStat->rec->mf_gps[pce][(findex + (SAMPLE_HISTORY - 1)) % SAMPLE_HISTORY];
                double mf_cnt_delta = timeStat->rec->mf_ids[pce][findex] - timeStat->rec->mf_ids[pce][(findex + (SAMPLE_HISTORY - 1)) % SAMPLE_HISTORY];
                timeStat->rec->mf_freq[pce] = mf_cnt_delta / mf_gps_delta;
            }

            timeStat->rec->timekeeping_sample_index[pce] = (timeStat->rec->timekeeping_sample_index[pce] + 1) % SAMPLE_HISTORY;
            timeStat->rec->timekeeping_cnt[pce]++;
            timeStat->rec->statcnt++;
        }
    }
    timeStat->unlock();

    return true;
}

/*----------------------------------------------------------------------------
 * parseSxpDiagPkt  - Parse SXP Diagnostic Packet
 *
 *   Notes: POSTS timeDiagStat
 *----------------------------------------------------------------------------*/
bool TimeProcessorModule::parseSxpDiagPkt(unsigned char* pktbuf)
{
    /* Parse Packet */
    uint32_t  position_pkt_rx_gps_time_secs           = parseInt(pktbuf + 16, 4);
    uint32_t  position_pkt_rx_gps_time_subsecs        = parseInt(pktbuf + 20, 4);
    uint32_t  pointing_pkt_rx_gps_time_secs           = parseInt(pktbuf + 24, 4);
    uint32_t  pointing_pkt_rx_gps_time_subsecs        = parseInt(pktbuf + 28, 4);
    uint32_t  pce_pkt_rx_gps_time_PCE1_secs           = parseInt(pktbuf + 32, 4);
    uint32_t  pce_pkt_rx_gps_time_PCE1_subsecs        = parseInt(pktbuf + 36, 4);
    uint32_t  pce_pkt_rx_gps_time_PCE2_secs           = parseInt(pktbuf + 40, 4);
    uint32_t  pce_pkt_rx_gps_time_PCE2_subsecs        = parseInt(pktbuf + 44, 4);
    uint32_t  pce_pkt_rx_gps_time_PCE3_secs           = parseInt(pktbuf + 48, 4);
    uint32_t  pce_pkt_rx_gps_time_PCE3_subsecs        = parseInt(pktbuf + 52, 4);
    uint32_t  position_pkt_sc_solution_time_secs      = parseInt(pktbuf + 56, 4);
    uint32_t  position_pkt_sc_solution_time_counts    = parseInt(pktbuf + 60, 4);
    uint32_t  pointing_pkt_sc_solution_time_secs      = parseInt(pktbuf + 64, 4);
    uint32_t  pointing_pkt_sc_solution_time_counts    = parseInt(pktbuf + 68, 4);

    uint32_t  first_major_frame_id_spot1              = parseInt(pktbuf + 72, 4);  // PCE 1 Strong
    uint32_t  first_major_frame_id_spot3              = parseInt(pktbuf + 80, 4);  // PCE 2 Strong
    uint32_t  first_major_frame_id_spot5              = parseInt(pktbuf + 88, 4);  // PCE 3 Strong

    uint32_t  first_major_frame_gps_secs_spot1        = parseInt(pktbuf + 96, 4);  // PCE 1 Strong
    uint32_t  first_major_frame_gps_subsecs_spot1     = parseInt(pktbuf + 100, 4); // PCE 1 Strong
    uint32_t  first_major_frame_gps_secs_spot3        = parseInt(pktbuf + 112, 4); // PCE 2 Strong
    uint32_t  first_major_frame_gps_subsecs_spot3     = parseInt(pktbuf + 116, 4); // PCE 2 Strong
    uint32_t  first_major_frame_gps_secs_spot5        = parseInt(pktbuf + 128, 4); // PCE 3 Strong
    uint32_t  first_major_frame_gps_subsecs_spot5     = parseInt(pktbuf + 132, 4); // PCE 3 Strong

    uint32_t  MajorFrameGPSTimeSec_PCE1               = parseInt(pktbuf + 144, 4); // PCE 1 Strong
    uint32_t  MajorFrameGPSTimeSubSec_PCE1            = parseInt(pktbuf + 148, 4); // PCE 1 Strong
    uint32_t  MajorFrameGPSTimeSec_PCE2               = parseInt(pktbuf + 152, 4); // PCE 2 Strong
    uint32_t  MajorFrameGPSTimeSubSec_PCE2            = parseInt(pktbuf + 156, 4); // PCE 2 Strong
    uint32_t  MajorFrameGPSTimeSec_PCE3               = parseInt(pktbuf + 160, 4); // PCE 3 Strong
    uint32_t  MajorFrameGPSTimeSubSec_PCE3            = parseInt(pktbuf + 164, 4); // PCE 3 Strong

//    uint32_t  MajorFrameAMETTimeUpper32Bits_PCE1      = parseInt(pktbuf + 168, 4); // PCE 1 Strong
//    uint32_t  MajorFrameAMETTimeLower32Bits_PCE1      = parseInt(pktbuf + 172, 4); // PCE 1 Strong
//    uint32_t  MajorFrameAMETTimeUpper32Bits_PCE2      = parseInt(pktbuf + 176, 4); // PCE 2 Strong
//    uint32_t  MajorFrameAMETTimeLower32Bits_PCE2      = parseInt(pktbuf + 180, 4); // PCE 2 Strong
//    uint32_t  MajorFrameAMETTimeUpper32Bits_PCE3      = parseInt(pktbuf + 184, 4); // PCE 3 Strong
//    uint32_t  MajorFrameAMETTimeLower32Bits_PCE3      = parseInt(pktbuf + 188, 4); // PCE 3 Strong

    uint32_t  MajorFrameCount_PCE1                    = parseInt(pktbuf + 192, 4); // PCE 1 Strong
    uint32_t  MajorFrameCount_PCE2                    = parseInt(pktbuf + 196, 4); // PCE 2 Strong
    uint32_t  MajorFrameCount_PCE3                    = parseInt(pktbuf + 200, 4); // PCE 3 Strong

    double  OnePPSToMajorFrameTime_PCE1             = parseFlt(pktbuf + 208, 8); // PCE 1 Strong
    double  OnePPSToMajorFrameTime_PCE2             = parseFlt(pktbuf + 216, 8); // PCE 2 Strong
    double  OnePPSToMajorFrameTime_PCE3             = parseFlt(pktbuf + 224, 8); // PCE 3 Strong

    double  asc_1pps_to_sc_1pps_delta_secs          = parseFlt(pktbuf + 232, 8); // this is an AMET value converted to double precision floating point
    uint32_t  gps_of_sc_1pps_secs                     = parseInt(pktbuf + 240, 4);
    uint32_t  gps_of_sc_1pps_subSecs                  = parseInt(pktbuf + 244, 4);
    uint32_t  sc_of_sc_1pps_secs                      = parseInt(pktbuf + 248, 4);
    uint32_t  sc_of_sc_1pps_counts                    = parseInt(pktbuf + 252, 4);
    uint32_t  gps_of_sc_tat_arrival_secs              = parseInt(pktbuf + 256, 4);
    uint32_t  gps_of_sc_tat_arrival_subSecs           = parseInt(pktbuf + 260, 4);
    uint32_t  gps_of_asc_1pps_secs                    = parseInt(pktbuf + 264, 4);
    uint32_t  gps_of_asc_1pps_subSecs                 = parseInt(pktbuf + 268, 4);

    int     sxp_extrap1_status                      = parseInt(pktbuf + 272, 1);
    int     sxp_extrap2_status                      = parseInt(pktbuf + 273, 1);
    int     sxp_extrap3_status                      = parseInt(pktbuf + 274, 1);
    int     sxp_extrap4_status                      = parseInt(pktbuf + 275, 1);
    int     sxp_extrap5_status                      = parseInt(pktbuf + 276, 1);
    int     sxp_extrap6_status                      = parseInt(pktbuf + 277, 1);

    timeDiagStat->lock();
    {
        /* Set Extrapolation Status */
        timeDiagStat->rec->sxp_status[0] = sxp_extrap1_status;
        timeDiagStat->rec->sxp_status[1] = sxp_extrap2_status;
        timeDiagStat->rec->sxp_status[2] = sxp_extrap3_status;
        timeDiagStat->rec->sxp_status[3] = sxp_extrap4_status;
        timeDiagStat->rec->sxp_status[4] = sxp_extrap5_status;
        timeDiagStat->rec->sxp_status[5] = sxp_extrap6_status;

        /* GPS Deltas */
        if(diagTimeRef == TIME_REF_ASC_1PPS_GPS)
        {
            /* Calculate GPS Times */
            double asc_1pps_gps         = (double)gps_of_asc_1pps_secs + ((double)gps_of_asc_1pps_subSecs / TIME_32BIT_FLOAT_MAX_VALUE);

            double sc_1pps_gps          = (double)gps_of_sc_1pps_secs           + ((double)gps_of_sc_1pps_subSecs           / TIME_32BIT_FLOAT_MAX_VALUE);
            double sc_tat_rx_gps        = (double)gps_of_sc_tat_arrival_secs    + ((double)gps_of_sc_tat_arrival_subSecs    / TIME_32BIT_FLOAT_MAX_VALUE);
            double sc_att_rx_gps        = (double)pointing_pkt_rx_gps_time_secs + ((double)pointing_pkt_rx_gps_time_subsecs / TIME_32BIT_FLOAT_MAX_VALUE);
            double sc_pos_rx_gps        = (double)position_pkt_rx_gps_time_secs + ((double)position_pkt_rx_gps_time_subsecs / TIME_32BIT_FLOAT_MAX_VALUE);

            double sc2gps_offset        = sc_1pps_gps - ((double)sc_of_sc_1pps_secs + ((double)sc_of_sc_1pps_counts * 0.000001));

            double sc_att_sol_gps       = sc2gps_offset + ((double)pointing_pkt_sc_solution_time_secs + ((double)pointing_pkt_sc_solution_time_counts * 0.000001));
            double sc_pos_sol_gps       = sc2gps_offset + ((double)position_pkt_sc_solution_time_secs + ((double)position_pkt_sc_solution_time_counts * 0.000001));

            double sxp_pce_time_rx_gps[NUM_PCES];
            sxp_pce_time_rx_gps[0]      = (double)pce_pkt_rx_gps_time_PCE1_secs + ((double)pce_pkt_rx_gps_time_PCE1_subsecs / TIME_32BIT_FLOAT_MAX_VALUE);
            sxp_pce_time_rx_gps[1]      = (double)pce_pkt_rx_gps_time_PCE2_secs + ((double)pce_pkt_rx_gps_time_PCE2_subsecs / TIME_32BIT_FLOAT_MAX_VALUE);
            sxp_pce_time_rx_gps[2]      = (double)pce_pkt_rx_gps_time_PCE3_secs + ((double)pce_pkt_rx_gps_time_PCE3_subsecs / TIME_32BIT_FLOAT_MAX_VALUE);

            double sxp_1st_mf_extrap_gps[NUM_PCES];
            sxp_1st_mf_extrap_gps[0]    = (double)first_major_frame_gps_secs_spot1 + ((double)first_major_frame_gps_subsecs_spot1 / TIME_32BIT_FLOAT_MAX_VALUE);
            sxp_1st_mf_extrap_gps[1]    = (double)first_major_frame_gps_secs_spot3 + ((double)first_major_frame_gps_subsecs_spot3 / TIME_32BIT_FLOAT_MAX_VALUE);
            sxp_1st_mf_extrap_gps[2]    = (double)first_major_frame_gps_secs_spot5 + ((double)first_major_frame_gps_subsecs_spot5 / TIME_32BIT_FLOAT_MAX_VALUE);

            double pce_1st_mf_1pps_gps[NUM_PCES];
            pce_1st_mf_1pps_gps[0]      = (double)MajorFrameGPSTimeSec_PCE1 + ((double)MajorFrameGPSTimeSubSec_PCE1 / TIME_32BIT_FLOAT_MAX_VALUE);
            pce_1st_mf_1pps_gps[1]      = (double)MajorFrameGPSTimeSec_PCE2 + ((double)MajorFrameGPSTimeSubSec_PCE2 / TIME_32BIT_FLOAT_MAX_VALUE);
            pce_1st_mf_1pps_gps[2]      = (double)MajorFrameGPSTimeSec_PCE3 + ((double)MajorFrameGPSTimeSubSec_PCE3 / TIME_32BIT_FLOAT_MAX_VALUE);

            /* Populate Delta Times */
            timeDiagStat->rec->sc_1pps_delta     = sc_1pps_gps       - asc_1pps_gps;
            timeDiagStat->rec->sc_tat_rx_delta   = sc_tat_rx_gps     - asc_1pps_gps;
            timeDiagStat->rec->sc_att_rx_delta   = sc_att_rx_gps     - asc_1pps_gps;
            timeDiagStat->rec->sc_pos_rx_delta   = sc_pos_rx_gps     - asc_1pps_gps;
            timeDiagStat->rec->sc_att_sol_delta  = sc_att_sol_gps    - asc_1pps_gps;
            timeDiagStat->rec->sc_pos_sol_delta  = sc_pos_sol_gps    - asc_1pps_gps;

            for(int p = 0; p < NUM_PCES; p++)
            {
                timeDiagStat->rec->sxp_pce_time_rx_delta[p]      = sxp_pce_time_rx_gps[p]    - asc_1pps_gps;
                timeDiagStat->rec->sxp_1st_mf_extrap_delta[p]    = sxp_1st_mf_extrap_gps[p]  - asc_1pps_gps;
                timeDiagStat->rec->pce_1st_mf_1pps_delta[p]      = pce_1st_mf_1pps_gps[p]    - asc_1pps_gps;
            }

            timeDiagStat->rec->asc_1pps_gps_ref  = asc_1pps_gps;
            timeDiagStat->rec->ref = TIME_REF_ASC_1PPS_GPS;
        }
        /* AMET Deltas */
        else if(diagTimeRef == TIME_REF_ASC_1PPS_AMET)
        {
            /* Populate Delta Times */
            if(asc_1pps_to_sc_1pps_delta_secs < 1.0)
            {
                timeDiagStat->rec->sc_1pps_delta = asc_1pps_to_sc_1pps_delta_secs * -1.0;
            }
            else
            {
                timeDiagStat->rec->sc_1pps_delta = asc_1pps_to_sc_1pps_delta_secs - 1.0;
            }

            timeDiagStat->rec->sxp_1st_mf_extrap_delta[0] = OnePPSToMajorFrameTime_PCE1 + ((first_major_frame_id_spot1 - MajorFrameCount_PCE1) * 0.020);
            timeDiagStat->rec->sxp_1st_mf_extrap_delta[1] = OnePPSToMajorFrameTime_PCE2 + ((first_major_frame_id_spot3 - MajorFrameCount_PCE2) * 0.020);
            timeDiagStat->rec->sxp_1st_mf_extrap_delta[2] = OnePPSToMajorFrameTime_PCE3 + ((first_major_frame_id_spot5 - MajorFrameCount_PCE3) * 0.020);

            timeDiagStat->rec->pce_1st_mf_1pps_delta[0] = OnePPSToMajorFrameTime_PCE1;
            timeDiagStat->rec->pce_1st_mf_1pps_delta[1] = OnePPSToMajorFrameTime_PCE2;
            timeDiagStat->rec->pce_1st_mf_1pps_delta[2] = OnePPSToMajorFrameTime_PCE3;

            timeDiagStat->rec->ref = TIME_REF_ASC_1PPS_AMET;
        }
    }
    timeDiagStat->unlock();

    timeDiagStat->post();

    return true;
}

/*----------------------------------------------------------------------------
 * attachSimHkApidCmd
 *----------------------------------------------------------------------------*/
int TimeProcessorModule::attachSimHkApidCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    simHkApid = (uint16_t)strtol(argv[0], NULL, 0);

    return 0;
}

/*----------------------------------------------------------------------------
 * attachSxpHkApidCmd
 *----------------------------------------------------------------------------*/
int TimeProcessorModule::attachSxpHkApidCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    sxpHkApid = (uint16_t)strtol(argv[0], NULL, 0);

    return 0;
}

/*----------------------------------------------------------------------------
 * attachTimekeepingApidCmd
 *----------------------------------------------------------------------------*/
int TimeProcessorModule::attachTimekeepingApidCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    timekeepingApid[0] = (uint16_t)strtol(argv[0], NULL, 0);
    timekeepingApid[1] = (uint16_t)strtol(argv[1], NULL, 0);
    timekeepingApid[2] = (uint16_t)strtol(argv[2], NULL, 0);

    return 0;
}

/*----------------------------------------------------------------------------
 * attachSxpDiagApidCmd
 *----------------------------------------------------------------------------*/
int TimeProcessorModule::attachSxpDiagApidCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    sxpDiagApid = (uint16_t)strtol(argv[0], NULL, 0);

    return 0;
}

/*----------------------------------------------------------------------------
 * setSxpDiagTimeRefCmd
 *----------------------------------------------------------------------------*/
int TimeProcessorModule::setSxpDiagTimeRefCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    if(strcmp(argv[0], "GPS") == 0)
    {
        sxpDiagApid = TIME_REF_ASC_1PPS_GPS;
    }
    else if(strcmp(argv[0], "AMET") == 0)
    {
        sxpDiagApid = TIME_REF_ASC_1PPS_AMET;
    }
    else
    {
        mlog(CRITICAL, "Invalid time reference supplied %s\n", argv[0]);
        return -1;
    }

    return 0;
}
