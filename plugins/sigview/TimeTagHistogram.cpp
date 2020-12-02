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

#include "TimeTagHistogram.h"
#include "AtlasHistogram.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* TimeTagHistogram::rec_type = "TagHist";

RecordObject::fieldDef_t TimeTagHistogram::rec_def[] =
{
    {"NUMDLB",      INT32,  offsetof(ttHist_t, numDownlinkBands),           1,    NULL, NATIVE_FLAGS},
    {"DLB0_START",  UINT16, offsetof(ttHist_t, downlinkBands[0].start),     1,    NULL, NATIVE_FLAGS},
    {"DLB0_WIDTH",  UINT16, offsetof(ttHist_t, downlinkBands[0].width),     1,    NULL, NATIVE_FLAGS},
    {"DLB0_MASK",   UINT32, offsetof(ttHist_t, downlinkBands[0].mask),      1,    NULL, NATIVE_FLAGS},
    {"DLB0_TAGCNT", INT32,  offsetof(ttHist_t, downlinkBandsTagCnt[0]),     1,    NULL, NATIVE_FLAGS},
    {"DLB1_START",  UINT16, offsetof(ttHist_t, downlinkBands[1].start),     1,    NULL, NATIVE_FLAGS},
    {"DLB1_WIDTH",  UINT16, offsetof(ttHist_t, downlinkBands[1].width),     1,    NULL, NATIVE_FLAGS},
    {"DLB1_MASK",   UINT32, offsetof(ttHist_t, downlinkBands[1].mask),      1,    NULL, NATIVE_FLAGS},
    {"DLB1_TAGCNT", INT32,  offsetof(ttHist_t, downlinkBandsTagCnt[1]),     1,    NULL, NATIVE_FLAGS},
    {"DLB2_START",  UINT16, offsetof(ttHist_t, downlinkBands[2].start),     1,    NULL, NATIVE_FLAGS},
    {"DLB2_WIDTH",  UINT16, offsetof(ttHist_t, downlinkBands[2].width),     1,    NULL, NATIVE_FLAGS},
    {"DLB2_MASK",   UINT32, offsetof(ttHist_t, downlinkBands[2].mask),      1,    NULL, NATIVE_FLAGS},
    {"DLB2_TAGCNT", INT32,  offsetof(ttHist_t, downlinkBandsTagCnt[2]),     1,    NULL, NATIVE_FLAGS},
    {"DLB3_START",  UINT16, offsetof(ttHist_t, downlinkBands[3].start),     1,    NULL, NATIVE_FLAGS},
    {"DLB3_WIDTH",  UINT16, offsetof(ttHist_t, downlinkBands[3].width),     1,    NULL, NATIVE_FLAGS},
    {"DLB3_MASK",   UINT32, offsetof(ttHist_t, downlinkBands[3].mask),      1,    NULL, NATIVE_FLAGS},
    {"DLB3_TAGCNT", INT32,  offsetof(ttHist_t, downlinkBandsTagCnt[3]),     1,    NULL, NATIVE_FLAGS},
    {"SLIP",        INT32,  offsetof(ttHist_t, slipCnt),                    1,    NULL, NATIVE_FLAGS},
    {"SEGCNT",      INT32,  offsetof(ttHist_t, pktStats.segcnt),            1,    NULL, NATIVE_FLAGS},
    {"PKTCNT",      INT32,  offsetof(ttHist_t, pktStats.pktcnt),            1,    NULL, NATIVE_FLAGS},
    {"MFC_ERRORS",  UINT32, offsetof(ttHist_t, pktStats.mfc_errors),        1,    NULL, NATIVE_FLAGS},
    {"HDR_ERRORS",  UINT32, offsetof(ttHist_t, pktStats.hdr_errors),        1,    NULL, NATIVE_FLAGS},
    {"FMT_ERRORS",  UINT32, offsetof(ttHist_t, pktStats.fmt_errors),        1,    NULL, NATIVE_FLAGS},
    {"DLB_ERRORS",  UINT32, offsetof(ttHist_t, pktStats.dlb_errors),        1,    NULL, NATIVE_FLAGS},
    {"TAG_ERRORS",  UINT32, offsetof(ttHist_t, pktStats.tag_errors),        1,    NULL, NATIVE_FLAGS},
    {"PKT_ERRORS",  UINT32, offsetof(ttHist_t, pktStats.pkt_errors),        1,    NULL, NATIVE_FLAGS},
    {"WARNINGS",    UINT32, offsetof(ttHist_t, pktStats.warnings),          1,    NULL, NATIVE_FLAGS},
    {"MIN_TAGS",    UINT32, offsetof(ttHist_t, pktStats.min_tags),          1,    NULL, NATIVE_FLAGS},
    {"MAX_TAGS",    UINT32, offsetof(ttHist_t, pktStats.max_tags),          1,    NULL, NATIVE_FLAGS},
    {"AVG_TAGS",    DOUBLE, offsetof(ttHist_t, pktStats.avg_tags),          1,    NULL, NATIVE_FLAGS},
    {"CHCNT",       INT32,  offsetof(ttHist_t, channelCounts),   NUM_CHANNELS,    NULL, NATIVE_FLAGS},
    {"CHBIAS",      DOUBLE, offsetof(ttHist_t, channelBiases),   NUM_CHANNELS,    NULL, NATIVE_FLAGS},
};

