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
#include <float.h>

#include "core.h"
#include "icesat2.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/* Vegetation Record Definitions */

const char* Atl08Dispatch::vegRecType = "atl08rec.vegetation";
const RecordObject::fieldDef_t Atl08Dispatch::vegRecDef[] = {
    {"extent_id",               RecordObject::UINT64,   offsetof(vegetation_t, extent_id),      1,  NULL, NATIVE_FLAGS},
    {"segment_id",              RecordObject::UINT32,   offsetof(vegetation_t, segment_id),     1,  NULL, NATIVE_FLAGS},
    {"rgt",                     RecordObject::UINT16,   offsetof(vegetation_t, rgt),            1,  NULL, NATIVE_FLAGS},
    {"cycle",                   RecordObject::UINT16,   offsetof(vegetation_t, cycle),          1,  NULL, NATIVE_FLAGS},
    {"spot",                    RecordObject::UINT8,    offsetof(vegetation_t, spot),           1,  NULL, NATIVE_FLAGS},
    {"gt",                      RecordObject::UINT8,    offsetof(vegetation_t, gt),             1,  NULL, NATIVE_FLAGS},
    {"delta_time",              RecordObject::DOUBLE,   offsetof(vegetation_t, delta_time),     1,  NULL, NATIVE_FLAGS},
    {"lat",                     RecordObject::DOUBLE,   offsetof(vegetation_t, latitude),       1,  NULL, NATIVE_FLAGS},
    {"lon",                     RecordObject::DOUBLE,   offsetof(vegetation_t, longitude),      1,  NULL, NATIVE_FLAGS},
    {"distance",                RecordObject::DOUBLE,   offsetof(vegetation_t, distance),       1,  NULL, NATIVE_FLAGS},
    {"percentiles",             RecordObject::FLOAT,    offsetof(vegetation_t, percentiles),    NUM_PERCENTILES,  NULL, NATIVE_FLAGS}
};

const char* Atl08Dispatch::batchRecType = "atl08rec";
const RecordObject::fieldDef_t Atl08Dispatch::batchRecDef[] = {
    {"vegetation",              RecordObject::USER,     offsetof(atl08_t, vegetation),          0,  vegRecType, NATIVE_FLAGS}
};

/* Lua Functions */

const char* Atl08Dispatch::LuaMetaName = "Atl08Dispatch";
const struct luaL_Reg Atl08Dispatch::LuaMetaTable[] = {
    {NULL,          NULL}
};

/* Local Class Data */

