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

/*
 * TODO: Scale background by downlink band range
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <math.h>

#include "AtlasHistogram.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double AtlasHistogram::HISTOGRAM_DEFAULT_FILTER_WIDTH = 1.5;
const double AtlasHistogram::histogramBias[NUM_TYPES] = { 4.0, 6.0, 3.0, 3.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
AtlasHistogram::AtlasHistogram(const char* _rec_type, type_t _type,
                               int _intperiod, double _binsize,
                               int _pcenum, long _mfc, mfdata_t* _mfdata,
                               double _gps, double _rws, double _rww):
    RecordObject(_rec_type)
{
    /* Set Histogram Pointer */
    hist = (hist_t*)recordData;

    /* Initialize Histogram Fields */
    hist->type              = _type;
    hist->integrationPeriod = _intperiod;
    hist->binSize           = _binsize;
    hist->pceNum            = _pcenum;
    hist->majorFrameCounter = _mfc;
    hist->gpsAtMajorFrame   = _gps;
    hist->rangeWindowStart  = _rws;
    hist->rangeWindowWidth  = _rww;

    /* Set Major Frame Data */
    if(_mfdata != NULL)
    {
        hist->majorFramePresent = true;
        LocalLib::copy(&hist->majorFrameData, _mfdata, sizeof(mfdata_t));
    }
    else
    {
        hist->majorFramePresent = false;
    }

    /* Set Attributes */
    hist->transmitCount   = 0;
    hist->noiseFloor      = 0.0;
    hist->signalRange     = 0.0;
    hist->signalWidth     = 0.0;
    hist->signalEnergy    = 0.0;
    hist->pktBytes        = 0;
    hist->pktErrors       = 0;
    hist->ignoreStartBin  = 0;
    hist->ignoreStopBin   = 0;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
AtlasHistogram::~AtlasHistogram(void)
{
}

/*----------------------------------------------------------------------------
 * setBin  -
 *----------------------------------------------------------------------------*/
