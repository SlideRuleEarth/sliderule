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

#include "BlinkProcessorModule.h"
#include "TimeProcessorModule.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double BlinkProcessorModule::DEFAULT_10NS_PERIOD = 10.0;

const char* BlinkProcessorModule::true10Key = "true10ns";

const char* BlinkStat::rec_type = "BlinkStat";

RecordObject::fieldDef_t BlinkStat::rec_def[] =
{
    {"MFC",               UINT64, offsetof(blinkStat_t, mfc),         1,    NULL, NATIVE_FLAGS},
    {"SHOT",              UINT8,  offsetof(blinkStat_t, shot),        1,    NULL, NATIVE_FLAGS},
    {"RX_CNT",            UINT32, offsetof(blinkStat_t, rxcnt),       1,    NULL, NATIVE_FLAGS},
    {"TX_SC_GPS",         DOUBLE, offsetof(blinkStat_t, tx_sc_gps),   1,    NULL, NATIVE_FLAGS},
    {"TX_ASC_GPS",        DOUBLE, offsetof(blinkStat_t, tx_asc_gps),  1,    NULL, NATIVE_FLAGS},
    {"TX_SXP_GPS",        DOUBLE, offsetof(blinkStat_t, tx_sxp_gps),  1,    NULL, NATIVE_FLAGS},
    {"TX_PCE_GPS",        DOUBLE, offsetof(blinkStat_t, tx_pce_gps),  1,    NULL, NATIVE_FLAGS}
};