const double Atl08Dispatch::PercentileInterval[NUM_PERCENTILES] = {
    5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 85, 98
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :atl08(<outq name>, <parms>)
 *----------------------------------------------------------------------------*/
int Atl08Dispatch::luaCreate (lua_State* L)
{
    RqstParms* parms = NULL;
    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);
        parms = (RqstParms*)getLuaObject(L, 2, RqstParms::OBJECT_TYPE);

        /* Create ATL06 Dispatch */
        return createLuaObject(L, new Atl08Dispatch(L, outq_name, parms));
    }
    catch(const RunTimeException& e)
    {
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl08Dispatch::init (void)
{
    /*
     * Note: the size associated with the batch record includes only one set of
     * vegetation stats; this forces any software accessing more than one set
     * of stats to manage the size of the record manually.
     */
    RECDEF(vegRecType, vegRecDef, sizeof(vegetation_t), NULL);
    RECDEF(batchRecType, batchRecDef, offsetof(atl08_t, vegetation[1]), NULL);
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl08Dispatch::Atl08Dispatch (lua_State* L, const char* outq_name, RqstParms* _parms):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(outq_name);
    assert(_parms);

    /* Initialize Parameters */
    parms = _parms;

    /*
     * Note: when allocating memory for this record, the full record size is used;
     * this extends the memory available past the one set of stats provided in the
     * definition.
     */
    recObj = new RecordObject(batchRecType, sizeof(atl08_t));
    recData = (atl08_t*)recObj->getRecordData();

    /* Initialize Publisher */
    outQ = new Publisher(outq_name);
    batchIndex = 0;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Atl08Dispatch::~Atl08Dispatch(void)
{
    delete outQ;
    delete recObj;
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool Atl08Dispatch::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    vegetation_t result[RqstParms::NUM_PAIR_TRACKS];
    Atl03Reader::extent_t* extent = (Atl03Reader::extent_t*)record->getRecordData();

    /* Clear Results */
    LocalLib::set(result, 0, sizeof(result));

    /* Process Extent */
    for(int t = 0; t < RqstParms::NUM_PAIR_TRACKS; t++)
    {
        /* Check Extent */
        if(extent->photon_count[t] <= 0)
        {
            continue;
        }

        /* Initialize Results */
        geolocateResult(extent, t, result);

        /* Execute Algorithm Stages */
        if(parms->stages[RqstParms::STAGE_PHOREAL])
        {
            phorealAlgorithm(extent, t, result);
        }

        /* Post Results */
        postResult(t, result);
    }

    /* Return Status */
    return true;
}

/*----------------------------------------------------------------------------
 * processTimeout
 *----------------------------------------------------------------------------*/
bool Atl08Dispatch::processTimeout (void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * processTermination
 *
 *  Note that RecordDispatcher will only call this once
 *----------------------------------------------------------------------------*/
bool Atl08Dispatch::processTermination (void)
{
    postResult(-1, NULL);
    return true;
}

/*----------------------------------------------------------------------------
 * geolocateResult
 *----------------------------------------------------------------------------*/
void Atl08Dispatch::geolocateResult (Atl03Reader::extent_t* extent, int t, vegetation_t* result)
{
    /* Get Orbit Info */
    RqstParms::sc_orient_t sc_orient = (RqstParms::sc_orient_t)extent->spacecraft_orientation;
    RqstParms::track_t track = (RqstParms::track_t)extent->reference_pair_track;

    /* Extent Attributes */
    result[t].extent_id = extent->extent_id | RqstParms::EXTENT_ID_ELEVATION | t;
    result[t].segment_id = extent->segment_id[t];
    result[t].rgt = extent->reference_ground_track_start;
    result[t].cycle = extent->cycle_start;
    result[t].spot = RqstParms::getSpotNumber(sc_orient, track, t);
    result[t].gt = RqstParms::getGroundTrack(sc_orient, track, t);

    /* Determine Starting Photon and Number of Photons */
    Atl03Reader::photon_t* ph = (Atl03Reader::photon_t*)((uint8_t*)extent + extent->photon_offset[t]);
    int num_ph = extent->photon_count[t];

    /* Calculate Sums */
    double sum_distance = 0.0;
    double sum_delta_time = 0.0;
    double sum_latitude = 0.0;
    double sum_longitude = 0.0;
    for(int i = 0; i < num_ph; i++)
    {
        sum_delta_time += ph[i].delta_time;
        sum_latitude += ph[i].latitude;
        sum_longitude += ph[i].longitude;
        sum_distance += ph[i].distance + extent->segment_distance[t];
    }

    /* Calculate Averages */
    result[t].distance = sum_distance / num_ph;
    result[t].delta_time = sum_delta_time / num_ph;
    result[t].latitude = sum_latitude / num_ph;
    result[t].longitude = sum_longitude / num_ph;
}

/*----------------------------------------------------------------------------
 * phorealAlgorithm
 *----------------------------------------------------------------------------*/
void Atl08Dispatch::phorealAlgorithm (Atl03Reader::extent_t* extent, int t, vegetation_t* result)
{
    /* Determine Starting Photon and Number of Photons */
    Atl03Reader::photon_t* ph = (Atl03Reader::photon_t*)((uint8_t*)extent + extent->photon_offset[t]);
    int num_ph = extent->photon_count[t];

    /* Determine Maximum Relief Height */
    double max_h = 0.0;
    for(int i = 0; i < num_ph; i++)
    {
        if(ph[i].relief > max_h)
        {
            max_h = ph[i].relief;
        }
    }

    /* Calculate Number of Bins */
    int num_bins = (int)ceil(max_h / parms->phoreal.binsize);
    if(num_bins > MAX_BINS)
    {
        mlog(WARNING, "Maximum number of bins truncated from %d to maximum allowed of %d", num_bins, MAX_BINS);
        result[t].pflags |= BIN_OVERFLOW_FLAG;
        num_bins = MAX_BINS;
    }
    else if(num_bins <= 0)
    {
        mlog(WARNING, "Number of bins (%lf/%lf) calculated was less than 1, setting to 1", max_h, parms->phoreal.binsize);
        result[t].pflags |= BIN_UNDERFLOW_FLAG;
        num_bins = 1;
    }

    /* Bin All Photons */
    long* bins = new long[num_bins];
    for(int i = 0; i < num_ph; i++)
    {
        int bin = (int)floor(ph[i].relief / parms->phoreal.binsize);
        if(bin < 0) bin = 0;
        else if(bin >= num_bins) bin = num_bins - 1;
        bins[bin]++;
    }

    /* Generate Cumulative Bins */
    long* cbins = new long[num_bins];
    cbins[0] = bins[0];
    for(int b = 1; b < num_bins; b++)
    {
        cbins[b] = cbins[b - 1] + bins[b];
    }

    /* Calculate Percentiles */
    {
        int b = 0; // bin index
        for(int p = 0; p < NUM_PERCENTILES; p++)
        {
            while(b < num_bins)
            {
                double percentage = ((double)cbins[b] / (double)num_ph) * 100.0;
                if(percentage >= PercentileInterval[p])
                {
                    result[t].percentiles[p] = (b + 1) * parms->phoreal.binsize;
                    break;
                }
                b++;
            }
        }
    }

    /* Clean Up */
    delete [] bins;
    delete [] cbins;
}

/*----------------------------------------------------------------------------
 * postResult
 *----------------------------------------------------------------------------*/
void Atl08Dispatch::postResult (int t, vegetation_t* result)
{
    batchMutex.lock();
    {
        /* Populate Batch Record */
        if(result) recData->vegetation[batchIndex++] = result[t];

        /* Check If Batch Record Should Be Posted*/
        if((!result && batchIndex > 0) || batchIndex == BATCH_SIZE)
        {
            /* Calculate Record Size */
            int size = batchIndex * sizeof(vegetation_t);

            /* Serialize Record */
            unsigned char* buffer;
            int bufsize = recObj->serialize(&buffer, RecordObject::REFERENCE, size);

            /* Post Record */
            int post_status = MsgQ::STATE_TIMEOUT;
            while((post_status = outQ->postCopy(buffer, bufsize, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT);

            /* Reset Batch Index */
            batchIndex = 0;
        }
    }
    batchMutex.unlock();
}
