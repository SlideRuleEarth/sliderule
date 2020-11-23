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

#include "TimeTagProcessorModule.h"
#include "TimeProcessorModule.h"
#include "TimeTagHistogram.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

#include <math.h>
#include <float.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/
const double TimeTagProcessorModule::DEFAULT_10NS_PERIOD            = 10.0;
const double TimeTagProcessorModule::DEFAULT_SIGNAL_WIDTH           = 0.0;    // zero indicates auto
const double TimeTagProcessorModule::DEFAULT_GPS_TOLERANCE          = 0.00001;
const double TimeTagProcessorModule::DEFAULT_TEP_LOCATION           = 18.0;   // ns
const double TimeTagProcessorModule::DEFAULT_TEP_WIDTH              = 5.0;    // ns
const double TimeTagProcessorModule::DEFAULT_LOOPBACK_LOCATION      = 75.0;   // ns
const double TimeTagProcessorModule::DEFAULT_LOOPBACK_WIDTH         = 100.0;  // ns

const double TimeTagProcessorModule::DetectorDeadTime               = 1.0;    // nanoseconds
const double TimeTagProcessorModule::MaxFineTimeCal                 = 0.300;  // ns
const double TimeTagProcessorModule::MinFineTimeCal                 = 0.100;  // ns
const double TimeTagProcessorModule::DefaultTimeTagBinSize          = 1.5;    // m

const char* TimeTagProcessorModule::fullColumnIntegrationKey        = "fullColumnIntegration";
const char* TimeTagProcessorModule::autoSetTrueRulerClkPeriodKey    = "autoSetTrueRulerClkPeriod";
const char* TimeTagProcessorModule::blockTepKey                     = "blockTep";
const char* TimeTagProcessorModule::lastGpsKey                      = "lastGps";
const char* TimeTagProcessorModule::lastGpsMfcKey                   = "lastGpsMfc";
const char* TimeTagProcessorModule::signalWidthKey                  = "signalWidthKey";
const char* TimeTagProcessorModule::tepLocationKey                  = "tepLocationKey";
const char* TimeTagProcessorModule::tepWidthKey                     = "tepWidthKey";
const char* TimeTagProcessorModule::loopbackLocationKey             = "loopbackLocationKey";
const char* TimeTagProcessorModule::loopbackWidthKey                = "loopbackWidthKey";

/******************************************************************************
 * PACKET STATISTIC
 ******************************************************************************/

const char* PktStat::rec_type = "PktStat";

RecordObject::fieldDef_t PktStat::rec_def[] =
{
    {"PCE",        UINT32, offsetof(pktStat_t, pce),        1,  NULL, NATIVE_FLAGS},
    {"MFC_ERRORS", UINT32, offsetof(pktStat_t, mfc_errors), 1,  NULL, NATIVE_FLAGS},
    {"HDR_ERRORS", UINT32, offsetof(pktStat_t, hdr_errors), 1,  NULL, NATIVE_FLAGS},
    {"FMT_ERRORS", UINT32, offsetof(pktStat_t, fmt_errors), 1,  NULL, NATIVE_FLAGS},
    {"DLB_ERRORS", UINT32, offsetof(pktStat_t, dlb_errors), 1,  NULL, NATIVE_FLAGS},
    {"TAG_ERRORS", UINT32, offsetof(pktStat_t, tag_errors), 1,  NULL, NATIVE_FLAGS},
    {"PKT_ERRORS", UINT32, offsetof(pktStat_t, pkt_errors), 1,  NULL, NATIVE_FLAGS},
    {"WARNINGS",   UINT32, offsetof(pktStat_t, warnings),   1,  NULL, NATIVE_FLAGS},
    {"SUM_TAGS",   UINT32, offsetof(pktStat_t, sum_tags),   1,  NULL, NATIVE_FLAGS},
    {"MIN_TAGS",   UINT32, offsetof(pktStat_t, min_tags),   1,  NULL, NATIVE_FLAGS},
    {"MAX_TAGS",   UINT32, offsetof(pktStat_t, max_tags),   1,  NULL, NATIVE_FLAGS},
    {"AVG_TAGS",   DOUBLE, offsetof(pktStat_t, avg_tags),   1,  NULL, NATIVE_FLAGS}
};

int PktStat::rec_elem = sizeof(PktStat::rec_def) / sizeof(RecordObject::fieldDef_t);

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
PktStat::PktStat(CommandProcessor* cmd_proc, const char* stat_name): StatisticRecord<pktStat_t>(cmd_proc, stat_name, rec_type)
{
    cmdProc->registerObject(stat_name, this);
}

/******************************************************************************
 * CHANNEL STATISTIC
 ******************************************************************************/

const char* ChStat::rec_type = "ChStat";

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ChStat::ChStat(CommandProcessor* cmd_proc, const char* stat_name): StatisticRecord<chStat_t>(cmd_proc, stat_name, rec_type)
{
    cmdProc->registerObject(stat_name, this);
}

/*----------------------------------------------------------------------------
 * defineRecord
 *----------------------------------------------------------------------------*/
void ChStat::defineRecord(void)
{
    definition_t* def;
    recordDefErr_t status = addDefinition(&def, rec_type, "PCE", sizeof(chStat_t), NULL, 0, 128);

    if(status == SUCCESS_DEF)
    {
        addField(def, "PCE", UINT32, offsetof(chStat_t, pce), 1, NULL, NATIVE_FLAGS);

        for(int i = 0; i < NUM_CHANNELS; i++)
        {
            char field_name[MAX_FIELD_NAME_SIZE];
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "RX_CNT", i),    UINT32, offsetof(chStat_t, rx_cnt) + (i * sizeof(uint32_t)),    1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "NUM_DUPR", i),  UINT32, offsetof(chStat_t, num_dupr) + (i * sizeof(uint32_t)),  1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "NUM_DUPF", i),  UINT32, offsetof(chStat_t, num_dupf) + (i * sizeof(uint32_t)),  1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "CELL_CNTS", i), UINT32, offsetof(chStat_t, cell_cnts) + (i * sizeof(uint32_t)), 1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "TDC_CALR", i),  DOUBLE, offsetof(chStat_t, tdc_calr) + (i * sizeof(double)),    1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "MIN_CALR", i),  DOUBLE, offsetof(chStat_t, min_calr) + (i * sizeof(double)),    1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "MAX_CALR", i),  DOUBLE, offsetof(chStat_t, max_calr) + (i * sizeof(double)),    1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "AVG_CALR", i),  DOUBLE, offsetof(chStat_t, avg_calr) + (i * sizeof(double)),    1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "TDC_CALF", i),  DOUBLE, offsetof(chStat_t, tdc_calf) + (i * sizeof(double)),    1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "MIN_CALF", i),  DOUBLE, offsetof(chStat_t, min_calf) + (i * sizeof(double)),    1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "MAX_CALF", i),  DOUBLE, offsetof(chStat_t, max_calf) + (i * sizeof(double)),    1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "AVG_CALF", i),  DOUBLE, offsetof(chStat_t, avg_calf) + (i * sizeof(double)),    1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "BIAS", i),      DOUBLE, offsetof(chStat_t, bias) + (i * sizeof(double)),        1, NULL, NATIVE_FLAGS);
            addField(def, StringLib::format(field_name, MAX_FIELD_NAME_SIZE, "%s[%d]", "DEAD_TIME", i), DOUBLE, offsetof(chStat_t, dead_time) + (i * sizeof(double)),   1, NULL, NATIVE_FLAGS);
        }
    }
}

/******************************************************************************
 * TRANSMIT STATISTIC
 ******************************************************************************/

const char* TxStat::rec_type = "TxStat";

RecordObject::fieldDef_t TxStat::rec_def[] =
{
    {"PCE",              UINT32, offsetof(txStat_t, pce),         1, NULL, NATIVE_FLAGS},
    {"TXCNT",            UINT32, offsetof(txStat_t, txcnt),       1, NULL, NATIVE_FLAGS},
    {"MIN_TAGS[STRONG]", UINT32, offsetof(txStat_t, min_tags[0]), 1, NULL, NATIVE_FLAGS},
    {"MIN_TAGS[WEAK]",   UINT32, offsetof(txStat_t, min_tags[1]), 1, NULL, NATIVE_FLAGS},
    {"MAX_TAGS[STRONG]", UINT32, offsetof(txStat_t, max_tags[0]), 1, NULL, NATIVE_FLAGS},
    {"MAX_TAGS[WEAK]",   UINT32, offsetof(txStat_t, max_tags[1]), 1, NULL, NATIVE_FLAGS},
    {"AVG_TAGS[STRONG]", DOUBLE, offsetof(txStat_t, avg_tags[0]), 1, NULL, NATIVE_FLAGS},
    {"AVG_TAGS[WEAK]",   DOUBLE, offsetof(txStat_t, avg_tags[1]), 1, NULL, NATIVE_FLAGS},
    {"STD_TAGS[STRONG]", DOUBLE, offsetof(txStat_t, std_tags[0]), 1, NULL, NATIVE_FLAGS},
    {"STD_TAGS[WEAK]",   DOUBLE, offsetof(txStat_t, std_tags[1]), 1, NULL, NATIVE_FLAGS},
    {"MIN_DELTA",        DOUBLE, offsetof(txStat_t, min_delta),   1, NULL, NATIVE_FLAGS},
    {"MAX_DELTA",        DOUBLE, offsetof(txStat_t, max_delta),   1, NULL, NATIVE_FLAGS},
    {"AVG_DELTA",        DOUBLE, offsetof(txStat_t, avg_delta),   1, NULL, NATIVE_FLAGS}
};

int TxStat::rec_elem = sizeof(TxStat::rec_def) / sizeof(RecordObject::fieldDef_t);

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
TxStat::TxStat(CommandProcessor* cmd_proc, const char* stat_name): StatisticRecord<txStat_t>(cmd_proc, stat_name, rec_type)
{
    cmdProc->registerObject(stat_name, this);
}

/******************************************************************************
 * SIGNAL STATISTIC
 ******************************************************************************/

const char* SigStat::rec_type = "SigStat";

