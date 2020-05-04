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

#include "ReportProcessorStatistic.h"
#include "TimeTagProcessorModule.h"
#include "TimeProcessorModule.h"
#include "BceProcessorModule.h"
#include "LaserProcessorModule.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ReportProcessorStatistic::rec_type = "reportStat";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *
 *   Notes: Private constructor due to non-permanent CommandableObject... this is
 *          called through CommandableObject::createObject method and therefore
 *          the object's deallocation is managed by CommandableObject
 *----------------------------------------------------------------------------*/
ReportProcessorStatistic::ReportProcessorStatistic(CommandProcessor* cmd_proc, const char* obj_name,
                                                    const char* ttproc_name[NUM_PCES],
                                                    const char* timeproc_name,
                                                    const char* bceproc_name,
                                                    const char* laserproc_name):
    StatisticRecord<reportStat_t>(cmd_proc, obj_name, rec_type)
{
    /* Initialize Data */
    liveFilename = NULL;

    for(int p = 0; p < NUM_PCES; p++)
    {
        chName[p] = StringLib::concat(ttproc_name[p], ".", ChStat::rec_type);
        txName[p] = StringLib::concat(ttproc_name[p], ".", TxStat::rec_type);
        sigName[p] = StringLib::concat(ttproc_name[p], ".", SigStat::rec_type);
    }

    timeProcName = StringLib::duplicate(timeproc_name);
    bceProcName = StringLib::duplicate(bceproc_name);
    laserProcName = StringLib::duplicate(laserproc_name);
    bceStatName = StringLib::concat(bceProcName, ".", BceStat::rec_type);

    /* Register Commands */
    registerCommand("GENERATE_REPORT",       (cmdFunc_t)&ReportProcessorStatistic::generateReportCmd,      1, "<filename>");
    registerCommand("GENERATE_FULL_REPORT",  (cmdFunc_t)&ReportProcessorStatistic::generateFullReportCmd,  1, "<filename>");
    registerCommand("START_LIVE_FILE",       (cmdFunc_t)&ReportProcessorStatistic::startLiveFileCmd,       1, "<filename>");
    registerCommand("STOP_LIVE_FILE",        (cmdFunc_t)&ReportProcessorStatistic::stopLiveFileCmd,        0, "");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ReportProcessorStatistic::~ReportProcessorStatistic(void)
{
    stopTelemetry();
    if(liveFilename != NULL) delete [] liveFilename;
    for(int p = 0; p < NUM_PCES; p++)
    {
        delete [] chName[p];
        delete [] txName[p];
        delete [] sigName[p];
    }

    delete [] timeProcName;
    delete [] bceProcName;
    delete [] laserProcName;
}

/*----------------------------------------------------------------------------
 * prepost  -
 *----------------------------------------------------------------------------*/
void ReportProcessorStatistic::prepost(void)
{
    /* --------------------- */
    /* Initialize Statistics */
    /* --------------------- */

    sigStat_t   tt_sig_stats[NUM_PCES];
    bceStat_t   bce_stat;
    double      pri_laser_energy;
    double      red_laser_energy;

    /* ------------------ */
    /* Get Current Values */
    /* ------------------ */

    for(int p = 0; p < NUM_PCES; p++)
    {
        if(cmdProc->getCurrentValue(sigName[p], "cv", &tt_sig_stats[p], sizeof(sigStat_t)) <= 0)
        {
            mlog(WARNING, "Unable to get signal stats %s!\n",  sigName[p]);
            memset(&tt_sig_stats[p], 0, sizeof(sigStat_t));
        }
    }

    if(cmdProc->getCurrentValue(laserProcName, LaserProcessorModule::primaryLaserEnergyKey, &pri_laser_energy, sizeof(pri_laser_energy)) <= 0)
    {
        mlog(WARNING, "Unable to get primary laser energy: %s\n", LaserProcessorModule::primaryLaserEnergyKey);
        pri_laser_energy = 0;
    }

    if(cmdProc->getCurrentValue(laserProcName, LaserProcessorModule::redundantLaserEnergyKey, &red_laser_energy, sizeof(red_laser_energy)) <= 0)
    {
        mlog(WARNING, "Unable to get redundant laser energy: %s\n", LaserProcessorModule::redundantLaserEnergyKey);
        red_laser_energy = 0;
    }

    if(cmdProc->getCurrentValue(bceStatName, "cv", &bce_stat, sizeof(bce_stat)) <= 0)
    {
        mlog(WARNING, "Unable to get BCE statistics: %s\n", bceStatName);
        memset(&bce_stat, 0, sizeof(bce_stat));
    }

    /* --------------- */
    /* Build Statistic */
    /* --------------- */

    rec->statcnt++;

    for(int p = 0; p < NUM_PCES; p++)
    {
        for(int s = 0; s < NUM_SPOTS; s++)
        {
            int i = (p * NUM_SPOTS) + s;
            rec->spot[i].rws      = tt_sig_stats[p].rws[s];
            rec->spot[i].rww      = tt_sig_stats[p].rww[s];
            rec->spot[i].sigrng   = tt_sig_stats[p].sigrng[s];
            rec->spot[i].bkgnd    = tt_sig_stats[p].bkgnd[s];
            rec->spot[i].sigpes   = tt_sig_stats[p].sigpes[s];
            rec->spot[i].teppe    = tt_sig_stats[p].teppe[s];
            rec->spot[i].bcepower = bce_stat.power[i];
            rec->spot[i].bceatten = bce_stat.atten[i];
        }
    }

    rec->prilaserenergy = pri_laser_energy;
    rec->redlaserenergy = red_laser_energy;

    /* --------------- */
    /* Write Live File */
    /* --------------- */

    liveFilenameMut.lock();
    {
        if(liveFilename != NULL)
        {
            FILE* fp = fopen(liveFilename, "w");
            if(fp != NULL)
            {
                writeLiveFile(fp);
                fclose(fp);
            }
            else
            {
                mlog(CRITICAL, "Failed to open file %s\n", liveFilename);
                delete [] liveFilename;
                liveFilename = NULL;
            }
        }
    }
    liveFilenameMut.unlock();
}

/******************************************************************************
 PUBLIC STATIC FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* ReportProcessorStatistic::createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* ttproc_name[NUM_PCES];

                ttproc_name[0]  = StringLib::checkNullStr(argv[0]);
                ttproc_name[1]  = StringLib::checkNullStr(argv[1]);
                ttproc_name[2]  = StringLib::checkNullStr(argv[2]);
    const char* timeproc_name   = StringLib::checkNullStr(argv[3]);
    const char* bceproc_name    = StringLib::checkNullStr(argv[4]);
    const char* laserproc_name  = StringLib::checkNullStr(argv[5]);

    RecordObject::defineRecord(rec_type, NULL, sizeof(reportStat_t), NULL, 0, 32);

    return new ReportProcessorStatistic(cmd_proc, name, ttproc_name, timeproc_name, bceproc_name, laserproc_name);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * writeLiveFile  -
 *----------------------------------------------------------------------------*/
bool ReportProcessorStatistic::writeLiveFile(FILE* fp)
{
    fprintf(fp, "<NAME>,<SPOT>,<VALUE>\n");

    fprintf(fp, "STATCNT,%d,%d\n", 1, rec->statcnt);
    fprintf(fp, "STATCNT,%d,%d\n", 2, rec->statcnt);
    fprintf(fp, "STATCNT,%d,%d\n", 3, rec->statcnt);
    fprintf(fp, "STATCNT,%d,%d\n", 4, rec->statcnt);
    fprintf(fp, "STATCNT,%d,%d\n", 5, rec->statcnt);
    fprintf(fp, "STATCNT,%d,%d\n", 6, rec->statcnt);

    fprintf(fp, "RWS,%d,%.6e\n", 1, rec->spot[0].rws);
    fprintf(fp, "RWS,%d,%.6e\n", 2, rec->spot[1].rws);
    fprintf(fp, "RWS,%d,%.6e\n", 3, rec->spot[2].rws);
    fprintf(fp, "RWS,%d,%.6e\n", 4, rec->spot[3].rws);
    fprintf(fp, "RWS,%d,%.6e\n", 5, rec->spot[4].rws);
    fprintf(fp, "RWS,%d,%.6e\n", 6, rec->spot[5].rws);

    fprintf(fp, "RWW,%d,%.6e\n", 1, rec->spot[0].rww);
    fprintf(fp, "RWW,%d,%.6e\n", 2, rec->spot[1].rww);
    fprintf(fp, "RWW,%d,%.6e\n", 3, rec->spot[2].rww);
    fprintf(fp, "RWW,%d,%.6e\n", 4, rec->spot[3].rww);
    fprintf(fp, "RWW,%d,%.6e\n", 5, rec->spot[4].rww);
    fprintf(fp, "RWW,%d,%.6e\n", 6, rec->spot[5].rww);

    fprintf(fp, "TOF,%d,%.6e\n", 1, rec->spot[0].sigrng);
    fprintf(fp, "TOF,%d,%.6e\n", 2, rec->spot[1].sigrng);
    fprintf(fp, "TOF,%d,%.6e\n", 3, rec->spot[2].sigrng);
    fprintf(fp, "TOF,%d,%.6e\n", 4, rec->spot[3].sigrng);
    fprintf(fp, "TOF,%d,%.6e\n", 5, rec->spot[4].sigrng);
    fprintf(fp, "TOF,%d,%.6e\n", 6, rec->spot[5].sigrng);

    fprintf(fp, "BKGND,%d,%.6e\n", 1, rec->spot[0].bkgnd);
    fprintf(fp, "BKGND,%d,%.6e\n", 2, rec->spot[1].bkgnd);
    fprintf(fp, "BKGND,%d,%.6e\n", 3, rec->spot[2].bkgnd);
    fprintf(fp, "BKGND,%d,%.6e\n", 4, rec->spot[3].bkgnd);
    fprintf(fp, "BKGND,%d,%.6e\n", 5, rec->spot[4].bkgnd);
    fprintf(fp, "BKGND,%d,%.6e\n", 6, rec->spot[5].bkgnd);

    fprintf(fp, "RX,%d,%.6e\n", 1, rec->spot[0].sigpes);
    fprintf(fp, "RX,%d,%.6e\n", 2, rec->spot[1].sigpes);
    fprintf(fp, "RX,%d,%.6e\n", 3, rec->spot[2].sigpes);
    fprintf(fp, "RX,%d,%.6e\n", 4, rec->spot[3].sigpes);
    fprintf(fp, "RX,%d,%.6e\n", 5, rec->spot[4].sigpes);
    fprintf(fp, "RX,%d,%.6e\n", 6, rec->spot[5].sigpes);

    fprintf(fp, "ATTEN,%d,%.6e\n", 1, rec->spot[0].bceatten);
    fprintf(fp, "ATTEN,%d,%.6e\n", 2, rec->spot[1].bceatten);
    fprintf(fp, "ATTEN,%d,%.6e\n", 3, rec->spot[2].bceatten);
    fprintf(fp, "ATTEN,%d,%.6e\n", 4, rec->spot[3].bceatten);
    fprintf(fp, "ATTEN,%d,%.6e\n", 5, rec->spot[4].bceatten);
    fprintf(fp, "ATTEN,%d,%.6e\n", 6, rec->spot[5].bceatten);

    fprintf(fp, "POWER,%d,%.6e\n", 1, rec->spot[0].bcepower);
    fprintf(fp, "POWER,%d,%.6e\n", 2, rec->spot[1].bcepower);
    fprintf(fp, "POWER,%d,%.6e\n", 3, rec->spot[2].bcepower);
    fprintf(fp, "POWER,%d,%.6e\n", 4, rec->spot[3].bcepower);
    fprintf(fp, "POWER,%d,%.6e\n", 5, rec->spot[4].bcepower);
    fprintf(fp, "POWER,%d,%.6e\n", 6, rec->spot[5].bcepower);

    fprintf(fp, "PRILASER,%d,%.6e\n", 7, rec->prilaserenergy);
    fprintf(fp, "REDLASER,%d,%.6e\n", 7, rec->redlaserenergy);

    fprintf(fp, "TEP,%d,%.6e\n", 1, rec->spot[0].teppe);
    fprintf(fp, "TEP,%d,%.6e\n", 2, rec->spot[1].teppe);
    fprintf(fp, "TEP,%d,%.6e\n", 3, rec->spot[2].teppe);
    fprintf(fp, "TEP,%d,%.6e\n", 4, rec->spot[3].teppe);
    fprintf(fp, "TEP,%d,%.6e\n", 5, rec->spot[4].teppe);
    fprintf(fp, "TEP,%d,%.6e\n", 6, rec->spot[5].teppe);

    return true;
}

/*----------------------------------------------------------------------------
 * generateReportCmd
 *----------------------------------------------------------------------------*/
int ReportProcessorStatistic::generateReportCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    char* report_filename = argv[0];

    FILE* fp = fopen(report_filename, "w");
    if(fp == NULL)
    {
        mlog(CRITICAL, "unable to open file: %s\n", argv[0]);
        return -1;
    }

    writeLiveFile(fp);

    fclose(fp);

    return 0;
}

