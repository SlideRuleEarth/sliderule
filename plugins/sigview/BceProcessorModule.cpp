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

#include "BceProcessorModule.h"
#include "TimeTagHistogram.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * TIME DIAGNOSTIC STATISTIC
 ******************************************************************************/

const char* BceStat::rec_type = "BceStat";

RecordObject::fieldDef_t BceStat::rec_def[] =
{
    {"POWER[0]",       DOUBLE, offsetof(bceStat_t, power[0]),  sizeof(((bceStat_t*)0)->power[0]), NATIVE_FLAGS},
    {"POWER[1]",       DOUBLE, offsetof(bceStat_t, power[1]),  sizeof(((bceStat_t*)0)->power[1]), NATIVE_FLAGS},
    {"POWER[2]",       DOUBLE, offsetof(bceStat_t, power[2]),  sizeof(((bceStat_t*)0)->power[2]), NATIVE_FLAGS},
    {"POWER[3]",       DOUBLE, offsetof(bceStat_t, power[3]),  sizeof(((bceStat_t*)0)->power[3]), NATIVE_FLAGS},
    {"POWER[4]",       DOUBLE, offsetof(bceStat_t, power[4]),  sizeof(((bceStat_t*)0)->power[4]), NATIVE_FLAGS},
    {"POWER[5]",       DOUBLE, offsetof(bceStat_t, power[5]),  sizeof(((bceStat_t*)0)->power[5]), NATIVE_FLAGS},
    {"ATTENUATION[0]", DOUBLE, offsetof(bceStat_t, atten[0]),  sizeof(((bceStat_t*)0)->atten[0]), NATIVE_FLAGS},
    {"ATTENUATION[1]", DOUBLE, offsetof(bceStat_t, atten[1]),  sizeof(((bceStat_t*)0)->atten[1]), NATIVE_FLAGS},
    {"ATTENUATION[2]", DOUBLE, offsetof(bceStat_t, atten[2]),  sizeof(((bceStat_t*)0)->atten[2]), NATIVE_FLAGS},
    {"ATTENUATION[3]", DOUBLE, offsetof(bceStat_t, atten[3]),  sizeof(((bceStat_t*)0)->atten[3]), NATIVE_FLAGS},
    {"ATTENUATION[4]", DOUBLE, offsetof(bceStat_t, atten[4]),  sizeof(((bceStat_t*)0)->atten[4]), NATIVE_FLAGS},
    {"ATTENUATION[5]", DOUBLE, offsetof(bceStat_t, atten[5]),  sizeof(((bceStat_t*)0)->atten[5]), NATIVE_FLAGS}
};

int BceStat::rec_elem = sizeof(BceStat::rec_def) / sizeof(RecordObject::fieldDef_t);

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BceStat::BceStat(CommandProcessor* cmd_proc, const char* stat_name): StatisticRecord<bceStat_t>(cmd_proc, stat_name, rec_type)
{
    cmdProc->registerObject(stat_name, this);
}