RecordObject::fieldDef_t SigStat::rec_def[] =
{
    {"PCE",            UINT32, offsetof(sigStat_t, pce),          1,    NULL, NATIVE_FLAGS},
    {"RWS[STRONG]",    DOUBLE, offsetof(sigStat_t, rws[0]),       1,    NULL, NATIVE_FLAGS},
    {"RWS[WEAK]",      DOUBLE, offsetof(sigStat_t, rws[1]),       1,    NULL, NATIVE_FLAGS},
    {"RWW[STRONG]",    DOUBLE, offsetof(sigStat_t, rww[0]),       1,    NULL, NATIVE_FLAGS},
    {"RWW[WEAK]",      DOUBLE, offsetof(sigStat_t, rww[1]),       1,    NULL, NATIVE_FLAGS},
    {"SIGRNG[STRONG]", DOUBLE, offsetof(sigStat_t, sigrng[0]),    1,    NULL, NATIVE_FLAGS},
    {"SIGRNG[WEAK]",   DOUBLE, offsetof(sigStat_t, sigrng[1]),    1,    NULL, NATIVE_FLAGS},
    {"BKGND[STRONG]",  DOUBLE, offsetof(sigStat_t, bkgnd[0]),     1,    NULL, NATIVE_FLAGS},
    {"BKGND[WEAK]",    DOUBLE, offsetof(sigStat_t, bkgnd[1]),     1,    NULL, NATIVE_FLAGS},
    {"SIGPES[STRONG]", DOUBLE, offsetof(sigStat_t, sigpes[0]),    1,    NULL, NATIVE_FLAGS},
    {"SIGPES[WEAK]",   DOUBLE, offsetof(sigStat_t, sigpes[1]),    1,    NULL, NATIVE_FLAGS},
    {"TEPPE[STRONG]",  DOUBLE, offsetof(sigStat_t, teppe[0]),     1,    NULL, NATIVE_FLAGS},
    {"TEPPE[WEAK]",    DOUBLE, offsetof(sigStat_t, teppe[1]),     1,    NULL, NATIVE_FLAGS},
};

int SigStat::rec_elem = sizeof(SigStat::rec_def) / sizeof(RecordObject::fieldDef_t);

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SigStat::SigStat(CommandProcessor* cmd_proc, const char* stat_name): StatisticRecord<sigStat_t>(cmd_proc, stat_name, rec_type)
{
    cmdProc->registerObject(stat_name, this);
}