bool AtlasHistogram::setBin(int bin, int val)
{
    if(bin < MAX_HIST_SIZE && bin >= 0)
    {
        hist->sum -= hist->bins[bin];
        hist->bins[bin] = val;
        hist->sum += hist->bins[bin];

        if(bin >= hist->size)
        {
            hist->size = bin + 1;
        }

        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * addBin  -
 *----------------------------------------------------------------------------*/
bool AtlasHistogram::addBin(int bin, int val)
{
    if(bin < MAX_HIST_SIZE && bin >= 0)
    {
        hist->bins[bin] += val;
        hist->sum += val;

        if(bin >= hist->size)
        {
            hist->size = bin + 1;
        }

        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * incBin  -
 *----------------------------------------------------------------------------*/
bool AtlasHistogram::incBin(int bin)
{
    if(bin < MAX_HIST_SIZE && bin >= 0)
    {
        hist->bins[bin]++;
        hist->sum++;

        if(bin >= hist->size)
        {
            hist->size = bin + 1;
        }

        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * getSum  -
 *----------------------------------------------------------------------------*/
int AtlasHistogram::getSum(void)
{
    return hist->sum;
}

/*----------------------------------------------------------------------------
 * getMean  -
 *----------------------------------------------------------------------------*/
double AtlasHistogram::getMean(void)
{
    if(hist->size > 0)    return (double)getSum() / (double)hist->size;
    else                return 0.0;
}

/*----------------------------------------------------------------------------
 * getStdev  -
 *----------------------------------------------------------------------------*/
double AtlasHistogram::getStdev(void)
{
    double diffsum = 0.0;
    double mean = getMean();

    for(int i = 0; i < hist->size; i++)
    {
        double diff = hist->bins[i] - mean;
        diffsum += diff * diff;
    }

    if(hist->size > 1)    return sqrt(diffsum / (hist->size - 1));
    else                return 0.0;
}

/*----------------------------------------------------------------------------
 * getMin  -
 *----------------------------------------------------------------------------*/
int AtlasHistogram::getMin(int start, int stop)
{
    if(stop < start) stop = hist->size;

    long minval = INT_MAX;
    for(int i = start; i < stop; i++)
    {
        if(hist->bins[i] < minval)
        {
            minval = hist->bins[i];
        }
    }

    return minval;
}

/*----------------------------------------------------------------------------
 * getMax  -
 *----------------------------------------------------------------------------*/
int AtlasHistogram::getMax(int start, int stop)
{
    if(stop < start) stop = hist->size;

    long maxval = 0;
    for(int i = start; i < stop; i++)
    {
        if(hist->bins[i] > maxval)
        {
            maxval = hist->bins[i];
        }
    }

    return maxval;
}

/*----------------------------------------------------------------------------
 * getSumRange  -
 *
 *   Notes: stop bin is inclusive
 *----------------------------------------------------------------------------*/
int AtlasHistogram::getSumRange(int start_bin, int stop_bin)
{
    long sum = 0;
    long safe_start_bin = MAX(start_bin, 0);
    long safe_stop_bin = MIN(stop_bin + 1, hist->size);

    for(int i = safe_start_bin; i < safe_stop_bin; i++)
    {
        sum += hist->bins[i];
    }

    return sum;
}

/*----------------------------------------------------------------------------
 * scale  -
 *----------------------------------------------------------------------------*/
void AtlasHistogram::scale(double _scale)
{
    for(int i = 0; i < hist->size; i++)
    {
        hist->bins[i] = (int)(hist->bins[i] * _scale);
    }
}

/*----------------------------------------------------------------------------
 * scale  -
 *----------------------------------------------------------------------------*/
void AtlasHistogram::addScalar(int scalar)
{
    for(int i = 0; i < hist->size; i++)
    {
        hist->bins[i] += scalar;
    }
}

/*----------------------------------------------------------------------------
 * getSize  -
 *----------------------------------------------------------------------------*/
int AtlasHistogram::getSize(void)
{
    return hist->size;
}

/*----------------------------------------------------------------------------
 * []  -
 *----------------------------------------------------------------------------*/
int AtlasHistogram::operator[](int index)
{
    if(index < hist->size && index >= 0)
    {
        return hist->bins[index];
    }
    else
    {
        return 0;
    }
}

/*----------------------------------------------------------------------------
 * setIgnore  -
 *----------------------------------------------------------------------------*/
void AtlasHistogram::setIgnore(int start, int stop)
{
    hist->ignoreStartBin = start;
    hist->ignoreStopBin = stop;
}

/*----------------------------------------------------------------------------
 * setPktBytes  -
 *----------------------------------------------------------------------------*/
void AtlasHistogram::setPktBytes(int bytes)
{
    hist->pktBytes = bytes;
}

/*----------------------------------------------------------------------------
 * addPktBytes  -
 *----------------------------------------------------------------------------*/
int AtlasHistogram::addPktBytes(int bytes)
{
    return (hist->pktBytes += bytes);
}

/*----------------------------------------------------------------------------
 * setPktErrors  -
 *----------------------------------------------------------------------------*/
void AtlasHistogram::setPktErrors(int errors)
{
    hist->pktErrors = errors;
}

/*----------------------------------------------------------------------------
 * addPktErrors  -
 *----------------------------------------------------------------------------*/
int AtlasHistogram::addPktErrors(int errors)
{
    return (hist->pktErrors += errors);
}

/*----------------------------------------------------------------------------
 * setTransmitCount  -
 *----------------------------------------------------------------------------*/
void AtlasHistogram::setTransmitCount(int count)
{
    hist->transmitCount = count;
}

/*----------------------------------------------------------------------------
 * addTransmitCount  -
 *----------------------------------------------------------------------------*/
int AtlasHistogram::addTransmitCount(int count)
{
    return (hist->transmitCount += count);
}

/*----------------------------------------------------------------------------
 * setTepEnergy  -
 *----------------------------------------------------------------------------*/
void AtlasHistogram::setTepEnergy(double energy)
{
    hist->tepEnergy = energy;
}

/*----------------------------------------------------------------------------
 * getXXXX  -
 *
 *   Notes: inclined methods for getting data
 *----------------------------------------------------------------------------*/

AtlasHistogram::type_t  AtlasHistogram::getType                 (void) { return hist->type;              }
int                     AtlasHistogram::getIntegrationPeriod    (void) { return hist->integrationPeriod; }
double                  AtlasHistogram::getBinSize              (void) { return hist->binSize;           }
int                     AtlasHistogram::getPceNum               (void) { return hist->pceNum;            }
long                    AtlasHistogram::getMajorFrameCounter    (void) { return hist->majorFrameCounter; }
bool                    AtlasHistogram::isMajorFramePresent     (void) { return hist->majorFramePresent; }
const mfdata_t*         AtlasHistogram::getMajorFrameData       (void) { return &hist->majorFrameData;   }
double                  AtlasHistogram::getGpsAtMajorFrame      (void) { return hist->gpsAtMajorFrame;   }
double                  AtlasHistogram::getRangeWindowStart     (void) { return hist->rangeWindowStart;  }
double                  AtlasHistogram::getRangeWindowWidth     (void) { return hist->rangeWindowWidth;  }
int                     AtlasHistogram::getTransmitCount        (void) { return hist->transmitCount;     }
double                  AtlasHistogram::getNoiseFloor           (void) { return hist->noiseFloor;        }
double                  AtlasHistogram::getNoiseBin             (void) { return hist->noiseBin;          }
double                  AtlasHistogram::getSignalRange          (void) { return hist->signalRange;       }
double                  AtlasHistogram::getSignalWidth          (void) { return hist->signalWidth;       }
double                  AtlasHistogram::getSignalEnergy         (void) { return hist->signalEnergy;      }
double                  AtlasHistogram::getTepEnergy            (void) { return hist->tepEnergy;         }
int                     AtlasHistogram::getPktBytes             (void) { return hist->pktBytes;          }
int                     AtlasHistogram::getPktErrors            (void) { return hist->pktErrors;         }

/*----------------------------------------------------------------------------
 * str2type  -
 *----------------------------------------------------------------------------*/
AtlasHistogram::type_t AtlasHistogram::str2type(const char* str)
{
         if(strcmp(str, "SAL") == 0) return SAL;
    else if(strcmp(str, "WAL") == 0) return WAL;
    else if(strcmp(str, "SAM") == 0) return SAM;
    else if(strcmp(str, "WAM") == 0) return WAM;
    else if(strcmp(str, "STT") == 0) return STT;
    else if(strcmp(str, "WTT") == 0) return WTT;
    else if(strcmp(str, "GRL") == 0) return GRL;
    else if(strcmp(str, "SHS") == 0) return SHS;
    else if(strcmp(str, "WHS") == 0) return WHS;
    else if(strcmp(str, "NAS") == 0) return NAS;

    return NAS;
}

/*----------------------------------------------------------------------------
 * type2str  -
 *----------------------------------------------------------------------------*/
const char* AtlasHistogram::type2str(type_t _type)
{
         if(_type == NAS) return "NAS";
    else if(_type == SAL) return "SAL";
    else if(_type == WAL) return "WAL";
    else if(_type == SAM) return "SAM";
    else if(_type == WAM) return "WAM";
    else if(_type == STT) return "STT";
    else if(_type == WTT) return "WTT";
    else if(_type == GRL) return "GRL";
    else if(_type == SHS) return "SHS";
    else if(_type == WHS) return "WHS";

    return "NAS";
}

/*----------------------------------------------------------------------------
 * calcAttributes  -
 *----------------------------------------------------------------------------*/
bool AtlasHistogram::calcAttributes(double sigwid, double bincal)
{
    (void)sigwid;
    (void)bincal;

    int filter_width_bins;

    /* Calculate Maximum Bins */
    for(int i = 0; i < NUM_MAX_BINS; i++)
    {
        hist->maxVal[i] = 0;
        hist->maxBin[i] = 0;
    }

    for(int i = 0; i < hist->size; i++)
    {
        int rank = NUM_MAX_BINS;
        for(int j = 0; j < NUM_MAX_BINS; j++)
        {
            if(hist->bins[i] > hist->maxVal[(NUM_MAX_BINS - 1) - j])
            {
                rank--;
            }
            else
            {
                break;
            }
        }

        if(rank < NUM_MAX_BINS)
        {
            for(int k = NUM_MAX_BINS - 1; k > rank; k--)
            {
                hist->maxVal[k] = hist->maxVal[k - 1];
                hist->maxBin[k] = hist->maxBin[k - 1];
            }

            hist->maxVal[rank] = hist->bins[i];
            hist->maxBin[rank] = i;
        }
    }

    /* Calculate Filter Width Size */
    if(sigwid == 0.0)
    {
        filter_width_bins = (int)ceil(HISTOGRAM_DEFAULT_FILTER_WIDTH / hist->binSize);
    }
    else
    {
        filter_width_bins = (int)round(sigwid * (3.0 / 20.0) / hist->binSize); // sigwid is ns
    }

    /* Calculate Signal Bin and Sum */
    int maxval = 0;
    int maxbin = 0;
    int nsize = hist->size - filter_width_bins + 1;
    for(int n = 0; n < nsize; n++)
    {
        int sum = 0;
        for(int m = 0; m < filter_width_bins; m++)
        {
            int b = n + m;
            if(b < hist->ignoreStartBin || b >= hist->ignoreStopBin)
            {
                sum += hist->bins[b];
            }
        }

        if(sum > maxval)
        {
            maxval = sum;
            maxbin = n;
        }
    }

    /* First Pass Signal Width (start and stop) */
    long begin_sigbin       = maxbin;
    long end_sigbin         = maxbin + filter_width_bins;
    long saved_begin_sigbin = begin_sigbin; // needed if signal width is overriden
    long saved_end_sigbin   = end_sigbin; // needed if signal width is overriden

    /* Calculate Edge Threshold */
    double thresh_bins = (hist->size - filter_width_bins);
    double thresh_events_per_bin = 0.0;
    if(thresh_bins > 0)
    {
        thresh_events_per_bin = (hist->sum - maxval) / thresh_bins;
    }
    double edge_thresh  = thresh_events_per_bin + (sqrt(thresh_events_per_bin) * 1);

    /* Search for new Max Bin */
    maxval = hist->bins[begin_sigbin];
    for(int i = begin_sigbin; i < end_sigbin; i++)
    {
        if(hist->bins[i] > maxval)
        {
            maxval = hist->bins[i];
            maxbin = i;
        }
    }

    /* Set Signal Bin and Width */
    hist->signalWidth = 1.0;

    /* Search Left for Beginning of Signal */
    begin_sigbin = maxbin;
    while(begin_sigbin > 0 && hist->bins[begin_sigbin] > edge_thresh)
    {
        begin_sigbin--;
        hist->signalWidth += 1.0;
    }
    if(begin_sigbin > 0)
    {
        begin_sigbin--; // one bin of padding
    }

    /* Search Right for End of Signal */
    end_sigbin = maxbin;
    while(end_sigbin < hist->size && hist->bins[end_sigbin] > edge_thresh)
    {
        end_sigbin++;
        hist->signalWidth += 1.0;
    }
    if(end_sigbin < (hist->size - 1))
    {
        end_sigbin++; // one bin of padding
    }

    /* Calculate Signal Width in Nanoseconds */
    hist->signalWidth = hist->signalWidth * hist->binSize * 20.0 / 3.0;

    /* Restore Begin and End Bins if Signal Width is Overridden */
    if(sigwid != 0.0)
    {
        begin_sigbin = saved_begin_sigbin;
        end_sigbin = saved_end_sigbin;
    }

    /* Boundary Check Bins */
    hist->beginSigBin = MAX(begin_sigbin, 0);
    hist->endSigBin = MIN(end_sigbin, hist->size - 1);

    return true;
}

/*----------------------------------------------------------------------------
 * defineHistogram  -
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t AtlasHistogram::defineHistogram(const char* rec_type, int data_size, fieldDef_t* fields, int num_fields)
{
    definition_t* def;
    recordDefErr_t status;
    hist_t hist;

    status = addDefinition(&def, rec_type, "TYPE", data_size, fields, num_fields, 128);
    if(status == SUCCESS_DEF)
    {
        addField(def, "SIZE",              INT32,  offsetof(hist_t, size),                                         sizeof(hist.size),                                       NATIVE_FLAGS);
        addField(def, "SUM",               INT32,  offsetof(hist_t, sum),                                          sizeof(hist.sum),                                        NATIVE_FLAGS);
        addField(def, "TYPE",              INT32,  offsetof(hist_t, type),                                         sizeof(hist.type),                                       NATIVE_FLAGS);
        addField(def, "INTPERIOD",         INT32,  offsetof(hist_t, integrationPeriod),                            sizeof(hist.integrationPeriod),                          NATIVE_FLAGS);
        addField(def, "BINSIZE",           DOUBLE, offsetof(hist_t, binSize),                                      sizeof(hist.binSize),                                    NATIVE_FLAGS);
        addField(def, "PCE",               INT32,  offsetof(hist_t, pceNum),                                       sizeof(hist.pceNum),                                     NATIVE_FLAGS);
        addField(def, "MFC",               INT64,  offsetof(hist_t, majorFrameCounter),                            sizeof(hist.majorFrameCounter),                          NATIVE_FLAGS);
        addField(def, "MFP",               INT8,   offsetof(hist_t, majorFramePresent),                            sizeof(hist.majorFramePresent),                          NATIVE_FLAGS);
        addField(def, "GPS",               DOUBLE, offsetof(hist_t, gpsAtMajorFrame),                              sizeof(hist.gpsAtMajorFrame),                            NATIVE_FLAGS);
        addField(def, "RWS",               DOUBLE, offsetof(hist_t, rangeWindowStart),                             sizeof(hist.rangeWindowStart),                           NATIVE_FLAGS);
        addField(def, "RWW",               DOUBLE, offsetof(hist_t, rangeWindowWidth),                             sizeof(hist.rangeWindowWidth),                           NATIVE_FLAGS);
        addField(def, "TXCNT",             INT32,  offsetof(hist_t, transmitCount),                                sizeof(hist.transmitCount),                              NATIVE_FLAGS);
        addField(def, "BKGND",             DOUBLE, offsetof(hist_t, noiseFloor),                                   sizeof(hist.noiseFloor),                                 NATIVE_FLAGS);
        addField(def, "BINBKG",            DOUBLE, offsetof(hist_t, noiseBin),                                     sizeof(hist.noiseBin),                                   NATIVE_FLAGS);
        addField(def, "SIGRNG",            DOUBLE, offsetof(hist_t, signalRange),                                  sizeof(hist.signalRange),                                NATIVE_FLAGS);
        addField(def, "SIGWID",            DOUBLE, offsetof(hist_t, signalWidth),                                  sizeof(hist.signalWidth),                                NATIVE_FLAGS);
        addField(def, "SIGPES",            DOUBLE, offsetof(hist_t, signalEnergy),                                 sizeof(hist.signalEnergy),                               NATIVE_FLAGS);
        addField(def, "PKT_BYTES",         INT32,  offsetof(hist_t, pktBytes),                                     sizeof(hist.pktBytes),                                   NATIVE_FLAGS);
        addField(def, "PKT_ERRORS",        INT32,  offsetof(hist_t, pktErrors),                                    sizeof(hist.pktErrors),                                  NATIVE_FLAGS);
        addField(def, "TEP_START",         INT32,  offsetof(hist_t, ignoreStartBin),                               sizeof(hist.ignoreStartBin),                             NATIVE_FLAGS);
        addField(def, "TEP_STOP",          INT32,  offsetof(hist_t, ignoreStopBin),                                sizeof(hist.ignoreStopBin),                              NATIVE_FLAGS);
        addField(def, "MAXVAL[0]",         INT32,  offsetof(hist_t, maxVal[0]),                                    sizeof(hist.maxVal[0]),                                  NATIVE_FLAGS);
        addField(def, "MAXVAL[1]",         INT32,  offsetof(hist_t, maxVal[1]),                                    sizeof(hist.maxVal[1]),                                  NATIVE_FLAGS);
        addField(def, "MAXVAL[2]",         INT32,  offsetof(hist_t, maxVal[2]),                                    sizeof(hist.maxVal[2]),                                  NATIVE_FLAGS);
        addField(def, "MAXBIN[0]",         INT32,  offsetof(hist_t, maxBin[0]),                                    sizeof(hist.maxBin[0]),                                  NATIVE_FLAGS);
        addField(def, "MAXBIN[1]",         INT32,  offsetof(hist_t, maxBin[1]),                                    sizeof(hist.maxBin[1]),                                  NATIVE_FLAGS);
        addField(def, "MAXBIN[2]",         INT32,  offsetof(hist_t, maxBin[2]),                                    sizeof(hist.maxBin[2]),                                  NATIVE_FLAGS);
        addField(def, "RWDROPOUT",         UINT8,  offsetof(hist_t, majorFrameData.RangeWindowDropout_Err),        sizeof(hist.majorFrameData.RangeWindowDropout_Err),      NATIVE_FLAGS);
        addField(def, "DIDNOTFINISHTX",    UINT8,  offsetof(hist_t, majorFrameData.DidNotFinishTransfer_Err),      sizeof(hist.majorFrameData.DidNotFinishTransfer_Err),    NATIVE_FLAGS);
        addField(def, "DIDNOTFINISHWR",    UINT8,  offsetof(hist_t, majorFrameData.DidNotFinishWritingData_Err),   sizeof(hist.majorFrameData.DidNotFinishWritingData_Err), NATIVE_FLAGS);
        addField(def, "DFCEDAC",           UINT32, offsetof(hist_t, majorFrameData.EDACStatusBits),                sizeof(hist.majorFrameData.EDACStatusBits),              NATIVE_FLAGS);
        addField(def, "SDRAMMISMATCH",     UINT8,  offsetof(hist_t, majorFrameData.SDRAMMismatch_Err),             sizeof(hist.majorFrameData.SDRAMMismatch_Err),           NATIVE_FLAGS);
        addField(def, "TRACKINGFIFO",      UINT8,  offsetof(hist_t, majorFrameData.Tracking_FIFOWentFull),         sizeof(hist.majorFrameData.Tracking_FIFOWentFull),       NATIVE_FLAGS);
        addField(def, "STARTTAGFIFO",      UINT8,  offsetof(hist_t, majorFrameData.StartTag_FIFOWentFull),         sizeof(hist.majorFrameData.StartTag_FIFOWentFull),       NATIVE_FLAGS);
        addField(def, "DFCSTATUS",         UINT64, offsetof(hist_t, majorFrameData.DFCStatusBits),                 sizeof(hist.majorFrameData.DFCStatusBits),               NATIVE_FLAGS);
    }

    return status;
}