/******************************************************************************
 * BCE PROCESSOR PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
BceProcessorModule::BceProcessorModule(CommandProcessor* cmd_proc, const char* obj_name, const char* histq_name):
    CcsdsProcessorModule(cmd_proc, obj_name)
{
    assert(histq_name);

    /* Define BCE Statistic Record */
    BceStat::defineRecord(BceStat::rec_type, NULL, sizeof(bceStat_t), BceStat::rec_def, BceStat::rec_elem, 32);

    /* Create BCE Statistics */
    char stat_name[MAX_STAT_NAME_SIZE];
    bceStat = new BceStat(cmd_proc, StringLib::format(stat_name, MAX_STAT_NAME_SIZE, "%s.%s", obj_name, BceStat::rec_type));

    /* Create MsgQ */
    histQ = new Publisher(histq_name);

    /* Initialize APIDs */
    for(int i = 0; i < NUM_GRLS; i++)
    {
        histApids[i][BceHistogram::WAV] = INVALID_APID;
        histApids[i][BceHistogram::TOF] = INVALID_APID;
    }

    histApids[0][BceHistogram::WAV] = 0x60F;
    histApids[1][BceHistogram::WAV] = 0x610;
    histApids[2][BceHistogram::WAV] = 0x611;
    histApids[3][BceHistogram::WAV] = 0x612;
    histApids[4][BceHistogram::WAV] = 0x613;
    histApids[5][BceHistogram::WAV] = 0x614;
    histApids[0][BceHistogram::TOF] = 0x619;
    histApids[1][BceHistogram::TOF] = 0x61A;
    histApids[2][BceHistogram::TOF] = 0x61B;
    histApids[3][BceHistogram::TOF] = 0x61C;
    histApids[4][BceHistogram::TOF] = 0x61D;
    histApids[5][BceHistogram::TOF] = 0x61E;
    histApids[6][BceHistogram::TOF] = 0x61F;

    attenApid = 0x605;
    powerApid = 0x607;

    /* Initialize GRL Histograms */
    BceHistogram::defineHistogram();

    /* Register Commands */
    registerCommand("ATTACH_HIST_APID",  (cmdFunc_t)&BceProcessorModule::attachHistApidCmd,  3,  "<GRL> <BCE type> <apid>");
    registerCommand("ATTACH_ATTEN_APID", (cmdFunc_t)&BceProcessorModule::attachAttenApidCmd, 1,  "<apid>");
    registerCommand("ATTACH_POWER_APID", (cmdFunc_t)&BceProcessorModule::attachPowerApidCmd, 1,  "<apid>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
BceProcessorModule::~BceProcessorModule(void)
{
    // --- bceStat is a CommandableObject via that class --
    //    delete bceStat;
    delete histQ;
}

/******************************************************************************
 * BCE PROCESSOR PUBLIC STATIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* BceProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    const char* histq_name = StringLib::checkNullStr(argv[0]);

    if(histq_name == NULL)
    {
        mlog(CRITICAL, "Must supply histogram queue when creating BCE processor module\n");
        return NULL;
    }

    return new BceProcessorModule(cmd_proc, name, histq_name);
}

/******************************************************************************
 * BCE PROCESSOR PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processSegments  - Parser for BCE Packets
 *----------------------------------------------------------------------------*/
bool BceProcessorModule::processSegments(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    (void)numpkts;

    bool status = true;

    int numsegs = segments.length();
    for(int p = 0; p < numsegs; p++)
    {
        unsigned char* pktbuf = segments[p]->getBuffer();
        uint16_t apid = segments[p]->getAPID();

        if      (apid == attenApid) status = status && parseAttenuation(pktbuf);
        else if (apid == powerApid) status = status && parsePower(pktbuf);
        else                        status = status && parseWaveforms(pktbuf);
    }

    return status;
}

/*----------------------------------------------------------------------------
 * parseWaveforms  -
 *----------------------------------------------------------------------------*/
bool BceProcessorModule::parseWaveforms(unsigned char* pktbuf)
{
    int                     grl     = 0;
    int                     spot    = -1;
    BceHistogram::subtype_t subtype = BceHistogram::INVALID;
    uint16_t                  apid    = CCSDS_GET_APID(pktbuf);

    if(apid == histApids[0][BceHistogram::WAV])
    {
        subtype = BceHistogram::WAV;
        grl = 1;
        spot = STRONG_SPOT;
    }
    else if(apid == histApids[1][BceHistogram::WAV])
    {
        subtype = BceHistogram::WAV;
        grl = 2;
        spot = WEAK_SPOT;
    }
    else if(apid == histApids[2][BceHistogram::WAV])
    {
        subtype = BceHistogram::WAV;
        grl = 3;
        spot = STRONG_SPOT;
    }
    else if(apid == histApids[3][BceHistogram::WAV])
    {
        subtype = BceHistogram::WAV;
        grl = 4;
        spot = WEAK_SPOT;
    }
    else if(apid == histApids[4][BceHistogram::WAV])
    {
        subtype = BceHistogram::WAV;
        grl = 5;
        spot = STRONG_SPOT;
    }
    else if(apid == histApids[5][BceHistogram::WAV])
    {
        subtype = BceHistogram::WAV;
        grl = 6;
        spot = WEAK_SPOT;
    }
    else if(apid == histApids[0][BceHistogram::TOF])
    {
        subtype = BceHistogram::TOF;
        grl = 1;
        spot = STRONG_SPOT;
    }
    else if(apid == histApids[1][BceHistogram::TOF])
    {
        subtype = BceHistogram::TOF;
        grl = 2;
        spot = WEAK_SPOT;
    }
    else if (apid == histApids[2][BceHistogram::TOF])
    {
        subtype = BceHistogram::TOF;
        grl = 3;
        spot = STRONG_SPOT;
    }
    else if (apid == histApids[3][BceHistogram::TOF])
    {
        subtype = BceHistogram::TOF;
        grl = 4;
        spot = WEAK_SPOT;
    }
    else if (apid == histApids[4][BceHistogram::TOF])
    {
        subtype = BceHistogram::TOF;
        grl = 5;
        spot = STRONG_SPOT;
    }
    else if (apid == histApids[5][BceHistogram::TOF])
    {
        subtype = BceHistogram::TOF;
        grl = 6;
        spot = WEAK_SPOT;
    }
    else if (apid == histApids[6][BceHistogram::TOF])
    {
        subtype = BceHistogram::TOF;
        grl = 7;
        spot = STRONG_SPOT;
    }
    else
    {
        return false;
    }

    /* Read Out Header Fields */
    uint16_t  bce_test_id         = (uint16_t)parseInt(pktbuf + 18, 2); (void)bce_test_id;
    int     osc_id              = parseInt(pktbuf + 20, 1);
    int     osc_ch              = parseInt(pktbuf + 21, 1);
    uint32_t  osc_gps_sec         = parseInt(pktbuf + 22, 4);
    double  osc_gps_subsec      = parseFlt(pktbuf + 26, 8);
    double  x_inc               = parseFlt(pktbuf + 34, 4); (void)x_inc;
    double  x_zero              = parseFlt(pktbuf + 38, 4); (void)x_zero;
    double  y_scale             = parseFlt(pktbuf + 42, 4); (void)y_scale;
    double  y_offset            = parseFlt(pktbuf + 46, 4); (void)y_offset;
    double  y_zero              = parseFlt(pktbuf + 50, 4); (void)y_zero;
    uint16_t  waveform_samples    = (uint16_t)parseInt(pktbuf + 59, 2);

    /* Calculated Values */
    double  gps             = (double)osc_gps_sec + osc_gps_subsec;
    double  samples_per_ns  = 24.0; //0.000000001 / x_inc;
    double  downsample      = (BceHistogram::BINSIZE * 20.0 / 3.0) * samples_per_ns;
    int     num_samples     = (int)((int)(waveform_samples / downsample) * downsample);

    /* Create Histogram */
    BceHistogram* hist = new BceHistogram(AtlasHistogram::GRL, 1, BceHistogram::BINSIZE * downsample, gps, grl, spot, osc_id, osc_ch, subtype);

    /* Start Populating Bins */
    for(int i = 0; i < num_samples; i++)
    {
        int bin = (int)(i / downsample);
        int pktindex = 61 + i;

        if(pktindex >= CCSDS_GET_LEN(pktbuf))
        {
            mlog(CRITICAL, "Invalid index %d:%d into packet of length %d\n", pktindex, waveform_samples, CCSDS_GET_LEN(pktbuf));
            break;
        }

        signed char val = (signed char)parseInt(pktbuf + pktindex, 1);
        hist->addBin(bin, val);
    }

    /* Process Packet */
    double y_conv = 500.0 * y_scale;
    hist->scale(y_conv);
    long minval = hist->getMin(0, hist->getSize());
    hist->addScalar(-minval);
    hist->calcAttributes(0.0, 10.0);

    /* Post Histogram */
    unsigned char* buffer; // reference to serial buffer
    int size = hist->serialize(&buffer, RecordObject::REFERENCE);
    histQ->postCopy(buffer, size);
    delete hist;

    return true;
}

/*----------------------------------------------------------------------------
 * parseAttenuation  - Parse BCE Fiber Optic Attenuator Settings
 *----------------------------------------------------------------------------*/
bool BceProcessorModule::parseAttenuation(unsigned char* pktbuf)
{
    bceStat->lock();
    {
        bceStat->rec->atten[0] = integrateAverage(bceStat->rec->attencnt, bceStat->rec->atten[0], parseFlt(pktbuf + 20, 4));
        bceStat->rec->atten[1] = integrateAverage(bceStat->rec->attencnt, bceStat->rec->atten[1], parseFlt(pktbuf + 24, 4));
        bceStat->rec->atten[2] = integrateAverage(bceStat->rec->attencnt, bceStat->rec->atten[2], parseFlt(pktbuf + 28, 4));
        bceStat->rec->atten[3] = integrateAverage(bceStat->rec->attencnt, bceStat->rec->atten[3], parseFlt(pktbuf + 32, 4));
        bceStat->rec->atten[4] = integrateAverage(bceStat->rec->attencnt, bceStat->rec->atten[4], parseFlt(pktbuf + 36, 4));
        bceStat->rec->atten[5] = integrateAverage(bceStat->rec->attencnt, bceStat->rec->atten[5], parseFlt(pktbuf + 40, 4));

        bceStat->rec->attencnt++;
    }
    bceStat->unlock();

    return true;
}

/*----------------------------------------------------------------------------
 * parse_bcepow_pkt  - Parse BCE Raw Signal Measurements and Calibration Values
 *----------------------------------------------------------------------------*/
bool BceProcessorModule::parsePower(unsigned char* pktbuf)
{
    bceStat->lock();
    {
        bceStat->rec->power[0] = integrateAverage(bceStat->rec->powercnt, bceStat->rec->power[0], parseFlt(pktbuf + 20, 4));
        bceStat->rec->power[1] = integrateAverage(bceStat->rec->powercnt, bceStat->rec->power[1], parseFlt(pktbuf + 24, 4));
        bceStat->rec->power[2] = integrateAverage(bceStat->rec->powercnt, bceStat->rec->power[2], parseFlt(pktbuf + 28, 4));
        bceStat->rec->power[3] = integrateAverage(bceStat->rec->powercnt, bceStat->rec->power[3], parseFlt(pktbuf + 32, 4));
        bceStat->rec->power[4] = integrateAverage(bceStat->rec->powercnt, bceStat->rec->power[4], parseFlt(pktbuf + 36, 4));
        bceStat->rec->power[5] = integrateAverage(bceStat->rec->powercnt, bceStat->rec->power[5], parseFlt(pktbuf + 40, 4));

        bceStat->rec->powercnt++;
    }
    bceStat->unlock();

    return true;
}

/*----------------------------------------------------------------------------
 * attachHistApidCmd
 *----------------------------------------------------------------------------*/
int BceProcessorModule::attachHistApidCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    int g = (int)strtol(argv[0], NULL, 0);  // GRL
    int t = (int)strtol(argv[1], NULL, 0);  // BCE TYPE
    int a = (int)strtol(argv[2], NULL, 0);  // APID

    if(g < 0 || g >= NUM_GRLS)
    {
        mlog(CRITICAL, "Invalid GRL specified: %d\n", g);
        return -1;
    }

    if(t < 0 || t > BceHistogram::NUM_SUB_TYPES)
    {
        mlog(CRITICAL, "Invalid BCE type specified: %d\n", t);
        return -1;
    }

    histApids[g][t] = a;

    return 0;
}

/*----------------------------------------------------------------------------
 * attachAttenApidCmd
 *----------------------------------------------------------------------------*/
int BceProcessorModule::attachAttenApidCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    attenApid = (uint16_t)strtol(argv[0], NULL, 0);

    return 0;
}

/*----------------------------------------------------------------------------
 * attachPowerApidCmd
 *----------------------------------------------------------------------------*/
int BceProcessorModule::attachPowerApidCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    powerApid = (uint16_t)strtol(argv[0], NULL, 0);

    return 0;
}
