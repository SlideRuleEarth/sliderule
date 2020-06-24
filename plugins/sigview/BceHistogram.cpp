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

#include "atlasdefines.h"
#include "BceHistogram.h"
#include "AtlasHistogram.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double BceHistogram::BINSIZE = 0.15; // meters

const char* BceHistogram::rec_type = "BceHist";

RecordObject::fieldDef_t BceHistogram::rec_def[] =
{
    {"GRL",       INT32,  offsetof(bceHist_t, grl),       1,    NULL, NATIVE_FLAGS},
    {"SPOT",      INT32,  offsetof(bceHist_t, spot),      1,    NULL, NATIVE_FLAGS},
    {"OSC_ID",    INT32,  offsetof(bceHist_t, oscId),     1,    NULL, NATIVE_FLAGS},
    {"OSC_CH",    INT32,  offsetof(bceHist_t, oscCh),     1,    NULL, NATIVE_FLAGS},
    {"SUBTYPE",   INT32,  offsetof(bceHist_t, subtype),   1,    NULL, NATIVE_FLAGS}
};

int BceHistogram::rec_elem = sizeof(BceHistogram::rec_def) / sizeof(RecordObject::fieldDef_t);

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
BceHistogram::BceHistogram(AtlasHistogram::type_t _type, int _intperiod, double _binsize, double _gps, int _grl, int _spot, int _oscid, int _oscch, int _subtype):
    AtlasHistogram(rec_type, _type, _intperiod, _binsize, NOT_PCE, 0, NULL, _gps, 0.0, 0.0)
{
    bce = (bceHist_t*)recordData;

    bce->grl = _grl;
    bce->spot = _spot;
    bce->oscId = _oscid;
    bce->oscCh = _oscch;
    bce->subtype = _subtype;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
BceHistogram::~BceHistogram(void)
{
}

/*----------------------------------------------------------------------------
 * getOscId  -
 *----------------------------------------------------------------------------*/
int BceHistogram::getOscId(void)
{
    return bce->oscId;
}

/*----------------------------------------------------------------------------
 * getOscCh  -
 *----------------------------------------------------------------------------*/
int BceHistogram::getOscCh(void)
{
    return bce->oscCh;
}

/*----------------------------------------------------------------------------
 * calcAttributes  -
 *----------------------------------------------------------------------------*/
bool BceHistogram::calcAttributes(double sigwid, double true10ns)
{
    /* Call Parent Calculation */
    AtlasHistogram::calcAttributes(sigwid, true10ns);

    return true;
}

/*----------------------------------------------------------------------------
 * defineHistogram  -
 *----------------------------------------------------------------------------*/
RecordObject::recordDefErr_t BceHistogram::defineHistogram(void)
{
    return AtlasHistogram::defineHistogram(rec_type, sizeof(bceHist_t), rec_def, rec_elem);
}