int TimeTagHistogram::rec_elem = sizeof(TimeTagHistogram::rec_def) / sizeof(RecordObject::fieldDef_t);


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
TimeTagHistogram::TimeTagHistogram( AtlasHistogram::type_t _type, int _intperiod, double _binsize,
                                    int _pcenum, long _mfc, mfdata_t* _mfdata,
                                    double _gps, double _rws, double _rww,
                                    band_t* _bands, int _numbands, bool _deep_free):
    AtlasHistogram(rec_type, _type, _intperiod, _binsize, _pcenum, _mfc, _mfdata, _gps, _rws, _rww)
{
    tt = (ttHist_t*)recordData;

    /* Initialize Internal Data */
    memset(tags, 0, sizeof(tags));
    deepFree = _deep_free;

    /* Initialize Serializable Data */
    memset(tt->channelBiases, 0, sizeof(tt->channelBiases));
    memset(tt->channelCounts, 0, sizeof(tt->channelCounts));
    tt->numDownlinkBands = _numbands;
    for(int b = 0; b < tt->numDownlinkBands; b++)
    {
        LocalLib::copy(&tt->downlinkBands[b], &_bands[b], sizeof(band_t));
    }
    for(int b = tt->numDownlinkBands; b < MAX_NUM_DLBS; b++)
    {
        memset(&tt->downlinkBands[b], 0, sizeof(band_t));
    }
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
TimeTagHistogram::~TimeTagHistogram(void)
{
    for(int i = 0; i < tt->hist.size; i++)
    {
        if(tags[i] != NULL)
        {
            if(deepFree)
            {
                for(int k = 0; k < tags[i]->length(); k++)
                {
                    delete tags[i]->get(k);
                }
            }
            delete tags[i];
            tags[i] = NULL;
        }
    }
}

/*----------------------------------------------------------------------------
 * binTag  -
 *----------------------------------------------------------------------------*/
bool TimeTagHistogram::binTag(int bin, tag_t* tag)
{
    if(bin < MAX_HIST_SIZE && bin >= 0)
    {
        if(tags[bin] == NULL)
        {
            tags[bin] = new List<tag_t*>();
        }

        tags[bin]->add(tag);
        tt->hist.bins[bin]++;
        tt->hist.sum++;

        if(bin >= tt->hist.size)
        {
            tt->hist.size = bin + 1;
        }

        tt->downlinkBandsTagCnt[tag->band]++;

        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * setSlipCnt  -
 *----------------------------------------------------------------------------*/
void TimeTagHistogram::setSlipCnt(int slip_cnt)
{
    tt->slipCnt = slip_cnt;
}

/*----------------------------------------------------------------------------
 * setPktStats  -
 *----------------------------------------------------------------------------*/
void TimeTagHistogram::setPktStats(const stat_t* stats)
{
    tt->pktStats = *stats;
}

/*----------------------------------------------------------------------------
 * incChCount  -
 *----------------------------------------------------------------------------*/
void TimeTagHistogram::incChCount(int index)
{
    if(index >= 0 && index < NUM_CHANNELS)
    {
        tt->channelCounts[index]++;
    }
}

/*----------------------------------------------------------------------------
 * getTag  -
 *----------------------------------------------------------------------------*/
TimeTagHistogram::tag_t* TimeTagHistogram::getTag(int bin, int offset)
{
    if(bin < MAX_HIST_SIZE && bin >= 0)
    {
        if(tags[bin] != NULL)
        {
            return tags[bin]->get(offset);
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * getTagList  -
 *----------------------------------------------------------------------------*/
List<TimeTagHistogram::tag_t*>* TimeTagHistogram::getTagList(int bin)
{
    if(bin < MAX_HIST_SIZE && bin >= 0)
    {
        return tags[bin];
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * getChBiases  -
 *----------------------------------------------------------------------------*/
void TimeTagHistogram::getChBiases(double bias[], bool valid[], int start_ch, int stop_ch)
{
    int safe_start_ch = MAX(start_ch, 0);
    int safe_stop_ch = MIN(stop_ch, NUM_CHANNELS - 1);

    for(int i = safe_start_ch; i <= safe_stop_ch; i++)
    {
        valid[i] = tt->channelBiasSet[i];
        if(tt->channelBiasSet[i] == true)
        {
            bias[i] = tt->channelBiases[i];
        }
    }
}

/*----------------------------------------------------------------------------
 * getChCounts  -
 *----------------------------------------------------------------------------*/
void TimeTagHistogram::getChCounts(int cnts[])
{
    for(int i = 0; i < NUM_CHANNELS; i++)
    {
        cnts[i] = tt->channelCounts[i];
    }
}

/*----------------------------------------------------------------------------
 * getChCount  -
 *----------------------------------------------------------------------------*/

int TimeTagHistogram::getChCount(int channel)
{
    if(channel < NUM_CHANNELS && channel >= 0)
    {
        return tt->channelCounts[channel];
    }
    else
    {
        return 0;
    }
}

/*----------------------------------------------------------------------------
 * getNumDownlinkBands  -
 *----------------------------------------------------------------------------*/

int TimeTagHistogram::getNumDownlinkBands(void)
{
    return tt->numDownlinkBands;
}

/*----------------------------------------------------------------------------
 * getDownlinkBands  -
 *----------------------------------------------------------------------------*/

const TimeTagHistogram::band_t* TimeTagHistogram::getDownlinkBands(void)
{
    return tt->downlinkBands;
}

/*----------------------------------------------------------------------------
 * getDownlinkBands  -
 *----------------------------------------------------------------------------*/

const TimeTagHistogram::stat_t* TimeTagHistogram::getPktStats(void)
{
    return &tt->pktStats;
}

/*----------------------------------------------------------------------------
 * defineHistogram  -
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t TimeTagHistogram::defineHistogram(void)
{
    RecordObject::recordDefErr_t rc = AtlasHistogram::defineHistogram(rec_type, sizeof(ttHist_t), rec_def, rec_elem);
    return rc;
}

/*----------------------------------------------------------------------------
 * calcAttributes  -
 *----------------------------------------------------------------------------*/
bool TimeTagHistogram::calcAttributes (double sigwid, double bincal)
{
    (void)bincal;

    /* Call Parent Function */
    AtlasHistogram::calcAttributes(sigwid, bincal);

    /* Calculate Background */
    double _sigsum = 0.0;
    for(int i = tt->hist.beginSigBin; i <= tt->hist.endSigBin && i < MAX_HIST_SIZE; i++) _sigsum += tt->hist.bins[i];

    double _ignoresum = 0.0;
    for(int i = tt->hist.ignoreStartBin; i < tt->hist.ignoreStopBin && i < MAX_HIST_SIZE; i++) _ignoresum += tt->hist.bins[i];

    double bkgnd_bins = 0 - (tt->hist.endSigBin - tt->hist.beginSigBin + 1) - (tt->hist.ignoreStopBin - tt->hist.ignoreStartBin);
    long bkgnd_count = (long)(0 - _sigsum - _ignoresum);

    for(int d = 0; d < tt->numDownlinkBands; d++)
    {
        /*
         * We only want to count tags from the spot that this histogram is tied to
         */
        if( ( (tt->hist.type == STT) && ((~(tt->downlinkBands[d].mask) & 0x0FFFF) != 0) ) || // Strong Spot Histogram
            ( (tt->hist.type == WTT) && ((~(tt->downlinkBands[d].mask) & 0xF0000) != 0) ) || // Week Spot Histogram
            ( (tt->hist.type != STT) && (tt->hist.type != WTT) ) )                           // Other
        {
            bkgnd_bins += (double)(tt->downlinkBands[d].width + 1) / (tt->hist.binSize / 10.0);
            bkgnd_count += tt->downlinkBandsTagCnt[d];
        }
    }

    /* Calculate Noise */
    tt->hist.noiseBin = 0.0;
    if(bkgnd_bins > 0) tt->hist.noiseBin = (double)bkgnd_count / bkgnd_bins;
    tt->hist.noiseFloor = (((100000.0 / tt->hist.binSize) * (50.0 / tt->hist.integrationPeriod)) * tt->hist.noiseBin) / 1000000.0;
    if(tt->hist.transmitCount != 0) tt->hist.noiseFloor *= ((double)tt->hist.integrationPeriod * 200.0) / (double)tt->hist.transmitCount; // scale for actual tx pulses received

    /* Calculate Signal Attributes */
    double range_avg = 0.0;
    double retcount = 0;
    double bincount = 0;
    double range_sum = 0.0;
    for(long bin = tt->hist.beginSigBin; bin <= tt->hist.endSigBin; bin++)
    {
        List<TimeTagHistogram::tag_t*>* taglist = getTagList(bin);
        if(taglist != NULL)
        {
            for(int i = 0; i < taglist->length(); i++)
            {
                tag_t* rx = (tag_t*)(*taglist)[i];
                range_sum += rx->range;
                retcount += 1.0;
                bincount += 1.0;
            }
        }
        retcount -= tt->hist.noiseBin;
    }

    if(bincount > 0) range_avg = range_sum / bincount;

    tt->hist.signalRange  = range_avg;
    tt->hist.signalEnergy = retcount / tt->hist.transmitCount;

    /* Calculate Center of Signal for Each Channel */
    double chrange[NUM_CHANNELS];
    for(int chindex = 0; chindex < NUM_CHANNELS; chindex++)
    {
        double chcount = 0;
        double chsum = 0.0;
        for(long bin = tt->hist.beginSigBin; bin <= tt->hist.endSigBin; bin++)
        {
            List<TimeTagHistogram::tag_t*>* taglist = getTagList(bin);
            if(taglist != NULL)
            {
                for(int i = 0; i < taglist->length(); i++)
                {
                    tag_t* rx = (tag_t*)(*taglist)[i];
                    if((rx->channel) - 1 == chindex)
                    {
                        chsum += rx->range;
                        chcount++;
                    }
                }
            }
        }

        if(chcount > 0)
        {
            chrange[chindex] = chsum / chcount;
        }
        else
        {
            chrange[chindex] = 0.0;
        }
    }

    /* Calculate Range Biases from Signal Range */
    for(int chindex = 0; chindex < NUM_CHANNELS; chindex++)
    {
        if(chrange[chindex] != 0.0)
        {
            tt->channelBiases[chindex] = chrange[chindex] - tt->hist.signalRange;
            tt->channelBiasSet[chindex] = true;
        }
        else
        {
            tt->channelBiasSet[chindex] = false;
        }
    }

    /* Always return true */
    return true;
}
