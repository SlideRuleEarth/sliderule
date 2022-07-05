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
    alt->hist.noiseFloor = (((100000.0 / alt->hist.binSize) * (50.0 / alt->hist.integrationPeriod)) * alt->hist.noiseBin) / 1000000.0;
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

    alt->hist.signalRange    = (sigloc * alt->hist.binSize * (true10ns / 10.0)) + alt->hist.rangeWindowStart;
    alt->hist.signalEnergy   = retcount / (200.0 * alt->hist.integrationPeriod);

    /* Return Heuristic on Signal Found */
    return alt->hist.maxVal[0] > (alt->hist.noiseBin + (sqrt(alt->hist.noiseBin) * 3));
}