/******************************************************************************
 * TIME TAG PROCESSOR MODULE: PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
TimeTagProcessorModule::TimeTagProcessorModule(CommandProcessor* cmd_proc, const char* obj_name, int pcenum, const char* histq_name, const char* txtimeq_name):
    CcsdsProcessorModule(cmd_proc, obj_name),
    pce(pcenum)
{
    assert(histq_name);

    /* Initialize Parameters */
    RemoveDuplicates            = true;
    TrueRulerClkPeriod          = DEFAULT_10NS_PERIOD;
    SignalWidth                 = DEFAULT_SIGNAL_WIDTH;
    Correction                  = LOOPBACK;
    FullColumnIntegration       = false;
    AutoSetTrueRulerClkPeriod   = false;
    GpsAccuracyTolerance        = DEFAULT_GPS_TOLERANCE;
    TepLocation                 = DEFAULT_TEP_LOCATION;
    TepWidth                    = DEFAULT_TEP_WIDTH;
    LoopbackLocation            = DEFAULT_LOOPBACK_LOCATION;
    LoopbackWidth               = DEFAULT_LOOPBACK_WIDTH;
    BlockTep                    = true;
    TimeTagBinSize              = DefaultTimeTagBinSize;
    TimeTagZoomOffset           = 0.0;
    LastGps                     = 0.0;
    LastGpsMfc                  = 0;
    BuildUpMfc                  = false;
    BuildUpMfcCount             = 0;

    /* Post Initial Values to Current Value Table */
    cmdProc->setCurrentValue(getName(), fullColumnIntegrationKey,       (void*)&FullColumnIntegration,      sizeof(FullColumnIntegration));
    cmdProc->setCurrentValue(getName(), autoSetTrueRulerClkPeriodKey,   (void*)&AutoSetTrueRulerClkPeriod,  sizeof(AutoSetTrueRulerClkPeriod));
    cmdProc->setCurrentValue(getName(), blockTepKey,                    (void*)&BlockTep,                   sizeof(BlockTep));
    cmdProc->setCurrentValue(getName(), lastGpsKey,                     (void*)&LastGps,                    sizeof(LastGps));
    cmdProc->setCurrentValue(getName(), lastGpsMfcKey,                  (void*)&LastGpsMfc,                 sizeof(LastGpsMfc));
    cmdProc->setCurrentValue(getName(), signalWidthKey,                 (void*)&SignalWidth,                sizeof(SignalWidth));
    cmdProc->setCurrentValue(getName(), tepLocationKey,                 (void*)&TepLocation,                sizeof(TepLocation));
    cmdProc->setCurrentValue(getName(), tepWidthKey,                    (void*)&TepWidth,                   sizeof(TepWidth));
    cmdProc->setCurrentValue(getName(), loopbackLocationKey,            (void*)&LoopbackLocation,           sizeof(LoopbackLocation));
    cmdProc->setCurrentValue(getName(), loopbackWidthKey,               (void*)&LoopbackWidth,              sizeof(LoopbackWidth));

    /* Define Statistic Records */
    PktStat::defineRecord(PktStat::rec_type, "PCE", sizeof(pktStat_t), PktStat::rec_def, PktStat::rec_elem, 64);
    ChStat::defineRecord(); // dynamically defined
    TxStat::defineRecord(TxStat::rec_type,   "PCE", sizeof(txStat_t),  TxStat::rec_def,  TxStat::rec_elem,  64);
    SigStat::defineRecord(SigStat::rec_type, "PCE", sizeof(sigStat_t), SigStat::rec_def, SigStat::rec_elem, 64);

    /* Initialize Statistics */
    char stat_name[MAX_STAT_NAME_SIZE];
    pktStat = new PktStat(cmd_proc, StringLib::format(stat_name, MAX_STAT_NAME_SIZE, "%s.%s", obj_name, PktStat::rec_type));
    chStat  = new ChStat (cmd_proc, StringLib::format(stat_name, MAX_STAT_NAME_SIZE, "%s.%s", obj_name, ChStat::rec_type));
    txStat  = new TxStat (cmd_proc, StringLib::format(stat_name, MAX_STAT_NAME_SIZE, "%s.%s", obj_name, TxStat::rec_type));
    sigStat = new SigStat(cmd_proc, StringLib::format(stat_name, MAX_STAT_NAME_SIZE, "%s.%s", obj_name, SigStat::rec_type));

    /* Set PCE Number in Statistics */
    pktStat->rec->pce = pce;
    chStat->rec->pce = pce;
    txStat->rec->pce = pce;
    sigStat->rec->pce = pce;

    /* Initialize Channel Disables */
    memset(channelDisable, 0, sizeof(channelDisable));

    /* Processor Names */
    majorFrameProcName = NULL;
    timeProcName = NULL;
    timeStatName = NULL;

    /* Initialize Result File */
    resultFile = NULL;

    /* Initialize Streams */
    histQ   = new Publisher(histq_name);
    txTimeQ = new Publisher(txtimeq_name);

    /* Initialize Time Tag Histogram Record Definitions */
    TimeTagHistogram::defineHistogram();

    /* Register Commands */
    registerCommand("REMOVE_DUPLICATES",         (cmdFunc_t)&TimeTagProcessorModule::removeDuplicatesCmd,     1, "<true|false>");
    registerCommand("SET_CLK_PERIOD",            (cmdFunc_t)&TimeTagProcessorModule::setClkPeriodCmd,         1, "<period>");
    registerCommand("SET_SIGNAL_WIDTH",          (cmdFunc_t)&TimeTagProcessorModule::setSignalWidthCmd,       1, "<width in ns>");
    registerCommand("SET_CORRECTION",            (cmdFunc_t)&TimeTagProcessorModule::setCorrectionCmd,       -1, "<UNCORRECTED|LOOPBACK [<location in ns> <width in ns>]>");
    registerCommand("FULL_COL_MODE",             (cmdFunc_t)&TimeTagProcessorModule::fullColumnModeCmd,       1, "<ENABLE|DISABLE>");
    registerCommand("SET_TT_BINSIZE",            (cmdFunc_t)&TimeTagProcessorModule::ttBinsizeCmd,            1, "<binsize in ns | REVERT>");
    registerCommand("SET_TT_ZOOM_OFFSET",        (cmdFunc_t)&TimeTagProcessorModule::ttZoomCmd,               1, "<relative offset from range window start for zoom (when binsize < 1.5 meters) in nanoseconds>");
    registerCommand("CH_DISABLE",                (cmdFunc_t)&TimeTagProcessorModule::chDisableCmd,            2, "<ENABLE|DISABLE> <channel>");
    registerCommand("AUTO_SET_RULER_CLK",        (cmdFunc_t)&TimeTagProcessorModule::autoSetRulerClkCmd,      1, "<ENABLE|DISABLE>");
    registerCommand("SET_TEP_LOCATION",          (cmdFunc_t)&TimeTagProcessorModule::setTepLocationCmd,      -1, "<range in ns> [<width in ns>]");
    registerCommand("BLOCK_TEP",                 (cmdFunc_t)&TimeTagProcessorModule::blockTepCmd,             1, "<ENABLE|DISABLE>");
    registerCommand("BUILD_UP_MFC",              (cmdFunc_t)&TimeTagProcessorModule::buildUpMfcCmd,          -1, "<ENABLE [<major frame count>]|DISABLE>");
    registerCommand("ATTACH_MAJOR_FRAME_PROC",   (cmdFunc_t)&TimeTagProcessorModule::attachMFProcCmd,         1, "<major frame processor name>");
    registerCommand("ATTACH_TIME_PROC",          (cmdFunc_t)&TimeTagProcessorModule::attachTimeProcCmd,       1, "<time processor name>");
    registerCommand("START_RESULT_FILE",         (cmdFunc_t)&TimeTagProcessorModule::startResultFileCmd,      1, "<result filename>");
    registerCommand("STOP_RESULT_FILE",          (cmdFunc_t)&TimeTagProcessorModule::stopResultFileCmd,       0, "");
    registerCommand("WRITE_GRANULE_HIST",        (cmdFunc_t)&TimeTagProcessorModule::writeGranHistCmd,        2, "<strong histogram file> <weak histogram file>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
TimeTagProcessorModule::~TimeTagProcessorModule(void)
{
    delete histQ;
    delete txTimeQ;

    if(majorFrameProcName)  delete [] majorFrameProcName;
    if(timeProcName)        delete [] timeProcName;
    if(timeStatName)        delete [] timeStatName;

    // --- these statistics must be deleted via CommandableObject
    //  delete pktStat;
    //  delete chStat;
    //  delete txStat;
    //  delete sigStat;
}

/******************************************************************************
 * TIME TAG PROCESSOR MODULE: PUBLIC STATIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * str2clear
 *----------------------------------------------------------------------------*/
bool TimeTagProcessorModule::str2corr(const char* str, correction_t* c)
{
    if( (strcmp(str, "UNCORRECTED") == 0) || (strcmp(str, "uncorrected") == 0) )
    {
        *c = UNCORRECTED;
    }
    else if( (strcmp(str, "LOOPBACK") == 0) || (strcmp(str, "loopback") == 0) )
    {
        *c = LOOPBACK;
    }
    else
    {
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* TimeTagProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* histq_name   = StringLib::checkNullStr(argv[0]);
    const char* txtimeq_name = StringLib::checkNullStr(argv[1]);
    int         pcenum       = (int)strtol(argv[2], NULL, 0);

    if(histq_name == NULL)
    {
        mlog(CRITICAL, "Histogram queue cannot be null!\n");
        return NULL;
    }

    if(txtimeq_name == NULL)
    {
      mlog(CRITICAL, "Histogram queue cannot be null!\n");
      return NULL;
    }

    if(pcenum < 1 || pcenum > NUM_PCES)
    {
        mlog(CRITICAL, "Invalid PCE specified: %d, must be between 1 and %d\n", pcenum, NUM_PCES);
        return NULL;
    }

    return new TimeTagProcessorModule(cmd_proc, name, pcenum - 1, histq_name, txtimeq_name);
}

/******************************************************************************
 * TIME TAG PROCESSOR MODULE: PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processSegments  - Parser for Science Time Tag Telemetry Packet
 *
 *   Notes: ALT
 *----------------------------------------------------------------------------*/
bool TimeTagProcessorModule::processSegments(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    int             numsegs         = segments.length();
    int             intperiod       = numpkts;
    double          cvr             = 0.0;      // calibration value rising
    double          cvf             = 0.0;      // calibration value falling
    uint64_t        amet            = 0;
    long            mfc             = 0;
    long            numdlb          = 0;
    uint32_t        prevtag         = 0;        // only for within the shot
    uint32_t        prevtag_sticky  = 0;        // stays across transmits
    int             packet_bytes    = 0;
    int             txcnt_mf        = 0;        // number of transmit pulses in major frame
    int             tep_start_bin_s = 0;
    int             tep_stop_bin_s  = 0;
    int             tep_start_bin_w = 0;
    int             tep_stop_bin_w  = 0;
    shot_data_t*    shot_data       = NULL;

    /* Create String for Pretty Printing GPS Time */
    char gps_str[128];

    /* Create Shot Data List */
    List<shot_data_t*> shot_data_list;

    /* Create Packet Stats */
    pktStat_t pkt_stat;
    memset(&pkt_stat, 0, sizeof(pktStat_t));
    pkt_stat.segcnt = numsegs;
    pkt_stat.pktcnt = numpkts;

    /* Create Major Frame Data Buffer */
    mfdata_t*   mfdata_ptr = NULL;
    mfdata_t    mfdata;

    /* Create Downlink Band Stats */
    dlb_t dlb[MAX_NUM_DLBS];

    /* Create Local Channel Stats */
    chStat_t mf_ch_stat;
    memset(&mf_ch_stat, 0, sizeof(chStat_t));
    for(int c = 0; c < NUM_CHANNELS; c++)
    {
        mf_ch_stat.min_calr[c] = DBL_MAX;
        mf_ch_stat.min_calf[c] = DBL_MAX;
        mf_ch_stat.dead_time[c] = DBL_MAX;
    }

    /* Create Histograms */
    TimeTagHistogram* hist[NUM_SPOTS];
    hist[STRONG_SPOT] = NULL;
    hist[WEAK_SPOT]   = NULL;

    /*----------------------*/
    /* Pre-Process Settings */
    /*----------------------*/

    /* Loopback Correction (used for TEP as well) */
    int tx_loopback_index = 0;
    int max_tx_cnt = intperiod * MAX_NUM_SHOTS;
    int* tx_loopback_tags = new int[max_tx_cnt];
    for(int p = 0; p < numsegs; p++)
    {
        unsigned char*                  pktbuf  = segments[p]->getBuffer();
        CcsdsSpacePacket::seg_flags_t   seg     = segments[p]->getSEQFLG();
        int                             len     = segments[p]->getLEN();

        if(seg != CcsdsSpacePacket::SEG_START)
        {
            long i = 12;
            while(i < len && tx_loopback_index < max_tx_cnt)
            {
                long channel = (parseInt(pktbuf + i, 1) & 0xF8) >> 3;
                if(channel >= 24 && channel <= 27)
                {
                    tx_loopback_tags[tx_loopback_index++] = parseInt(pktbuf + i, 4);
                    i += 4;
                }
                else if((channel >= 1 && channel <= 20) || channel == 28) i += 3;
                else i += 1;
            }
        }
    }

    /* Use Calculated Ruler Clock Period */
    if(AutoSetTrueRulerClkPeriod)
    {
        if(cmdProc->getCurrentValue(timeProcName, TimeProcessorModule::true10Key, &TrueRulerClkPeriod, sizeof(TrueRulerClkPeriod)) <= 0)
        {
            mlog(CRITICAL, "Unable to retrieve current value of %s from %s, turning off auto-set\n", TimeProcessorModule::true10Key, timeProcName);
            AutoSetTrueRulerClkPeriod = false;
            cmdProc->setCurrentValue(getName(), autoSetTrueRulerClkPeriodKey, (void*)&AutoSetTrueRulerClkPeriod, sizeof(AutoSetTrueRulerClkPeriod));
        }
    }

    /*-----------------*/
    /* Process Segment */
    /*-----------------*/

    for(int p = 0; p < numsegs; p++)
    {
        /* Get Segment */
        unsigned char*                  pktbuf  = segments[p]->getBuffer();
        CcsdsSpacePacket::seg_flags_t   seg     = segments[p]->getSEQFLG();
        int                             len     = segments[p]->getLEN();

        /* Accumulate Packet Bytes */
        packet_bytes += len;

        /* Process Segments */
        if(seg == CcsdsSpacePacket::SEG_START)
        {
            /* Validate Number of Transmit Time Tags */
            if(txcnt_mf > MAX_NUM_SHOTS)
            {
                mlog(ERROR, "[%ld]: packet contained more than %d tx time tags: %d\n", mfc, MAX_NUM_SHOTS, shot_data_list.length());
                pkt_stat.pkt_errors++;
            }
            txcnt_mf = 0;

            /* Read Out Header Fields */
            mfc             = parseInt(pktbuf + 12, 4);
            amet            = parseInt(pktbuf + 16, 8);
            cvr             = TrueRulerClkPeriod / (parseInt(pktbuf + 24, 2) / 256.0); // ns
            cvf             = TrueRulerClkPeriod / (parseInt(pktbuf + 26, 2) / 256.0); // ns
            double rws_s    = parseInt(pktbuf + 28, 3) * TrueRulerClkPeriod; // ns
            double rww_s    = parseInt(pktbuf + 31, 2) * TrueRulerClkPeriod; // ns
            double rws_w    = parseInt(pktbuf + 33, 3) * TrueRulerClkPeriod; // ns
            double rww_w    = parseInt(pktbuf + 36, 2) * TrueRulerClkPeriod; // ns
            numdlb          = parseInt(pktbuf + 38, 1) + 1;

            /* Get Major Frame Data */
            char keyname[MajorFrameProcessorModule::MAX_KEY_NAME_SIZE];
            MajorFrameProcessorModule::buildKey(mfc, keyname);
            if(cmdProc->getCurrentValue(majorFrameProcName, keyname, &mfdata, sizeof(mfdata_t)) > 0)
            {
                if(mfdata.MajorFrameCount == mfc)
                {
                    mfdata_ptr = &mfdata;
                }
                else
                {
                    mlog(WARNING, "[%ld]: could not associate major frame data with science time tag data from %ld\n", mfc, mfdata.MajorFrameCount);
                    pkt_stat.warnings++;
                }
            }

            /* Handle GPS Time */
            double gps = 0.0;
            timeStat_t time_stat;
            if(cmdProc->getCurrentValue(timeStatName, "cv", &time_stat, sizeof(timeStat_t)) > 0)
            {
                if(time_stat.uso_freq_calc == true)
                {
                    /* Get GPS Current Values */
                    cmdProc->getCurrentValue(getName(), lastGpsKey, &LastGps, sizeof(LastGps));
                    cmdProc->getCurrentValue(getName(), lastGpsMfcKey, &LastGpsMfc, sizeof(LastGpsMfc));

                    /* Set GPS Time */
                    int64_t amet_delta = (int64_t)amet - (int64_t)time_stat.asc_1pps_amet;
                    gps = time_stat.asc_1pps_time + (((double)amet_delta * TrueRulerClkPeriod) / 1000000000.0);

                    /* Check GPS Time */
                    if(gps != 0.0 && LastGps != 0.0 && mfc > LastGpsMfc)
                    {
                        double expected_gps = LastGps + ((mfc - LastGpsMfc) * 0.020 * intperiod);
                        double gps_accuracy = fabs(expected_gps - gps);
                        if(gps_accuracy > (GpsAccuracyTolerance * intperiod))
                        {
                            mlog(WARNING, "[%ld]: AMET identification of major frame data exceeded accuracy tolerance of: %lf, actual: %lf\n", mfc, GpsAccuracyTolerance, gps_accuracy);
                            pkt_stat.warnings++;
                        }
                    }

                    /* Get GPS Current Values */
                    cmdProc->setCurrentValue(getName(), lastGpsKey, &gps, sizeof(gps));
                    cmdProc->setCurrentValue(getName(), lastGpsMfcKey, &mfc, sizeof(mfc));
                }
            }

            /* Get Pretty Print of GPS Time */
            long gps_ms = (long)(gps * 1000);
            TimeLib::gmt_time_t gmt_time = TimeLib::gps2gmttime(gps_ms);
            StringLib::format(gps_str, 128, "%d:%d:%d:%d:%d:%d", gmt_time.year, gmt_time.day, gmt_time.hour, gmt_time.minute, gmt_time.second, gmt_time.millisecond);

            /* Validate Number of Downlink Bands */
            if(numdlb > MAX_NUM_DLBS)
            {
                mlog(ERROR, "%s [%ld]: number of downlink bands exceed maximum %d, act %ld\n", gps_str, mfc, MAX_NUM_DLBS, numdlb);
                pkt_stat.hdr_errors++;
                numdlb = MAX_NUM_DLBS; // set so processing can continue
            }

            /* Read Out Downlink Bands*/
            for(int d = 0; d < numdlb; d++)
            {
                dlb[d].mask   = parseInt(pktbuf + 39 + (d*7) + 0, 3);
                dlb[d].start  = (uint16_t)parseInt(pktbuf + 39 + (d*7) + 3, 2);
                dlb[d].width  = (uint16_t)parseInt(pktbuf + 39 + (d*7) + 5, 2);
            }

            /* Create Time Tag Histograms */
            if(hist[STRONG_SPOT] == NULL) hist[STRONG_SPOT] = new TimeTagHistogram(AtlasHistogram::STT, intperiod, TimeTagBinSize, pce, mfc, mfdata_ptr, gps, rws_s, rww_s, dlb, numdlb, false);
            if(hist[WEAK_SPOT]   == NULL) hist[WEAK_SPOT]   = new TimeTagHistogram(AtlasHistogram::WTT, intperiod, TimeTagBinSize, pce, mfc, mfdata_ptr, gps, rws_w, rww_w, dlb, numdlb, false);

            /* Set TEP Blocking */
            if(BlockTep)
            {
                /* Locate Start and Stop for Strong */
                double rws_offset_s = fmod(rws_s, 100000.0);
                if(rws_offset_s < TepLocation)
                {
                    tep_start_bin_s = (int)MAX(floor((TepLocation - rws_offset_s - TepWidth) / (TimeTagBinSize * 20.0 / 3.0)), 0);
                    tep_stop_bin_s  = (int)ceil((TepLocation - rws_offset_s + TepWidth) / (TimeTagBinSize * 20.0 / 3.0));
                }
                else
                {
                    tep_start_bin_s = (int)MAX(floor(((100000.0 - rws_offset_s) + TepLocation - TepWidth) / (TimeTagBinSize * 20.0 / 3.0)), 0);
                    tep_stop_bin_s  = (int)ceil(((100000.0 - rws_offset_s) + TepLocation + TepWidth) / (TimeTagBinSize * 20.0 / 3.0));
                }

                /* Locate Start and Stop for Weak */
                double rws_offset_w = fmod(rws_w, 100000.0);
                if(rws_offset_w < TepLocation)
                {
                    tep_start_bin_w = (int)MAX(floor((TepLocation - rws_offset_w - TepWidth) / (TimeTagBinSize * 20.0 / 3.0)), 0);
                    tep_stop_bin_w  = (int)ceil((TepLocation - rws_offset_w + TepWidth) / (TimeTagBinSize * 20.0 / 3.0));
                }
                else
                {
                    tep_start_bin_w = (int)MAX(floor(((100000.0 - rws_offset_w) + TepLocation - TepWidth) / (TimeTagBinSize * 20.0 / 3.0)), 0);
                    tep_stop_bin_w  = (int)ceil(((100000.0 - rws_offset_w) + TepLocation + TepWidth) / (TimeTagBinSize * 20.0 / 3.0));
                }

                /* Set Ignore Region */
                if(tep_start_bin_s >= 0 && tep_stop_bin_s < AtlasHistogram::MAX_HIST_SIZE)
                {
                    hist[STRONG_SPOT]->setIgnore(tep_start_bin_s, tep_stop_bin_s);
                }
                else
                {
                    mlog(DEBUG, "Strong TEP region calculated outside of histogram: %d, %d - [%lf, %lf]\n", tep_start_bin_s, tep_stop_bin_s, rws_s, rww_s);
                    tep_start_bin_s = 0;
                    tep_stop_bin_s = 0;
                }

                if(tep_start_bin_w >= 0 && tep_stop_bin_w < AtlasHistogram::MAX_HIST_SIZE)
                {
                    hist[WEAK_SPOT]->setIgnore(tep_start_bin_w, tep_stop_bin_w);
                }
                else
                {
                    mlog(DEBUG, "Weak TEP region calculated outside of histogram: %d, %d - [%lf, %lf]\n", tep_start_bin_w, tep_stop_bin_w, rws_w, rww_w);
                    tep_start_bin_w = 0;
                    tep_stop_bin_w = 0;
                }
            }
        }
        else /* Process Continuation and End Segments */
        {
            /* Order Check */
            if(hist[STRONG_SPOT] == NULL || hist[WEAK_SPOT] == NULL)
            {
                mlog(ERROR, "start segment of time tag packet not received\n");
                pkt_stat.warnings++;
                return false;
            }

            /* Process Time Tags in Segment */
            long i = 12;
            while(i < len)
            {
                /* Read Channel */
                long id = parseInt(pktbuf + i, 1);
                long channel = (parseInt(pktbuf + i, 1) & 0xF8) >> 3;
                long channel_index = channel - 1;

                /* Check for Empty Packet */
                if((i == 1) && (id == 0xED))
                {
                    mlog(WARNING, "[%ld]: packet includes no time tags\n", mfc);
                    pkt_stat.warnings++;
                }

                /* Transmit Pulse */
                if(channel >= 24 && channel <= 27)
                {
                    if(shot_data != NULL)
                    {
                        shot_data_list.add(shot_data);

                        /* Check for Bursting Intermediary Histograms */
                        if(BuildUpMfc)
                        {
                            if(mfc == BuildUpMfcCount)
                            {
                                for(int s = 0; s < NUM_SPOTS; s++)
                                {
                                    hist[s]->setTransmitCount(shot_data_list.length()); // needed for calculating the signal attributes
                                    hist[s]->calcAttributes(SignalWidth, TrueRulerClkPeriod);
                                    unsigned char* buffer; // reference to serial buffer
                                    int size = hist[s]->serialize(&buffer, RecordObject::REFERENCE);
                                    histQ->postCopy(buffer, size);
                                }
                            }
                        }
                    }

                    shot_data = new shot_data_t;

                    shot_data->rx_index             = 0;
                    shot_data->truncated            = false;

                    shot_data->tx.tag               = parseInt(pktbuf + i, 4); i += 4;
                    shot_data->tx.width             = (shot_data->tx.tag  & 0x10000000) >> 28;
                    shot_data->tx.trailing_fine     = (shot_data->tx.tag  & 0x0FE00000) >> 21;
                    shot_data->tx.leading_coarse    = ((shot_data->tx.tag & 0x001FFF80) >> 7) + TransmitPulseCoarseCorrection;
                    shot_data->tx.leading_fine      = (shot_data->tx.tag  & 0x0000007F) >> 0;
                    shot_data->tx.time              = (shot_data->tx.leading_coarse * TrueRulerClkPeriod) - (shot_data->tx.leading_fine * cvr); // ns

                    shot_data->tx.return_count[STRONG_SPOT] = 0;
                    shot_data->tx.return_count[WEAK_SPOT]   = 0;

                    prevtag = 0; // reset previous tag for new shot
                    txcnt_mf++;  // count the transmit for check later on
                }
                /* Return Pulse */
                else if(channel >= 1 && channel <= 20)
                {
                    int spot;

                    /* Check If Transmit Pulse First */
                    if(shot_data == NULL)
                    {
                        mlog(ERROR, "%s [%ld]: fatal error... transmit time tag was not first in the packet\n", gps_str, mfc);
                        pkt_stat.fmt_errors++;

                        delete hist[STRONG_SPOT];
                        delete hist[WEAK_SPOT];

                        return false;
                    }

                    /* Check For Available Return Memory */
                    if(shot_data->rx_index == MAX_RX_PER_SHOT)
                    {
                        mlog(ERROR, "All statistics are invalid! Unable to allocate new rx pulse - reusing memory!\n");
                        shot_data->rx_index = 0;
                    }

                    /* Get Pointer to Available Return Memory */
                    rxPulse_t* rx = &(shot_data->rx[shot_data->rx_index]);

                    /* Read Time Tag */
                    rx->tag     = parseInt(pktbuf + i, 3); i += 3;
                    rx->toggle  = (rx->tag & 0x040000) >> 18;
                    rx->band    = (rx->tag & 0x020000) >> 17;
                    rx->coarse  = ((rx->tag & 0x01FF80) >> 7) + ReturnPulseCoarseCorrection;
                    rx->fine    = (rx->tag & 0x00007F) >> 0;

                    /* Sanity Check Values*/
                    if(rx->fine >= MAX_FINE_COUNT)
                    {
                        mlog(CRITICAL, "%s [%08X]: Fine count of %d exceeds maximum of %d\n", gps_str, (unsigned int)mfc, rx->fine, MAX_FINE_COUNT);
                        pkt_stat.fmt_errors++;
                        break;
                    }

                    /* Default Duplicate Status */
                    rx->duplicate = false;

                    /* Configure for Channel */
                    rx->channel = (uint8_t)channel;
                    if(channel >= 1 && channel <= 16)   spot = STRONG_SPOT;
                    else                                spot = WEAK_SPOT;

                    /* Increment Major Frame Channel Statistics */
                    if(channelDisable[channel_index] == false)
                    {
                        hist[spot]->incChCount(channel_index);
                    }

                    /* Check for Repeat Time Tags */
                    if(rx->tag == prevtag)
                    {
                        if(mfdata_ptr != NULL)
                        {
                            bool path_error = (spot == STRONG_SPOT) ? mfdata.TDC_StrongPath_Err : mfdata.TDC_WeakPath_Err;
                            if(!path_error)
                            {
                                mlog(ERROR, "%s [%ld]: time tag repeated %06X\n", gps_str, mfc, prevtag);
                                pkt_stat.tag_errors++;
                            }
                        }
                        else
                        {
                            mlog(WARNING, "%s [%ld]: time tag repeated %06X\n", gps_str, mfc, prevtag_sticky);
                            pkt_stat.warnings++;
                        }
                    }
                    prevtag = rx->tag;

                    if(rx->tag == prevtag_sticky)
                    {
                        mlog(WARNING, "%s [%ld]: time tag repeated %06X\n", gps_str, mfc, prevtag_sticky);
                        pkt_stat.warnings++;
                    }
                    prevtag_sticky = rx->tag;

                    /* Select Downlink Band */
                    int dlb_select = -1;

                    if( (numdlb > (0 + rx->band)) && ((dlb[(0 + rx->band)].mask & (1 << channel_index)) == 0) )	// NOTE: zero means enable in mask
                    {
                        dlb_select = 0 + rx->band;
                    }

                    if( (numdlb > (2 + rx->band)) && ((dlb[(2 + rx->band)].mask & (1 << channel_index)) == 0) )  // NOTE: zero means enable in mask
                    {
                        if(dlb_select != -1)
                        {
                            mlog(ERROR, "%s [%ld]: ambiguous downlink band settings\n", gps_str, mfc);
                            pkt_stat.dlb_errors++;
                        }
                        else
                        {
                            dlb_select = 2 + rx->band;
                        }
                    }

                    /* Process Time Tag */
                    if(dlb_select == -1)
                    {
                        mlog(ERROR, "%s [%ld]: no downlink band for timetag %06X\n", gps_str, mfc, rx->tag);
                        pkt_stat.dlb_errors++;
                    }
                    else if(rx->coarse > dlb[dlb_select].width)
                    {
                        mlog(ERROR, "%s [%ld]: timetag %06X is outside of downlink band %d [%d: %d]\n", gps_str, mfc, rx->tag, dlb_select, rx->coarse, dlb[dlb_select].width);
                        pkt_stat.tag_errors++;
                    }
                    else
                    {
                        /* Re-set Downlink Band */
                        rx->band = dlb_select;

                        /* Set Calibration Value */
                        if(rx->toggle == 0)
                        {
                            if((chStat->rec->avg_calf[channel_index] > MinFineTimeCal) &&
                               (chStat->rec->avg_calf[channel_index] < MaxFineTimeCal))
                            {
                                rx->calval = chStat->rec->avg_calf[channel_index];
                            }
                            else
                            {
                                rx->calval = cvf;
                            }
                        }
                        else
                        {
                            if((chStat->rec->avg_calr[channel_index] > MinFineTimeCal) &&
                               (chStat->rec->avg_calr[channel_index] < MaxFineTimeCal))
                            {
                                rx->calval = chStat->rec->avg_calr[channel_index];
                            }
                            else
                            {
                                rx->calval = cvr;
                            }
                        }

                        /* Calculate Range (all in ns) */
                        double coarse_time = (double)(dlb[rx->band].start + rx->coarse) * TrueRulerClkPeriod;
                        rx->range = (coarse_time - (rx->fine * rx->calval)) + (hist[spot]->getRangeWindowStart() * (10.0 / TrueRulerClkPeriod));
                        rx->range -= chStat->rec->bias[channel_index];
                        rx->range += (shot_data->tx.leading_fine * cvr);

                        /* Calculate Transmit Time Correction */
                        bool able_to_correct = true;
                        int tx_look_ahead = (int)lround(ceil(rx->range / (10000.0 * TrueRulerClkPeriod)));
                        int current_tx = shot_data_list.length() - 1;
                        int correct_tx = current_tx + tx_look_ahead;
                        double tx_correction = 0.0;
                        if(correct_tx < tx_loopback_index)
                        {
                            int correct_leading_coarse  = ((tx_loopback_tags[correct_tx] & 0x001FFF80) >> 7) + TransmitPulseCoarseCorrection;
                            int correct_leading_fine    = (tx_loopback_tags[correct_tx]  & 0x0000007F) >> 0;
                            double correct_time         = (correct_leading_coarse * TrueRulerClkPeriod) - (correct_leading_fine * cvr); // ns
                            tx_correction               = shot_data->tx.time - correct_time;
                        }
#if 0
                        else if(Correction == LOOPBACK)
                        {
                            // end of integration period... discard returns unable to associated to correct tx
                            // break from processing rest of packet... nothing left to do
                            // this also preserves the correct number of shots for scaling statistics
                            p = numsegs;
                            break;
                        }
#endif
                        else
                        {
                            // this is used later on, so that loopback returns
                            // that aren't corrected do not get binned in the histogram
                            able_to_correct = false;
                        }

                        /* Adjust Range for Transmit Time Correction */
                        bool valid_range = true;
                        if(Correction == LOOPBACK)
                        {
                            double range_from_tx = fmod(rx->range, 10000.0 * TrueRulerClkPeriod);
                            if(range_from_tx > (5000.0 * TrueRulerClkPeriod)) range_from_tx = range_from_tx - (10000.0 * TrueRulerClkPeriod);
                            double distance_from_loopback = fabs(range_from_tx - LoopbackLocation);
                            if(distance_from_loopback < LoopbackWidth)
                            {
                                if(able_to_correct)
                                {
                                    double candidate_range = rx->range + tx_correction;

                                    // test to see if the correction takes the pulse outside the loopback region
                                    // if it does, then don't correct, since it will artificially degrade the edges
                                    // of the region (since pulses outside the region are not being corrected into the region)
                                    double new_range_from_tx = fmod(candidate_range, 10000.0 * TrueRulerClkPeriod);
                                    if(new_range_from_tx > (5000.0 * TrueRulerClkPeriod)) new_range_from_tx = new_range_from_tx - (10000.0 * TrueRulerClkPeriod);
                                    double new_distance_from_loopback = fabs(new_range_from_tx - LoopbackLocation);
                                    if(new_distance_from_loopback < LoopbackWidth)
                                    {
                                        rx->range += tx_correction;
                                    }
                                }
                                else // uncorrected loopback return
                                {
                                    valid_range = false;
                                }
                            }
                        }

                        /* Calculate Histogram Bin */
                        int return_bin = 0;
                        if(FullColumnIntegration)
                        {
                            return_bin = (int)(rx->range * (0.15 / TimeTagBinSize)) % AtlasHistogram::MAX_HIST_SIZE; // 0.15 meters per nanosecond
                        }
                        else if(TimeTagBinSize >= DefaultTimeTagBinSize)
                        {
                            return_bin = (int)((rx->range - (hist[spot]->getRangeWindowStart() * (10.0 / TrueRulerClkPeriod))) * (0.15 / TimeTagBinSize)); // 0.15 meters per nanosecond
                        }
                        else if(TimeTagBinSize < DefaultTimeTagBinSize)
                        {
                            return_bin = (int)((rx->range - (hist[spot]->getRangeWindowStart() + TimeTagZoomOffset)) * (0.15 / TimeTagBinSize)); // 0.15 meters per nanosecond
                        }
                        else
                        {
                            mlog(CRITICAL, "Unable to calculate bin for pce %d with range %lf and binsize: %lf\n", pce, rx->range, TimeTagBinSize);
                        }

                        /* Check For Duplicate */
                        if(RemoveDuplicates == true)
                        {
                            for(int r = 0; r < shot_data->rx_list[rx->toggle][channel_index].length(); r++)
                            {
                                rxPulse_t* tag = shot_data->rx_list[rx->toggle][channel_index][r];

                                /* A duplicate time tag must meet the following criteria:
                                   1. Must be on the same channel
                                   2. Must have the same toggle
                                   3. Must be one coarse count away
                                   4. The difference between the fine counts must exceed the coarse count frequency (in order to achieve this precisely,
                                       extremely accurate cell calibrations are needed; as a substitute we assume the detector dead time and deduce that anything that falls within it is a duplicate) */
                                int coarse_delta = tag->coarse - rx->coarse;
                                int chain_span = coarse_delta * (tag->fine - rx->fine);
                                if( (abs(coarse_delta) == 1) && ((chain_span * rx->calval) >= (TrueRulerClkPeriod - DetectorDeadTime)) )
                                {
                                    rx->duplicate = true;

                                    double calval = TrueRulerClkPeriod / chain_span;
                                    if(rx->toggle == RISING_EDGE)
                                    {
                                        mf_ch_stat.avg_calr[channel_index] = integrateAverage(mf_ch_stat.num_dupr[channel_index], mf_ch_stat.avg_calr[channel_index], calval);
                                        if      (calval < mf_ch_stat.min_calr[channel_index]) mf_ch_stat.min_calr[channel_index] = calval;
                                        else if (calval > mf_ch_stat.max_calr[channel_index]) mf_ch_stat.max_calr[channel_index] = calval;
                                        mf_ch_stat.num_dupr[channel_index]++;
                                    }
                                    else // FALLING_EDGE
                                    {
                                        mf_ch_stat.avg_calf[channel_index] = integrateAverage(mf_ch_stat.num_dupf[channel_index], mf_ch_stat.avg_calf[channel_index], calval);
                                        if      (calval < mf_ch_stat.min_calf[channel_index]) mf_ch_stat.min_calf[channel_index] = calval;
                                        else if (calval > mf_ch_stat.max_calf[channel_index]) mf_ch_stat.max_calf[channel_index] = calval;
                                        mf_ch_stat.num_dupf[channel_index]++;
                                    }

                                    break;
                                }
                            }
                        }

                        /* Check For Dead-Time */
                        if(valid_range == true && rx->duplicate == false)
                        {
                            int opposite_edge = (rx->toggle + 1) % 2;
                            for(int r = 0; r < shot_data->rx_list[opposite_edge][channel_index].length(); r++)
                            {
                                rxPulse_t* tag = shot_data->rx_list[opposite_edge][channel_index][r];
                                double delta_range = fabs(tag->range - rx->range);
                                if(delta_range < mf_ch_stat.dead_time[channel_index])
                                {
                                    mf_ch_stat.dead_time[channel_index] = delta_range;
                                }
                            }
                        }

                        /* Bin & Count Return */
                        shot_data->tx.return_count[spot]++;
                        if( (RemoveDuplicates == false || rx->duplicate == false) &&
                            (channelDisable[channel_index] == false) )
                        {
                            mf_ch_stat.rx_cnt[channel_index]++;
                            mf_ch_stat.cell_cnts[channel_index][rx->fine]++;

                            if(valid_range == true)
                            {
                                hist[spot]->binTag(return_bin, rx);
                                shot_data->rx_list[rx->toggle][channel_index].add(rx);
                                shot_data->rx_index++;
                            }
                        }
                    }
                }
                /* Termination */
                else if(channel == 28)
                {
                    if(shot_data != NULL) shot_data->truncated = true;
                    int truncation_tag = parseInt(pktbuf + i, 3); i += 3;
                    mlog(WARNING, "%s [%ld]: packet truncation tag %06X detected\n", gps_str, mfc, truncation_tag);
                    pkt_stat.warnings++;
                }
                else if(id == 0xED) // channel == 29
                {
                    i += 1;
                    // Do Nothing - this value is used for padding and terminating the packets
                }
                else
                {
                    i += 1;
                    mlog(ERROR, "%s [%ld]: invalid channel detected. byte: %ld\n", gps_str, mfc, id);
                    pkt_stat.pkt_errors++;
                }
            }
        }

        /* Process Last Segment Checks */
        if(seg == CcsdsSpacePacket::SEG_STOP)
        {
            /* Cross Check Major Frame Data */
            for(int s = 0; s < NUM_SPOTS; s++)
            {
                if(mfdata_ptr != NULL && numpkts == 1) // only works if we are not integrating
                {
                    /* Check Range Window Start */
                    double dfc_rws;
                    if(s == STRONG_SPOT)    dfc_rws = (mfdata.StrongAltimetricRangeWindowStart + 13) * TrueRulerClkPeriod;
                    else                    dfc_rws = (mfdata.WeakAltimetricRangeWindowStart + 13) * TrueRulerClkPeriod;

                    if(dfc_rws != hist[s]->getRangeWindowStart())
                    {
                        mlog(ERROR, "%s [%ld]: %s science data range window did not match value reported by hardware, FSW: %.1lf, DFC: %.1lf\n", gps_str, mfc, s == STRONG_SPOT ? "strong" : "weak", hist[s]->getRangeWindowStart(), dfc_rws);
                        pkt_stat.pkt_errors++;
                    }

                    /* Check Range Window Width */
                    double dfc_rww = 0.0;
                    if(s == STRONG_SPOT)    dfc_rww = (mfdata.StrongAltimetricRangeWindowWidth + 1) * TrueRulerClkPeriod;
                    else                    dfc_rww = (mfdata.WeakAltimetricRangeWindowWidth + 1) * TrueRulerClkPeriod;

                    if(dfc_rww != hist[s]->getRangeWindowWidth())
                    {
                        mlog(ERROR, "%s [%ld]: %s science data range window width did not match value reported by hardware, FSW: %.1lf, DFC: %.1lf\n", gps_str, mfc, s == STRONG_SPOT ? "strong" : "weak", hist[s]->getRangeWindowWidth(), dfc_rww);
                        pkt_stat.pkt_errors++;
                    }
                }
            }
        }
    }

    /* Add Last Shot Data */
    shot_data_list.add(shot_data);  

    /* Validate Number of Transmit Time Tags */
    if(txcnt_mf > MAX_NUM_SHOTS)
    {
        mlog(ERROR, "%s [%ld]: packet contained more than %d tx time tags: %d\n", gps_str, mfc, MAX_NUM_SHOTS, shot_data_list.length());
        pkt_stat.pkt_errors++;
    }

    /*------------------------*/
    /* Process Transmit Stats */
    /*------------------------*/

    int         num_shots               = shot_data_list.length();

    double*     tx_deltas               = new double[num_shots];           
    double      tx_min_delta            = DBL_MAX;
    double      tx_max_delta            = 0.0;
    double      tx_sum_delta            = 0.0;

    uint32_t    tx_min_tags[NUM_SPOTS]  = { INT_MAX, INT_MAX };
    uint32_t    tx_max_tags[NUM_SPOTS]  = { 0, 0 };
    uint32_t    tx_sum_tags[NUM_SPOTS]  = { 0, 0 };

    for(int i = 0; i < num_shots; i++)
    {
        shot_data = shot_data_list[i];

        /* Caclulate Tx Tag Cnts */
        for(int s = 0; s < NUM_SPOTS; s++)
        {
            uint32_t  cnt = (uint32_t)shot_data->tx.return_count[s];
            bool    trunc = shot_data->truncated;

            if      (cnt < tx_min_tags[s] && !trunc)    tx_min_tags[s] = cnt;
            else if (cnt > tx_max_tags[s])              tx_max_tags[s] = cnt;

            tx_sum_tags[s] += cnt;
            pkt_stat.sum_tags += cnt;
        }

        /* Calculate Tx Deltas */
        if(i > 0)
        {
            shot_data_t* prev_shot_data = shot_data_list[i - 1];

            long coarse_delta = shot_data->tx.leading_coarse - prev_shot_data->tx.leading_coarse;
            double delta = shot_data->tx.time - prev_shot_data->tx.time;
            if(coarse_delta > 5000 || coarse_delta < -5000) delta = (10000.0 * TrueRulerClkPeriod) - delta;

            if      (delta < tx_min_delta)  tx_min_delta = delta;
            else if (delta > tx_max_delta)  tx_max_delta = delta;

            tx_deltas[i] = delta;
            tx_sum_delta += fabs(delta);
        }
        else
        {
            tx_deltas[i] = 0.0;
        }

//        long tx_absolute_time[1];
//        int stat = 0;
//        tx_absolute_time[0] = shot_data->tx.time + (i * 100000) + (amet * TrueRulerClkPeriod);
//
//        if ((stat = txTimeQ->postCopy(tx_absolute_time, sizeof(long), IO_CHECK)) < 0)
//        {
//	        mlog(ERROR, "Post in TimeTagProcessorModfule failed with: %d\n", stat);
//	    }

    }

    txStat->lock();
    {
        if(num_shots > 0)
        {
            /* Populate Tx Tag Cnts */
            for(int s = 0; s < NUM_SPOTS; s++)
            {
                if(txStat->rec->statcnt == 0)
                {
                    txStat->rec->min_tags[s] = tx_min_tags[s];
                    txStat->rec->max_tags[s] = tx_max_tags[s];
                }

                txStat->rec->avg_tags[s] = integrateAverage(txStat->rec->statcnt, txStat->rec->avg_tags[s], tx_sum_tags[s] / (double)num_shots);
                txStat->rec->min_tags[s] = MIN(txStat->rec->min_tags[s], tx_min_tags[s]);
                txStat->rec->max_tags[s] = MAX(txStat->rec->max_tags[s], tx_max_tags[s]);
                txStat->rec->std_tags[s] = sqrt(txStat->rec->avg_tags[s]);
            }

            /* Populate Tx Deltas */
            if(num_shots > 1)
            {
                txStat->rec->avg_delta = integrateAverage(txStat->rec->statcnt, txStat->rec->avg_delta, tx_sum_delta / (double)num_shots);
                txStat->rec->min_delta = MIN(txStat->rec->min_delta, tx_min_delta);
                txStat->rec->max_delta = MAX(txStat->rec->max_delta, tx_max_delta);
            }
        }

        txStat->rec->txcnt += num_shots;
        txStat->rec->statcnt++;
        txStat->rec->pce = pce;
    }
    txStat->unlock();

    /*--------------------------*/
    /* Update Signal Statistics */
    /*--------------------------*/

    /* Calculate Signal Attributes */
    for(int s = 0; s < NUM_SPOTS; s++)
    {
        hist[s]->setTransmitCount(num_shots); // needed for calculating the signal attributes
        bool sigfound = hist[s]->calcAttributes(SignalWidth, TrueRulerClkPeriod);
        if(!sigfound)
        {
            mlog(WARNING, "[%ld]: could not find signal in science time tag data for spot %s\n", mfc, s == 0 ? "STRONG_SPOT" : "WEAK_SPOT");
            pkt_stat.warnings++;
        }
    }

    /* Calculate TEP Strength */
    double teppe[NUM_SPOTS] = {0.0, 0.0};

    double tepbkg_s = (double)(tep_stop_bin_s - tep_start_bin_s) * hist[STRONG_SPOT]->getNoiseBin();
    double tepcnt_s = (double)hist[STRONG_SPOT]->getSumRange(tep_start_bin_s, tep_stop_bin_s);
    teppe[STRONG_SPOT] = (tepcnt_s - tepbkg_s) / (double)num_shots;

    double tepbkg_w = (double)(tep_stop_bin_w - tep_start_bin_w) * hist[WEAK_SPOT]->getNoiseBin();
    double tepcnt_w = (double)hist[WEAK_SPOT]->getSumRange(tep_start_bin_w, tep_stop_bin_w);
    teppe[WEAK_SPOT] = (tepcnt_w - tepbkg_w) / (double)num_shots;

    hist[STRONG_SPOT]->setTepEnergy(teppe[STRONG_SPOT]);
    hist[WEAK_SPOT]->setTepEnergy(teppe[WEAK_SPOT]);

    /* Set Signal Statistics */
    sigStat->lock();
    {
        for(int s = 0; s < NUM_SPOTS; s++)
        {
            sigStat->rec->rws[s]    = integrateAverage(sigStat->rec->statcnt, sigStat->rec->rws[s],    hist[s]->getRangeWindowStart());
            sigStat->rec->rww[s]    = integrateAverage(sigStat->rec->statcnt, sigStat->rec->rww[s],    hist[s]->getRangeWindowWidth());
            sigStat->rec->sigrng[s] = integrateAverage(sigStat->rec->statcnt, sigStat->rec->sigrng[s], hist[s]->getSignalRange());
            sigStat->rec->bkgnd[s]  = integrateAverage(sigStat->rec->statcnt, sigStat->rec->bkgnd[s],  hist[s]->getNoiseFloor());
            sigStat->rec->sigpes[s] = integrateAverage(sigStat->rec->statcnt, sigStat->rec->sigpes[s], hist[s]->getSignalEnergy());
            sigStat->rec->teppe[s]  = integrateAverage(sigStat->rec->statcnt, sigStat->rec->teppe[s],  teppe[s]);
        }
        sigStat->rec->statcnt++;
        sigStat->rec->pce = pce;
    }
    sigStat->unlock();

    /*---------------------------*/
    /* Update Channel Statistics */
    /*---------------------------*/

    chStat->lock();
    {
        /* Populate Biases */
        double biases[NUM_CHANNELS];
        bool bias_set[NUM_CHANNELS];

        memset(bias_set, 0, sizeof(bias_set));
        hist[STRONG_SPOT]->getChBiases(biases, bias_set, 0, 15);
        hist[WEAK_SPOT]->getChBiases(biases, bias_set, 16, 19);

        for(int ch = 0; ch < NUM_CHANNELS; ch++)
        {
            if(bias_set[ch])
            {
                chStat->rec->bias[ch] = integrateAverage(chStat->rec->statcnt, chStat->rec->bias[ch], biases[ch]);
            }
        }

        /* Update Global Channel Statics */
        for(int ch = 0; ch < NUM_CHANNELS; ch++)
        {
            /* Chain Delays */
            for(int g = 0; g < MAX_FINE_COUNT; g++)
            {
                chStat->rec->cell_cnts[ch][g] += mf_ch_stat.cell_cnts[ch][g];
            }

            /* TDC Cals */
            chStat->rec->tdc_calr[ch] = integrateAverage(chStat->rec->statcnt, chStat->rec->tdc_calr[ch], cvr);
            chStat->rec->tdc_calf[ch] = integrateAverage(chStat->rec->statcnt, chStat->rec->tdc_calf[ch], cvf);

            /* Rising Edge Cals */
            if(mf_ch_stat.num_dupr[ch] > 0)
            {
                chStat->rec->avg_calr[ch] = integrateWeightedAverage(chStat->rec->num_dupr[ch], chStat->rec->avg_calr[ch], mf_ch_stat.avg_calr[ch], mf_ch_stat.num_dupr[ch]);
                chStat->rec->max_calr[ch] = MAX(chStat->rec->max_calr[ch], mf_ch_stat.max_calr[ch]);

                if(chStat->rec->min_calr[ch] != 0.0)         chStat->rec->min_calr[ch] = MIN(mf_ch_stat.min_calr[ch], chStat->rec->min_calr[ch]);
                else if(mf_ch_stat.min_calr[ch] != DBL_MAX) chStat->rec->min_calr[ch] = mf_ch_stat.min_calr[ch];

                chStat->rec->num_dupr[ch] += mf_ch_stat.num_dupr[ch];
            }

            /* Falling Edge Cals */
            if(mf_ch_stat.num_dupf[ch] > 0)
            {
                chStat->rec->avg_calf[ch] = integrateWeightedAverage(chStat->rec->num_dupf[ch], chStat->rec->avg_calf[ch], mf_ch_stat.avg_calf[ch], mf_ch_stat.num_dupf[ch]);
                chStat->rec->max_calf[ch] = MAX(chStat->rec->max_calf[ch], mf_ch_stat.max_calf[ch]);

                if(chStat->rec->min_calf[ch] != 0.0)         chStat->rec->min_calf[ch] = MIN(mf_ch_stat.min_calf[ch], chStat->rec->min_calf[ch]);
                else if(mf_ch_stat.min_calf[ch] != DBL_MAX) chStat->rec->min_calf[ch] = mf_ch_stat.min_calf[ch];

                chStat->rec->num_dupf[ch] += mf_ch_stat.num_dupf[ch];
            }

            /* Dead Time */
            if(chStat->rec->dead_time[ch] != 0.0)            chStat->rec->dead_time[ch] = MIN(mf_ch_stat.dead_time[ch], chStat->rec->dead_time[ch]);
            else if(mf_ch_stat.dead_time[ch] != DBL_MAX)    chStat->rec->dead_time[ch] = mf_ch_stat.dead_time[ch];

            /* Stat Cnts */
            chStat->rec->rx_cnt[ch] += mf_ch_stat.rx_cnt[ch];
        }
        chStat->rec->statcnt++;
        chStat->rec->pce = pce;
    }
    chStat->unlock();


    /*----------------------*/
    /* Tx/Rx Slip Detection */
    /*----------------------*/

    if(mfdata_ptr)
    {
        /* Get Counts in Range Window */
        long total_counts = 0;
        for(int b = 0; b < MajorFrameProcessorModule::NUM_BKGND_CNTS; b++)
        {
            total_counts += mfdata_ptr->BackgroundCounts[b];
        }

        /* Check if No Data */
        if((hist[STRONG_SPOT]->getNumDownlinkBands()) > 0 && (total_counts == 0))
        {
            mlog(ERROR, "%s [%ld] - request for no data\n", gps_str, mfc);
        }
    }

    int slipped_rxs[NUM_SPOTS] = { 0, 0 };
    for(int tx = 0; tx < (num_shots - 1); tx++)
    {
        /* Get Shot Data */
        shot_data = shot_data_list[tx];

        /* Loop Through Rxs in Shot */
        int num_rxs = shot_data->tx.return_count[STRONG_SPOT] + shot_data->tx.return_count[WEAK_SPOT];
        for(int rx = 0; rx < num_rxs; rx++)
        {
            /* Determine Spot */
            int spot = STRONG_SPOT;
            if(shot_data->rx[rx].channel > 16)
            {
                spot = WEAK_SPOT;
            }   

            /* Get Signal Range and Width */
            double signal_range = hist[spot]->getSignalRange();
            double signal_energy = hist[spot]->getSignalEnergy();
            double signal_width = hist[spot]->getSignalWidth();

            /* Look For Slip Only On Sawtooth Drop and When There Is Signal*/
            if(fabs(tx_deltas[tx + 1]) > 20.0 && signal_energy > 0.5)
            {
                double range_delta = shot_data->rx[rx].range - signal_range;
                double slip_delta = range_delta - tx_deltas[tx + 1];

                if(fabs(slip_delta) < 1.0)
                {
                    slipped_rxs[spot]++;
                }
            }

            /* Build Granule Histogram - Only Use High Energy Narrow Returns */
//            if(signal_energy > 0.5 && signal_energy < 2.5 && signal_width < 20.0)
//            {
                double delta_range = signal_range - shot_data->rx[rx].range;
                delta_range = delta_range + 0.5 - (delta_range < 0);
                long bin = (long)delta_range;

                long hist_radius = GRANULE_HIST_SIZE / 2;
                if(bin >= -hist_radius && bin < hist_radius)
                {
                    granMut.lock();
                    {
                        granHist[spot][hist_radius + bin]++;
                    }
                    granMut.unlock();
                }
//            }
        }
    }

    /* Populate Slip Count */
    for(int s = 0; s < NUM_SPOTS; s++)
    {
        hist[s]->setSlipCnt(slipped_rxs[s]);
    }

    /*---------------------------*/
    /* Process Packet Statistics */
    /*---------------------------*/

    pktStat->lock();
    {
        pktStat->rec->sum_tags = pkt_stat.sum_tags;

        if(pktStat->rec->pktcnt == 0)
        {
            pktStat->rec->min_tags = pkt_stat.sum_tags;
            pktStat->rec->max_tags = pkt_stat.sum_tags;
        }
        else
        {
            if(pktStat->rec->min_tags > pkt_stat.sum_tags) pktStat->rec->min_tags = pkt_stat.sum_tags;
            if(pktStat->rec->max_tags < pkt_stat.sum_tags) pktStat->rec->max_tags = pkt_stat.sum_tags;
        }

        double avg_sum = (pktStat->rec->avg_tags * pktStat->rec->pktcnt) + pkt_stat.sum_tags;
        pktStat->rec->avg_tags = avg_sum / (pktStat->rec->pktcnt + 1);

        pktStat->rec->segcnt      += pkt_stat.segcnt;
        pktStat->rec->pktcnt      += pkt_stat.pktcnt;
        pktStat->rec->mfc_errors  += pkt_stat.mfc_errors;
        pktStat->rec->hdr_errors  += pkt_stat.hdr_errors;
        pktStat->rec->fmt_errors  += pkt_stat.fmt_errors;
        pktStat->rec->dlb_errors  += pkt_stat.dlb_errors;
        pktStat->rec->tag_errors  += pkt_stat.tag_errors;
        pktStat->rec->pkt_errors  += pkt_stat.pkt_errors;
        pktStat->rec->warnings    += pkt_stat.warnings;

        pktStat->rec->pce = pce;
    }
    pktStat->unlock();

    /*------------------------*/
    /* Process Histogram Core */
    /*------------------------*/

    for(int s = 0; s < NUM_SPOTS; s++)
    {
        /* Copy In Stats */
        hist[s]->setPktStats(&pkt_stat);
        hist[s]->setPktBytes(packet_bytes);
        hist[s]->setPktErrors(pkt_stat.mfc_errors +
                              pkt_stat.hdr_errors +
                              pkt_stat.fmt_errors +
                              pkt_stat.dlb_errors +
                              pkt_stat.tag_errors +
                              pkt_stat.pkt_errors);

        /* Post Histograms */
        unsigned char* buffer; // reference to serial buffer
        int size = hist[s]->serialize(&buffer, RecordObject::REFERENCE);
        histQ->postCopy(buffer, size);
    }

    /*---------------*/
    /* Write Summary */
    /*---------------*/

    if(resultFile)
    {
        fprintf(resultFile, "%ld, %d, %d, %u, %d, %d, %d, %d, %d, %d, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %ld, %04X, %d, %d, %04X, %d, %d, %04X, %d, %d, %04X, %d, %d\n",
                mfc, intperiod, num_shots, pkt_stat.sum_tags, hist[STRONG_SPOT]->getSum(), hist[WEAK_SPOT]->getSum(),
                tx_min_tags[STRONG_SPOT], tx_max_tags[STRONG_SPOT],
                tx_min_tags[WEAK_SPOT], tx_max_tags[WEAK_SPOT],
                hist[STRONG_SPOT]->getRangeWindowStart(),  hist[STRONG_SPOT]->getRangeWindowWidth(), hist[STRONG_SPOT]->getSignalRange(), hist[STRONG_SPOT]->getNoiseFloor(), hist[STRONG_SPOT]->getSignalEnergy(), teppe[STRONG_SPOT],
                hist[WEAK_SPOT]->getRangeWindowStart(),  hist[WEAK_SPOT]->getRangeWindowWidth(), hist[WEAK_SPOT]->getSignalRange(), hist[WEAK_SPOT]->getNoiseFloor(), hist[WEAK_SPOT]->getSignalEnergy(), teppe[WEAK_SPOT],
                mf_ch_stat.rx_cnt[0], mf_ch_stat.rx_cnt[1], mf_ch_stat.rx_cnt[2], mf_ch_stat.rx_cnt[3],
                mf_ch_stat.rx_cnt[4], mf_ch_stat.rx_cnt[5], mf_ch_stat.rx_cnt[6], mf_ch_stat.rx_cnt[7], mf_ch_stat.rx_cnt[8], mf_ch_stat.rx_cnt[9], mf_ch_stat.rx_cnt[10], mf_ch_stat.rx_cnt[11],
                mf_ch_stat.rx_cnt[12], mf_ch_stat.rx_cnt[13], mf_ch_stat.rx_cnt[14], mf_ch_stat.rx_cnt[15], mf_ch_stat.rx_cnt[16], mf_ch_stat.rx_cnt[17], mf_ch_stat.rx_cnt[18], mf_ch_stat.rx_cnt[19],
                numdlb,
                dlb[0].mask, dlb[0].start, dlb[0].width,
                dlb[1].mask, dlb[1].start, dlb[1].width,
                dlb[2].mask, dlb[2].start, dlb[2].width,
                dlb[3].mask, dlb[3].start, dlb[3].width);
    }


    /*----------------------*/
    /* Free Memory & Return */
    /*----------------------*/

    for(int s = 0; s < NUM_SPOTS; s++)
    {
        delete hist[s];
    }

    delete [] tx_loopback_tags;

    for(int i = 0; i < shot_data_list.length(); i++)
    {
        delete shot_data_list[i];
    }

    if( pkt_stat.mfc_errors +
        pkt_stat.hdr_errors +
        pkt_stat.fmt_errors +
        pkt_stat.dlb_errors +
        pkt_stat.tag_errors +
        pkt_stat.pkt_errors > 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

/*----------------------------------------------------------------------------
 * removeDuplicatesCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::removeDuplicatesCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool include;
    if(!StringLib::str2bool(argv[0], &include)) return -1;

    RemoveDuplicates = include;

    return 0;
}

/*----------------------------------------------------------------------------
 * setClkPeriodCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::setClkPeriodCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    TrueRulerClkPeriod = strtod(argv[0], NULL);

    return 0;
}

/*----------------------------------------------------------------------------
 * setSignalWidthCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::setSignalWidthCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    SignalWidth = strtod(argv[0], NULL);
    cmdProc->setCurrentValue(getName(), signalWidthKey, (void*)&SignalWidth, sizeof(SignalWidth));

    return 0;
}

/*----------------------------------------------------------------------------
 * setCorrectionCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::setCorrectionCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    TimeTagProcessorModule::correction_t c;
    if(!TimeTagProcessorModule::str2corr(argv[0], &c)) return -1;

    Correction = c;

    if(c == LOOPBACK && argc == 3)
    {
        LoopbackLocation = strtod(argv[1], NULL);
        LoopbackWidth = strtod(argv[2], NULL);
    }

    cmdProc->setCurrentValue(getName(), loopbackLocationKey, (void*)&LoopbackLocation, sizeof(LoopbackLocation));
    cmdProc->setCurrentValue(getName(), loopbackWidthKey,    (void*)&LoopbackWidth,    sizeof(LoopbackWidth));

    return 0;
}

/*----------------------------------------------------------------------------
 * fullColumnModeCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::fullColumnModeCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    FullColumnIntegration = enable;
    cmdProc->setCurrentValue(getName(), fullColumnIntegrationKey, (void*)&enable, sizeof(enable));

    return 0;
}

/*----------------------------------------------------------------------------
 * ttBinsizeCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::ttBinsizeCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    if(strcmp(argv[0], "REVERT") == 0 || strcmp(argv[0], "revert") == 0)
    {
        TimeTagBinSize = DefaultTimeTagBinSize;
    }
    else
    {
        TimeTagBinSize = (strtod(argv[0], NULL) * 3.0) / 20.0;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * ttZoomCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::ttZoomCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    double zoom_offset = strtod(argv[0], NULL);

    for(int i = 0; i < NUM_PCES; i++)
    {
        TimeTagZoomOffset = zoom_offset;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * chDisableCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::chDisableCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool disable;
    if(!StringLib::str2bool(argv[0], &disable)) return -1;

    int channel = (int)strtol(argv[1], NULL, 0) - 1;

    if(channel >= 0 && channel < NUM_CHANNELS)
    {
        channelDisable[channel] = disable;
    }
    else if(channel == -1)
    {
        for(int i = 0; i < NUM_CHANNELS; i++)
        {
            channelDisable[i] = disable;
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * autoSetRulerClkCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::autoSetRulerClkCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    AutoSetTrueRulerClkPeriod = enable;
    cmdProc->setCurrentValue(getName(), autoSetTrueRulerClkPeriodKey, (void*)&enable, sizeof(enable));

    return 0;
}

/*----------------------------------------------------------------------------
 * setTepLocationCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::setTepLocationCmd(int argc, char argv[][MAX_CMD_SIZE])
{
   TepLocation = strtod(argv[0], NULL);

    if(argc > 1)
    {
        TepWidth = strtod(argv[1], NULL);
    }

    cmdProc->setCurrentValue(getName(), tepLocationKey, (void*)&TepLocation, sizeof(TepLocation));
    cmdProc->setCurrentValue(getName(), tepWidthKey,    (void*)&TepWidth,    sizeof(TepWidth));

    return 0;
}

/*----------------------------------------------------------------------------
 * blockTepCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::blockTepCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    BlockTep = enable;
    cmdProc->setCurrentValue(getName(), blockTepKey, (void*)&enable, sizeof(enable));

    return 0;
}

/*----------------------------------------------------------------------------
 * buildUpMfcCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::buildUpMfcCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    BuildUpMfc = enable;
    if(enable)
    {
        BuildUpMfcCount = strtol(argv[1], NULL, 0);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * attachMFProcCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::attachMFProcCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    if(majorFrameProcName) delete [] majorFrameProcName;
    majorFrameProcName = StringLib::duplicate(StringLib::checkNullStr(argv[0]));

    return 0;
}

/*----------------------------------------------------------------------------
 * attachTimeProcCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::attachTimeProcCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    if(timeProcName) delete [] timeProcName;
    timeProcName = StringLib::duplicate(StringLib::checkNullStr(argv[0]));

    if(timeStatName) delete [] timeStatName;
    timeStatName = StringLib::concat(timeProcName, ".", TimeStat::rec_type);

    return 0;
}

/*----------------------------------------------------------------------------
 * startResultFileCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::startResultFileCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    FILE* fp = fopen(argv[0], "w");
    if(!fp)
    {
        mlog(CRITICAL, "Unable to open result file: %s\n", argv[0]);
        return -1;
    }
    else
    {
        resultFile = fp;
        fprintf(resultFile, "MajorFrame,IntegrationPeriod,TxCount,TimeTagCount,StrongReturnCount,WeakReturnCount,StrongMinTimeTagsPerTx,StrongMaxTimeTagsPerTx,WeakMinTimeTagsPerTx,WeakMaxTimeTagsPerTx,StrongRWS,StrongRWW,StrongRNG,StrongBkg,StrongPE,StrongTEPPE,WeakRWS,WeakRWW,WeakRNG,WeakBkg,WeakPE,WeakTEPPE,CH1,CH2,CH3,CH4,CH5,CH6,CH7,CH8,CH9,CH10,CH11,CH12,CH13,CH14,CH15,CH16,CH17,CH18,CH19,CH20,NumberOfDownlinkBands,DLB1MASK,DLB1START,DLB1WIDTH,DLB2MASK,DLB2START,DLB2WIDTH,DLB3MASK,DLB3START,DLB3WIDTH,DLB4MASK,DLB4START,DLB4WIDTH\n");
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * stopResultFileCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::stopResultFileCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    if(resultFile) fclose(resultFile);

    return 0;
}

/*----------------------------------------------------------------------------
 * writeGranHistCmd
 *----------------------------------------------------------------------------*/
int TimeTagProcessorModule::writeGranHistCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    for(int s = 0; s < NUM_SPOTS; s++)
    {
        FILE* fp = fopen(argv[s], "w");
        if(!fp)
        {
            mlog(CRITICAL, "Unable to open granule histogram file: %s\n", argv[s]);
            return -1;
        }
        else
        {
            fprintf(fp, "Index,Count\n");
            for(int i = 0; i < GRANULE_HIST_SIZE; i++)
            {
                fprintf(fp,"%d,%ld\n", i, granHist[s][i]);
            }
            fclose(fp);
        }
    }

    return 0;
}