/*----------------------------------------------------------------------------
 * generateFullReportCmd
 *----------------------------------------------------------------------------*/
int ReportProcessorStatistic::generateFullReportCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    /* Open Report File */
    char* report_filename = argv[0];
    FILE* fp = fopen(report_filename, "w");
    if(fp == NULL)
    {
        mlog(CRITICAL, "unable to open file: %s\n", argv[0]);
        return -1;
    }

    /* Get Current Value of True 10ns Period */
    double true_10ns_period = 10.0;
    cmdProc->getCurrentValue(timeProcName, TimeProcessorModule::true10Key, &true_10ns_period, sizeof(true_10ns_period));

    /* Write Report File */
    fprintf(fp, "\n\n");
    fprintf(fp, "------------------------------------------------------\n");
    fprintf(fp, "Transmit Statistics (all times provided in nanoseconds)\n");
    fprintf(fp, "------------------------------------------------------\n");
    for(int pce = 0; pce < NUM_PCES; pce++)
    {
        txStat_t tx_stat;
        if(cmdProc->getCurrentValue(txName[pce], "cv", &tx_stat, sizeof(txStat_t)) > 0)
        {
            fprintf(fp, "\nPCE: %d\n\n", pce + 1);

            fprintf(fp, "STATCNT:    %d\n", tx_stat.statcnt);
            fprintf(fp, "TXCNT:      %d\n", tx_stat.txcnt);
            fprintf(fp, "MINDELTA:   %.3lf\n", tx_stat.min_delta);
            fprintf(fp, "MAXDELTA:   %.3lf\n", tx_stat.max_delta);
            fprintf(fp, "AVGDELTA:   %.3lf\n", tx_stat.avg_delta);
            fprintf(fp, "            %-8s%-8s\n", "STRONG", "WEAK");
            fprintf(fp, "MINTAGS:    %-8d%-8d\n", tx_stat.min_tags[STRONG_SPOT], tx_stat.min_tags[WEAK_SPOT]);
            fprintf(fp, "MAXTAGS:    %-8d%-8d\n", tx_stat.max_tags[STRONG_SPOT], tx_stat.max_tags[WEAK_SPOT]);
            fprintf(fp, "AVGTAGS:    %-8.0lf%-8.0lf\n", tx_stat.avg_tags[STRONG_SPOT], tx_stat.avg_tags[WEAK_SPOT]);
            fprintf(fp, "STDTAGS:    %-8.0lf%-8.0lf\n", tx_stat.std_tags[STRONG_SPOT], tx_stat.std_tags[WEAK_SPOT]);
        }
        else
        {
            mlog(WARNING, "Unable to get tx stats %s!\n", txName[pce]);
        }
    }

    fprintf(fp, "\n\n");
    fprintf(fp, "-----------------------------------------------------\n");
    fprintf(fp, "Signal Statistics (all times provided in nanoseconds)\n");
    fprintf(fp, "-----------------------------------------------------\n");
    for(int pce = 0; pce < NUM_PCES; pce++)
    {
        fprintf(fp, "\nPCE: %d\n\n", pce + 1);

        sigStat_t sig_stat;
        if(cmdProc->getCurrentValue(sigName[pce], "cv", &sig_stat, sizeof(sigStat_t)) > 0)
        {
            fprintf(fp, "         %10s%10s\n", "STRONG", "WEAK");
            fprintf(fp, "STATCNT: %10d%10d\n",          sig_stat.statcnt,                  sig_stat.statcnt);
            fprintf(fp, "RWS:     %10.0lf%10.0lf\n",    sig_stat.rws[STRONG_SPOT],         sig_stat.rws[WEAK_SPOT]);
            fprintf(fp, "RWW:     %10.0lf%10.0lf\n",    sig_stat.rww[STRONG_SPOT],         sig_stat.rww[WEAK_SPOT]);
            fprintf(fp, "TOF:     %10.1lf%10.1lf\n",    sig_stat.sigrng[STRONG_SPOT],      sig_stat.sigrng[WEAK_SPOT]);
            fprintf(fp, "BKGND:   %10.4lf%10.4lf\n",    sig_stat.bkgnd[STRONG_SPOT],       sig_stat.bkgnd[WEAK_SPOT]);
            fprintf(fp, "RX:      %10.4lf%10.4lf\n",    sig_stat.sigpes[STRONG_SPOT],      sig_stat.sigpes[WEAK_SPOT]);
        }
        else
        {
            mlog(WARNING, "Unable to get signal stats %s!\n", sigName[pce]);
        }
    }

    fprintf(fp, "\n\n");
    fprintf(fp, "------------------------------------------------------\n");
    fprintf(fp, "Channel Statistics (all times provided in nanoseconds)\n");
    fprintf(fp, "------------------------------------------------------\n");
    for(int pce = 0; pce < NUM_PCES; pce++)
    {
        chStat_t ch_stat;
        if(cmdProc->getCurrentValue(chName[pce], "cv", &ch_stat, sizeof(chStat_t)) > 0)
        {
            fprintf(fp, "\nPCE: %d\n\n", pce + 1);
            fprintf(fp, "        STATCNT   NUMTAGS   NUMDUPR   TDCCALR   MINCALR   MAXCALR   AVGCALR   NUMDUPF   TDCCALF   MINCALF   MAXCALF   AVGCALF   BIAS      DEADTIME\n");
            for(int i = 0; i < NUM_CHANNELS; i++)
            {
                fprintf(fp, "[%-2d] %10d%10d%10d%10.3lf%10.3lf%10.3lf%10.3lf%10d%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf%10.3lf\n",
                             i+1, ch_stat.statcnt, ch_stat.rx_cnt[i],
                            ch_stat.num_dupr[i], ch_stat.tdc_calr[i], ch_stat.min_calr[i], ch_stat.max_calr[i], ch_stat.avg_calr[i],
                            ch_stat.num_dupr[i], ch_stat.tdc_calf[i], ch_stat.min_calf[i], ch_stat.max_calf[i], ch_stat.avg_calf[i],
                            ch_stat.bias[i], ch_stat.dead_time[i]);
            }

            fprintf(fp, "Delay Chain Calibrations (ns):\n     ");
            for(int i = 0; i < MAX_FINE_COUNT; i++)
            {
                fprintf(fp, "%10d", i);
            }
            fprintf(fp, "\n");
            for(int i = 0; i < NUM_CHANNELS; i++)
            {
                fprintf(fp, "[%-2d] ", i+1);
                for(int c = 0; c < MAX_FINE_COUNT; c++)
                {
                    if(ch_stat.rx_cnt[i] != 0)  fprintf(fp, "%10.3lf", ((double)ch_stat.cell_cnts[i][c] / (double)ch_stat.rx_cnt[i]) * true_10ns_period);
                    else                        fprintf(fp, "%10.3lf", 0.0);
                }
                fprintf(fp, "\n");
            }
        }
        else
        {
            mlog(WARNING, "Unable to get channel stats %s!\n", chName[pce]);
        }
    }

    /* Close Report File */
    fclose(fp);

    return 0;
}


/*----------------------------------------------------------------------------
 * startLiveFileCmd
 *----------------------------------------------------------------------------*/
int ReportProcessorStatistic::startLiveFileCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    liveFilenameMut.lock();
    {
        if(liveFilename != NULL) delete liveFilename;
        liveFilename = StringLib::duplicate(argv[0]);
    }
    liveFilenameMut.unlock();

    return 0;
}

/*----------------------------------------------------------------------------
 * stopLiveFileCmd
 *----------------------------------------------------------------------------*/
int ReportProcessorStatistic::stopLiveFileCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    liveFilenameMut.lock();
    {
        if(liveFilename != NULL) delete liveFilename;
        liveFilename = NULL;
    }
    liveFilenameMut.unlock();

    return 0;
}

