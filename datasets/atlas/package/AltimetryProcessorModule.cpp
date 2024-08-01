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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "AltimetryProcessorModule.h"
#include "TimeProcessorModule.h"
#include "MajorFrameProcessorModule.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double AltimetryProcessorModule::ALT_BINSIZE             = 3.0;  // meters
const double AltimetryProcessorModule::ATM_BINSIZE             = 30.0; // meters
const double AltimetryProcessorModule::DEFAULT_10NS_PERIOD     = 10.0;
const char* AltimetryProcessorModule::fullColumnIntegrationKey = "fullColumnIntegration";
const char* AltimetryProcessorModule::alignHistKey             = "alignHist";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
AltimetryProcessorModule::AltimetryProcessorModule(CommandProcessor* cmd_proc, const char* obj_name, int _pce, AltimetryHistogram::type_t _type, const char* histq_name):
    CcsdsProcessorModule(cmd_proc, obj_name),
    pce(_pce),
    type(_type)
{
    assert(histq_name);

    /* Create MsgQ */
    histQ = new Publisher(histq_name);

    /* Initialize Parameters */
    alignHistograms        = false;
    fullColumnIntegration  = false;
    trueRulerClkPeriod     = DEFAULT_10NS_PERIOD;
    majorFrameProcName     = NULL;

    /* Set Current Values */
    cmdProc->setCurrentValue(getName(), fullColumnIntegrationKey,       (void*)&fullColumnIntegration,      sizeof(fullColumnIntegration));
    cmdProc->setCurrentValue(getName(), alignHistKey,                   (void*)&alignHistograms,            sizeof(alignHistograms));

    /* Initialize Altimetry Histograms (Establishes Record Definitions) */
    AltimetryHistogram::defineHistogram();

    /* Register Commands */
    registerCommand("ALIGN_HISTS",               (cmdFunc_t)&AltimetryProcessorModule::alignHistsCmd,      1, "<ENABLE|DISABLE>");
    registerCommand("FULL_COLUMN_MODE",          (cmdFunc_t)&AltimetryProcessorModule::fullColumnModeCmd,  1, "<ENABLE|DISABLE>");
    registerCommand("SET_CLOCK_PERIOD",          (cmdFunc_t)&AltimetryProcessorModule::setClkPeriodCmd,    1, "<time processor name> | <period>");
    registerCommand("ATTACH_MAJOR_FRAME_PROC",   (cmdFunc_t)&AltimetryProcessorModule::attachMFProcCmd,    1, "<major frame processor name>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
AltimetryProcessorModule::~AltimetryProcessorModule(void)
{
    if(majorFrameProcName) delete [] majorFrameProcName;
    delete histQ;
}

/******************************************************************************
* PUBLIC STATIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* AltimetryProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    AtlasHistogram::type_t  type        = AtlasHistogram::str2type(argv[0]);
    const char*             histq_name  = StringLib::checkNullStr(argv[1]);
    int                     pce         = (int)strtol(argv[2], NULL, 0) - 1;

    if(pce < 0 || pce >= NUM_PCES)
    {
        mlog(CRITICAL, "Invalid pce specified: %d", pce);
        return NULL;
    }

    if(type == AtlasHistogram::NAS)
    {
        mlog(CRITICAL, "Invalid histogram type specified!");
        return NULL;
    }

    if(histq_name == NULL)
    {
        mlog(CRITICAL, "Must supply histogram queue when creating altimetry processor module");
        return NULL;
    }

    return new AltimetryProcessorModule(cmd_proc, name, pce, type, histq_name);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processSegments  - Parser for BCE Packets
 *----------------------------------------------------------------------------*/
bool AltimetryProcessorModule::processSegments(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    switch(type)
    {
        case AtlasHistogram::SAL:   // pass thru
        case AtlasHistogram::WAL:   return parseAltHist(segments, numpkts);
        case AtlasHistogram::SAM:   // pass thru
        case AtlasHistogram::WAM:   return parseAtmHist(segments, numpkts);
        default:                    return false;
    }
}

/*----------------------------------------------------------------------------
 * parse_AltHist  - Parser for Altimetric Histogram Packets
 *
 *   Notes: SAL, WAL
 *----------------------------------------------------------------------------*/
bool AltimetryProcessorModule::parseAltHist(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    /* Initialize Parameters */
    AltimetryHistogram* hist        = NULL;
    int                 bin         = 0;
    int                 num_bins    = 0;
    int                 pkt_errors  = 0;
    int                 pkt_bytes   = 0;
    int                 numsegs     = segments.length();

    /* Create Major Frame Data Buffer */
    mfdata_t*   mfdata_ptr = NULL;
    mfdata_t    mfdata;

    /* Check Number of Segments*/
    if(numsegs <= 0 || numsegs % NUM_ALT_SEGS_PER_PKT != 0)
    {
        mlog(ERROR, "Altimetric histogram packet with wrong number of segments: %d", numsegs);
        return false;
    }

    /* Process Segments */
    for(int p = 0; p < numsegs; p++)
    {
        /* Get Segment */
        unsigned char*                  pktbuf  = segments[p]->getBuffer();
        CcsdsSpacePacket::seg_flags_t   seg     = segments[p]->getSEQFLG();

        /* Set Pkt Bytes */
        pkt_bytes += segments[p]->getLEN();

        /* Process First Segment */
        if(seg == CcsdsSpacePacket::SEG_START)
        {
            /* Read Out Header Fields */
            long    mfc  = parseInt(pktbuf + 12, 4);
            double  rws  = parseInt(pktbuf + 16, 4) * trueRulerClkPeriod;  // ns
            double  rww  = parseInt(pktbuf + 20, 2) * trueRulerClkPeriod;  // ns

            /* Get Major Frame Data */
            char keyname[MajorFrameProcessorModule::MAX_KEY_NAME_SIZE];
            MajorFrameProcessorModule::buildKey(mfc, keyname);
            if(cmdProc->getCurrentValue(majorFrameProcName, keyname, &mfdata, sizeof(mfdata_t)) > 0)
            {
                if(mfdata.MajorFrameCount == mfc)   mfdata_ptr = &mfdata;
//                else                                mlog(WARNING, "[%04X]: could not associate major frame data with science time tag data", mfc);
            }

            /* Apply Bias Correction */
            if(fullColumnIntegration)           bin = (int)(rws * 3.0 / 20.0 / ALT_BINSIZE) % AtlasHistogram::MAX_HIST_SIZE; // 3 meters per 20 nanoseconds
            else if(alignHistograms == true)    bin = (int)((AtlasHistogram::histogramBias[type] * trueRulerClkPeriod) * 3.0 / 20.0 / ALT_BINSIZE);
            else                                bin = 0;

            /* Calculate Number of Bins */
            num_bins  = (parseInt(pktbuf + 20, 2) + 1) / 2;    // range window width divided by 2 and rounded up
            int max_bins = NUM_ALT_BINS_PER_PKT * NUM_ALT_SEGS_PER_PKT;
            if(num_bins > max_bins)
            {
                mlog(ERROR, "to many bins in altimetric range window %d, max is %d", num_bins, max_bins);
                pkt_errors++;
                num_bins = max_bins;
            }

            /* Create New Histogram */
            if(hist == NULL)
            {
                hist = new AltimetryHistogram(type, numpkts, ALT_BINSIZE, pce, mfc, mfdata_ptr, 0.0, rws, rww);
            }

            /* Start Populating Bins */
            for(int i = 0; i < MIN(num_bins, NUM_ALT_BINS_PER_PKT); i++)
            {
                hist->addBin(bin++, parseInt(pktbuf + 22 + (i*2), 2));
            }
        }
        else /* Process Continuation and End Segments */
        {
            /* Order Check */
            if(hist == NULL)
            {
                mlog(ERROR, "start segment of altimetric packet not received");
                return false;
            }

            /* Continue Populating Bins */
            int i = 0;
            while((bin < num_bins) && (i < MIN(num_bins, NUM_ALT_BINS_PER_PKT)))
            {
                hist->addBin(bin++, parseInt(pktbuf + 12 + ((i++)*2), 2));
            }
        }
    }

    /* Process Entire Packet */
    bool sigfound = hist->calcAttributes(0.0, trueRulerClkPeriod);
    if(!sigfound)
    {
        mlog(WARNING, "[%08lX]: could not find signal in altimetric histogram data", hist->getMajorFrameCounter());
    }

    /* Use Major Frame Data */
    if(mfdata_ptr != NULL)
    {
        /* Set Transmit Count */
        hist->setTransmitCount(mfdata.TxPulsesInMajorFrame);

        if(numpkts == 1) // only works if not integrating
        {
            /* Check Range Window Start */
            double dfc_rws = 0.0;
            if      (type == AtlasHistogram::SAL)   dfc_rws = mfdata.StrongAltimetricRangeWindowStart * trueRulerClkPeriod;
            else if (type == AtlasHistogram::WAL)   dfc_rws = mfdata.WeakAltimetricRangeWindowStart * trueRulerClkPeriod;

            if(dfc_rws != hist->getRangeWindowStart())
            {
                mlog(ERROR, "[%08lX]: %s alimteric range window start did not match value reported by hardware, FSW: %.1lf, DFC: %.1lf", hist->getMajorFrameCounter(), AtlasHistogram::type2str(type), hist->getRangeWindowStart(), dfc_rws);
                pkt_errors++;
            }

            /* Check Range Window Width */
            double dfc_rww = 0.0;
            if      (type == AtlasHistogram::SAL)   dfc_rww = mfdata.StrongAltimetricRangeWindowWidth * trueRulerClkPeriod;
            else if (type == AtlasHistogram::WAL)   dfc_rww = mfdata.WeakAltimetricRangeWindowWidth * trueRulerClkPeriod;

            if(dfc_rww != hist->getRangeWindowWidth())
            {
                mlog(ERROR, "[%08lX]: %s alimteric range window width did not match value reported by hardware, FSW: %.1lf, DFC: %.1lf", hist->getMajorFrameCounter(), AtlasHistogram::type2str(type), hist->getRangeWindowWidth(), dfc_rww);
                pkt_errors++;
            }
        }
    }

    /* Copy In Stats */
    hist->setPktErrors(pkt_errors);
    hist->setPktBytes(pkt_bytes);

    /* Post Histogram */
    unsigned char* buffer; // reference to serial buffer
    int size = hist->serialize(&buffer, RecordObject::REFERENCE);
    histQ->postCopy(buffer, size);
    delete hist;

    return true;
}

/*----------------------------------------------------------------------------
 * parse_atmhist_pkt  - Parser for Atmospheric Histogram Packets
 *
 *   Notes: SAM, WAM, ATM
 *----------------------------------------------------------------------------*/
bool AltimetryProcessorModule::parseAtmHist(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    AltimetryHistogram* hist        = NULL;
    int                 pkt_errors  = 0;
    int                 pkt_bytes   = 0;
    int                 numsegs     = segments.length();

    /* Check Parameters */
    if(numsegs <= 0)
    {
        mlog(CRITICAL, "No segments found");
        return false;
    }

    /* Create Major Frame Data Buffer */
    mfdata_t*   mfdata_ptr = NULL;
    mfdata_t    mfdata;

    /* Process Segments */
    for(int p = 0; p < numsegs; p++)
    {
        /* Get Packet */
        unsigned char*  pktbuf  = segments[p]->getBuffer();
        int             bin     = 0;

        /* Add Up Bytes */
        pkt_bytes += CCSDS_GET_LEN(pktbuf);

        /* Read Out Header Fields */
        long    mfc  = parseInt(pktbuf + 12, 4);
        double  rws  = parseInt(pktbuf + 16, 4) * trueRulerClkPeriod;  // ns
        double  rww  = parseInt(pktbuf + 20, 2) * trueRulerClkPeriod;  // ns

        /* Get Major Frame Data */
        char keyname[MajorFrameProcessorModule::MAX_KEY_NAME_SIZE];
        MajorFrameProcessorModule::buildKey(mfc, keyname);
        if(cmdProc->getCurrentValue(majorFrameProcName, keyname, &mfdata, sizeof(mfdata_t)) > 0)
        {
            if(mfdata.MajorFrameCount == mfc)   mfdata_ptr = &mfdata;
//            else                                mlog(WARNING, "[%04X]: could not associate major frame data with science time tag data", mfc);
        }

        /* Create Histogram */
        if(hist == NULL)
        {
            hist = new AltimetryHistogram(type, numpkts, ATM_BINSIZE, pce, mfc, mfdata_ptr, 0.0, rws, rww);
        }

        /* Apply Corrections & Modes */
        if(fullColumnIntegration)           bin = (int)(rws * 3.0 / 20.0 / ATM_BINSIZE) % AtlasHistogram::MAX_HIST_SIZE; // 3 meters per 20 nanoseconds
        else if(alignHistograms == true)    bin = (int)((AtlasHistogram::histogramBias[type] * trueRulerClkPeriod) * 3.0 / 20.0 / ATM_BINSIZE);
        else                                bin = 0;

        /* Start Populating Bins */
        for(int i = 0; i < NUM_ATM_BINS_PER_PKT; i++)
        {
            hist->addBin(bin++, parseInt(pktbuf + 22 + (i*2), 2));
        }
    }

    /* Process Packet */
    bool sigfound = hist->calcAttributes(0.0, trueRulerClkPeriod);
    if(!sigfound)
    {
        mlog(WARNING, "[%08lX]: could not find signal in atmospheric histogram data", hist->getMajorFrameCounter());
    }

    /* Use Major Frame Data */
    if(mfdata_ptr != NULL)
    {
        /* Set Transmit Count */
        hist->setTransmitCount(mfdata.TxPulsesInMajorFrame);

        if(numpkts == 1) // only works if not integrating
        {
            /* Check Range Window Start */
            double dfc_rws = 0.0;
            if      (type == AtlasHistogram::SAM)   dfc_rws = (mfdata.StrongAtmosphericRangeWindowStart + 13) * trueRulerClkPeriod;
            else if (type == AtlasHistogram::WAM)   dfc_rws = (mfdata.WeakAtmosphericRangeWindowStart + 13) * trueRulerClkPeriod;

            if(dfc_rws != hist->getRangeWindowStart())
            {
                mlog(ERROR, "[%08lX]: %s atmospheric range window start did not match value reported by hardware, FSW: %.1lf, DFC: %.1lf", hist->getMajorFrameCounter(), AtlasHistogram::type2str(type), hist->getRangeWindowStart(), dfc_rws);
                pkt_errors++;
            }

            /* Check Range Window Width */
            double dfc_rww = 0.0;
            if      (type == AtlasHistogram::SAM)   dfc_rww = (mfdata.StrongAtmosphericRangeWindowWidth + 1) * trueRulerClkPeriod;
            else if (type == AtlasHistogram::WAM)   dfc_rww = (mfdata.WeakAtmosphericRangeWindowWidth + 1) * trueRulerClkPeriod;

            if(dfc_rww != hist->getRangeWindowWidth())
            {
                mlog(ERROR, "[%08lX]: %s atmospheric range window width did not match value reported by hardware, FSW: %.1lf, DFC: %.1lf", hist->getMajorFrameCounter(), AtlasHistogram::type2str(type), hist->getRangeWindowWidth(), dfc_rww);
                pkt_errors++;
            }
        }
    }

    /* Copy In Stats */
    hist->setPktErrors(pkt_errors);
    hist->setPktBytes(pkt_bytes);

    /* Post Histogram */
    unsigned char* buffer; // reference to serial buffer
    int size = hist->serialize(&buffer, RecordObject::REFERENCE);
    histQ->postCopy(buffer, size);
    delete hist;

    return true;
}

/*----------------------------------------------------------------------------
 * alignHistsCmd
 *----------------------------------------------------------------------------*/
int AltimetryProcessorModule::alignHistsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    alignHistograms = enable;
    cmdProc->setCurrentValue(getName(), alignHistKey, (void*)&enable, sizeof(enable));

    return 0;
}

/*----------------------------------------------------------------------------
 * fullColumnModeCmd
 *----------------------------------------------------------------------------*/
int AltimetryProcessorModule::fullColumnModeCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    fullColumnIntegration = enable;
    cmdProc->setCurrentValue(getName(), fullColumnIntegrationKey, (void*)&enable, sizeof(enable));

    return 0;
}

/*----------------------------------------------------------------------------
 * setClkPeriodCmd
 *----------------------------------------------------------------------------*/
int AltimetryProcessorModule::setClkPeriodCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    double clk_period = 0.0;

    clk_period = strtod(argv[0], NULL);
    if(clk_period == 0.0)
    {
        if(cmdProc->getCurrentValue(argv[0], TimeProcessorModule::true10Key, &clk_period, sizeof(clk_period)) <= 0)
        {
            mlog(CRITICAL, "Unable to get clock period: either invalid number supplied or invalid time processor module name supplied!");
            return -1;
        }
    }

    trueRulerClkPeriod = clk_period;

    return 0;
}

/*----------------------------------------------------------------------------
 * attachMFProcCmd
 *----------------------------------------------------------------------------*/
int AltimetryProcessorModule::attachMFProcCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    if(majorFrameProcName != NULL) delete [] majorFrameProcName;
    majorFrameProcName = StringLib::duplicate(argv[0]);

    return 0;
}
