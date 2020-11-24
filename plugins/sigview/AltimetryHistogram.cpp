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

#include "AltimetryHistogram.h"
#include "AtlasHistogram.h"
#include "atlasdefines.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* AltimetryHistogram::rec_type = "AltHist";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
AltimetryHistogram::AltimetryHistogram(AtlasHistogram::type_t _type, int _intperiod, double _binsize,
                                       int _pcenum, long _mfc, mfdata_t* _mfdata,
                                       double _gps, double _rws, double _rww):
    AtlasHistogram(rec_type, _type, _intperiod, _binsize, _pcenum, _mfc, _mfdata, _gps, _rws, _rww)
{
    alt = (altHist_t*)recordData;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
AltimetryHistogram::~AltimetryHistogram(void)
{
}

/*----------------------------------------------------------------------------
 * defineHistogram  -
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t AltimetryHistogram::defineHistogram(void)
{
    RecordObject::recordDefErr_t rc = AtlasHistogram::defineHistogram(rec_type, sizeof(altHist_t), NULL, 0);
    return rc;
}

/*----------------------------------------------------------------------------
 * calcAttributes  -
 *----------------------------------------------------------------------------*/
bool AltimetryHistogram::calcAttributes(double sigwid, double true10ns)
{
    /* Call Parent Calculation */
    AtlasHistogram::calcAttributes(sigwid, true10ns);

    /* Calculate Background */
    double bkgnd_bins = (alt->hist.size - (alt->hist.endSigBin - alt->hist.beginSigBin + 1) - (alt->hist.ignoreStopBin - alt->hist.ignoreStartBin));

    double _sigsum = 0.0;
    for(int i = alt->hist.beginSigBin; i <= alt->hist.endSigBin; i++) _sigsum += alt->hist.bins[i];

    double _ignoresum = 0.0;
    for(int i = alt->hist.ignoreStartBin; i < alt->hist.ignoreStopBin; i++) _ignoresum += alt->hist.bins[i];

    alt->hist.noiseBin = 0.0;
    if(bkgnd_bins > 0) alt->hist.noiseBin = (alt->hist.sum - _sigsum - _ignoresum) / bkgnd_bins;
    alt->hist.noiseFloor = (((15000.0 / alt->hist.binSize) * (50.0 / alt->hist.integrationPeriod)) * alt->hist.noiseBin) / 1000000.0;
    if(alt->hist.transmitCount != 0) alt->hist.noiseFloor *= ((double)alt->hist.integrationPeriod * 200.0) / (double)alt->hist.transmitCount; // scale for actual tx pulses received

    /* Calculate Type Specific Attributes */
    double  sigloc          = 0.0;
    double  retcount        = 0.0;
    long    bincount        = 0;
    for(long bin = alt->hist.beginSigBin; bin <= alt->hist.endSigBin; bin++)
    {
        sigloc += bin * alt->hist.bins[bin];
        retcount += (alt->hist.bins[bin] - alt->hist.noiseBin);
        bincount += alt->hist.bins[bin];
    }
    sigloc /= (double)bincount;

    alt->hist.signalRange    = (sigloc * alt->hist.binSize * (true10ns / 1.5)) + alt->hist.rangeWindowStart;
    alt->hist.signalEnergy   = retcount / (200.0 * alt->hist.integrationPeriod);

    /* Return Heuristic on Signal Found */
    return alt->hist.maxVal[0] > (alt->hist.noiseBin + (sqrt(alt->hist.noiseBin) * 3));
}