int BlinkStat::rec_elem = sizeof(BlinkStat::rec_def) / sizeof(RecordObject::fieldDef_t);

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BlinkStat::BlinkStat(CommandProcessor* cmd_proc, const char* stat_name): StatisticRecord<blinkStat_t>(cmd_proc, stat_name, rec_type, false)
{
    setClear(CLEAR_ALWAYS);
    cmdProc->registerObject(stat_name, this);
}

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
BlinkProcessorModule::BlinkProcessorModule(CommandProcessor* cmd_proc, const char* obj_name, const char* time_proc_name):
    CcsdsProcessorModule(cmd_proc, obj_name)
{
    assert(time_proc_name);

    trueRulerClkPeriod = DEFAULT_10NS_PERIOD;
    darkZeroCount = 0;

    /* Post Initial Values to Current Value Table */
    cmdProc->setCurrentValue(getName(), true10Key, (void*)&trueRulerClkPeriod,  sizeof(trueRulerClkPeriod));

    /* Set Time Statistic Processor Name */
    timeStatName = StringLib::concat(time_proc_name, ".", TimeStat::rec_type);

    /* Define Statistic Record */
    BlinkStat::defineRecord(BlinkStat::rec_type, NULL, sizeof(blinkStat_t), BlinkStat::rec_def, BlinkStat::rec_elem, 32);

    /* Initialize Statistics */
    char stat_name[MAX_STAT_NAME_SIZE];
    blinkStat = new BlinkStat(cmd_proc, StringLib::format(stat_name, MAX_STAT_NAME_SIZE, "%s.%s", obj_name, BlinkStat::rec_type));
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
BlinkProcessorModule::~BlinkProcessorModule(void)
{
    // --- these must be deleted via CommandableObject
    //  delete blinkStat;

    delete [] timeStatName;
}

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* BlinkProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* time_proc_name = StringLib::checkNullStr(argv[0]);
    if(time_proc_name == NULL)
    {
        mlog(CRITICAL, "Must supply valid time processor name!\n");
        return NULL;
    }

    return new BlinkProcessorModule(cmd_proc, name, time_proc_name);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processSegments  - Parser for Science Time Tag Telemetry Packet
 *
 *   Notes: ALT
 *----------------------------------------------------------------------------*/
bool BlinkProcessorModule::processSegments(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    (void)numpkts;

    /* Data */
    bool    post_stat   = false;
    double  cvr         = 0.0;      // calibration value rising
    double  mf_sc_gps   = 0.0;
    double  mf_asc_gps  = 0.0;
    uint64_t  mfc         = 0;
    uint32_t  shot        = 0;
    uint32_t  rx_cnt      = 0;

    /*-----------------*/
    /* Process Segment */
    /*-----------------*/

    int numsegs = segments.length();
    for(int p = 0; p < numsegs; p++)
    {
        /* Get Segment */
        unsigned char*                  pktbuf  = segments[p]->getBuffer();
        CcsdsSpacePacket::seg_flags_t   seg     = segments[p]->getSEQFLG();
        int                             len     = segments[p]->getLEN();;

        /* Process Segments */
        if(seg == CcsdsSpacePacket::SEG_START)
        {
            shot = 0;
            post_stat = false;

            /* Read Out Header Fields */
            mfc             = parseInt(pktbuf + 12, 4);
            uint64_t amet     = parseInt(pktbuf + 16, 8);
            cvr             = trueRulerClkPeriod / (parseInt(pktbuf + 24, 2) / 256.0); // ns

            /* Handle S/C and ASC GPS Time */
            timeStat_t time_stat;
            if(cmdProc->getCurrentValue(timeStatName, "cv", &time_stat ,sizeof(timeStat_t)) > 0)
            {
                if(time_stat.uso_freq_calc == true)
                {
                    /* Set SC GPS Time */
                    int64_t sc_amet_delta = amet - time_stat.sc_1pps_amet;
                    double sc_time_delta = ((double)sc_amet_delta * trueRulerClkPeriod) / 1000000000.0;
                    mf_sc_gps = time_stat.sc_1pps_time + sc_time_delta;

                    /* Set ASC GPS Time */
                    int64_t asc_amet_delta = amet - time_stat.asc_1pps_amet;
                    double asc_time_delta = ((double)asc_amet_delta * trueRulerClkPeriod) / 1000000000.0;
                    mf_asc_gps = time_stat.asc_1pps_time + asc_time_delta;

                    /* Set Post Stat */
                    post_stat = true;
                }
            }
        }
        else /* Process Continuation and End Segments */
        {
            /* Process Time Tags in Segment */
            long i = 12;
            while(i < len)
            {
                /* Read Channel */
                long channel = (parseInt(pktbuf + i, 1) & 0xF8) >> 3;

                /* Transmit Pulse */
                if(channel >= 24 && channel <= 27)
                {
                    /* Check if Blink Detected and Post */
                    if(darkZeroCount >= DARK_ZERO_THRESHOLD && rx_cnt != 0)
                    {
                        blinkStat->rec->mfc = mfc;
                        blinkStat->rec->shot = shot;
                        if(post_stat)
                        {
                            blinkStat->post();
                        }
                    }

                    /* Parse Transmit Pulse */
                    unsigned long tag               = parseInt(pktbuf + i, 4); i += 4;
                    unsigned long leading_coarse    = ((tag & 0x001FFF80) >> 7) + TransmitPulseCoarseCorrection;
                    unsigned long leading_fine      = (tag  & 0x0000007F) >> 0;
                    double        t0_time           = shot * 10000 * trueRulerClkPeriod; // ns
                    double        tx_time           = ((leading_coarse * trueRulerClkPeriod) - (leading_fine * cvr) + t0_time) * 0.000000001; // sec

                    /* Set Times */
                    blinkStat->rec->tx_sc_gps = mf_sc_gps + tx_time;
                    blinkStat->rec->tx_asc_gps = mf_asc_gps + tx_time;

                    /* Update Counters */
                    if(rx_cnt == 0) darkZeroCount++;
                    else            darkZeroCount = 0;
                    blinkStat->rec->rxcnt = rx_cnt;
                    rx_cnt = 0;
                    shot++;
                }
                /* Strong Return Pulse - BLINKED */
                else if(channel >= 1 && channel <= 16)
                {
                    rx_cnt++; // only count the strong
                    i += 3;
                }
                /* Weak Return Pulse - NOT BLINKED */
                else if(channel >= 17 && channel <= 20)
                {
                    i += 3;
                }
                /* Termination */
                else if(channel == 28)
                {
                    i += 3; // Do Nothing - skip past truncation tag
                }
                else
                {
                    i += 1; // Do Nothing - skip past padding
                }
            }
        }
    }

    return true;
}
