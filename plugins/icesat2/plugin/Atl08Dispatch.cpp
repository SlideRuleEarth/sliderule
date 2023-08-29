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
    {"extent_id",               RecordObject::UINT64,   offsetof(vegetation_t, extent_id),              1, NULL, NATIVE_FLAGS},
    {"segment_id",              RecordObject::UINT32,   offsetof(vegetation_t, segment_id),             1, NULL, NATIVE_FLAGS},
    {"rgt",                     RecordObject::UINT16,   offsetof(vegetation_t, rgt),                    1, NULL, NATIVE_FLAGS},
    {"cycle",                   RecordObject::UINT16,   offsetof(vegetation_t, cycle),                  1, NULL, NATIVE_FLAGS},
    {"spot",                    RecordObject::UINT8,    offsetof(vegetation_t, spot),                   1, NULL, NATIVE_FLAGS},
    {"gt",                      RecordObject::UINT8,    offsetof(vegetation_t, gt),                     1, NULL, NATIVE_FLAGS},
    {"ph_count",                RecordObject::UINT32,   offsetof(vegetation_t, photon_count),           1, NULL, NATIVE_FLAGS},
    {"gnd_ph_count",            RecordObject::UINT32,   offsetof(vegetation_t, ground_photon_count),    1, NULL, NATIVE_FLAGS},
    {"veg_ph_count",            RecordObject::UINT32,   offsetof(vegetation_t, vegetation_photon_count),1, NULL, NATIVE_FLAGS},
    {"landcover",               RecordObject::UINT8,    offsetof(vegetation_t, landcover),              1, NULL, NATIVE_FLAGS},
    {"snowcover",               RecordObject::UINT8,    offsetof(vegetation_t, snowcover),              1, NULL, NATIVE_FLAGS},
    {"time",                    RecordObject::TIME8,    offsetof(vegetation_t, time_ns),                1, NULL, NATIVE_FLAGS},
    {"latitude",                RecordObject::DOUBLE,   offsetof(vegetation_t, latitude),               1, NULL, NATIVE_FLAGS},
    {"longitude",               RecordObject::DOUBLE,   offsetof(vegetation_t, longitude),              1, NULL, NATIVE_FLAGS},
    {"x_atc",                   RecordObject::DOUBLE,   offsetof(vegetation_t, x_atc),                  1, NULL, NATIVE_FLAGS},
    {"solar_elevation",         RecordObject::FLOAT,    offsetof(vegetation_t, solar_elevation),        1, NULL, NATIVE_FLAGS},
    {"h_te_median",             RecordObject::FLOAT,    offsetof(vegetation_t, h_te_median),            1, NULL, NATIVE_FLAGS},
    {"h_max_canopy",            RecordObject::FLOAT,    offsetof(vegetation_t, h_max_canopy),           1, NULL, NATIVE_FLAGS},
    {"h_min_canopy",            RecordObject::FLOAT,    offsetof(vegetation_t, h_min_canopy),           1, NULL, NATIVE_FLAGS},
    {"h_mean_canopy",           RecordObject::FLOAT,    offsetof(vegetation_t, h_mean_canopy),          1, NULL, NATIVE_FLAGS},
    {"h_canopy",                RecordObject::FLOAT,    offsetof(vegetation_t, h_canopy),               1, NULL, NATIVE_FLAGS},
    {"canopy_openness",         RecordObject::FLOAT,    offsetof(vegetation_t, canopy_openness),        1, NULL, NATIVE_FLAGS},
    {"canopy_h_metrics",        RecordObject::FLOAT,    offsetof(vegetation_t, canopy_h_metrics),       NUM_PERCENTILES, NULL, NATIVE_FLAGS}
};

const char* Atl08Dispatch::batchRecType = "atl08rec";
const RecordObject::fieldDef_t Atl08Dispatch::batchRecDef[] = {
    {"vegetation",              RecordObject::USER,     offsetof(atl08_t, vegetation),              0,  vegRecType, NATIVE_FLAGS}
};

const char* Atl08Dispatch::waveRecType = "waverec";
const RecordObject::fieldDef_t Atl08Dispatch::waveRecDef[] = {
    {"extent_id",               RecordObject::UINT64,   offsetof(waveform_t, extent_id),            1, NULL, NATIVE_FLAGS},
    {"num_bins",                RecordObject::UINT16,   offsetof(waveform_t, num_bins),             1, NULL, NATIVE_FLAGS},
    {"binsize",                 RecordObject::FLOAT,    offsetof(waveform_t, binsize),              1, NULL, NATIVE_FLAGS},
    {"waveform",                RecordObject::FLOAT,    offsetof(waveform_t, waveform),             0, NULL, NATIVE_FLAGS}
};

/* Lua Functions */

const char* Atl08Dispatch::LuaMetaName = "Atl08Dispatch";
const struct luaL_Reg Atl08Dispatch::LuaMetaTable[] = {
    {NULL,          NULL}
};

/* Local Class Data */

const double Atl08Dispatch::PercentileInterval[NUM_PERCENTILES] = {
    5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :atl08(<outq name>, <parms>)
 *----------------------------------------------------------------------------*/
int Atl08Dispatch::luaCreate (lua_State* L)
{
    Icesat2Parms* parms = NULL;
    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);
        parms = (Icesat2Parms*)getLuaObject(L, 2, Icesat2Parms::OBJECT_TYPE);

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
     * of stats to manage the size of the record manually.  Same for waveform
     * record - except it allows for a waveform of no bins.
     */
    RECDEF(vegRecType, vegRecDef, sizeof(vegetation_t), NULL);
    RECDEF(batchRecType, batchRecDef, offsetof(atl08_t, vegetation[1]), NULL);
    RECDEF(waveRecType, waveRecDef, offsetof(waveform_t, waveform), NULL);
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl08Dispatch::Atl08Dispatch (lua_State* L, const char* outq_name, Icesat2Parms* _parms):
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
bool Atl08Dispatch::processRecord (RecordObject* record, okey_t key, recVec_t* records)
{
    (void)key;

    Atl03Reader::extent_t* extent = (Atl03Reader::extent_t*)record->getRecordData();

    /* Check Extent */
    if(extent->photon_count <= 0)
    {
        return true;
    }

    /* Initialize Results */
    vegetation_t result;
    geolocateResult(extent, result);

    /* Execute Algorithm Stages */
    if(parms->stages[Icesat2Parms::STAGE_PHOREAL])
    {
        phorealAlgorithm(extent, result);
    }

    /* Post Results */
    postResult(&result);

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
    postResult(NULL);
    return true;
}

/*----------------------------------------------------------------------------
 * geolocateResult
 *----------------------------------------------------------------------------*/
void Atl08Dispatch::geolocateResult (Atl03Reader::extent_t* extent, vegetation_t& result)
{
    /* Get Orbit Info */
    Icesat2Parms::sc_orient_t sc_orient = (Icesat2Parms::sc_orient_t)extent->spacecraft_orientation;
    Icesat2Parms::track_t track = (Icesat2Parms::track_t)extent->reference_pair_track;

    /* Extent Attributes */
    result.extent_id = extent->extent_id | Icesat2Parms::EXTENT_ID_ELEVATION;
    result.segment_id = extent->segment_id;
    result.rgt = extent->reference_ground_track;
    result.cycle = extent->cycle;
    result.spot = Icesat2Parms::getSpotNumber(sc_orient, track, extent->reference_pair_track);
    result.gt = Icesat2Parms::getGroundTrack(sc_orient, track, extent->reference_pair_track);
    result.photon_count = extent->photon_count;
    result.solar_elevation = extent->solar_elevation;

    /* Determine Starting Photon and Number of Photons */
    Atl03Reader::photon_t* ph = extent->photons;
    uint32_t num_ph = extent->photon_count;

    /* Calculate Geolocation Fields */
    if(num_ph == 0)
    {
        result.time_ns = 0;
        result.latitude = 0.0;
        result.longitude = 0.0;
        result.x_atc = extent->segment_distance;
    }
    else if(parms->phoreal.geoloc == Icesat2Parms::PHOREAL_CENTER)
    {
        /* Calculate Sums */
        double time_ns_min = DBL_MAX, time_ns_max = -DBL_MAX;
        double latitude_min = DBL_MAX, latitude_max = -DBL_MAX;
        double longitude_min = DBL_MAX, longitude_max = -DBL_MAX;
        double x_atc_min = DBL_MAX, x_atc_max = -DBL_MAX;
        for(uint32_t i = 0; i < num_ph; i++)
        {
            if(ph[i].time_ns    < time_ns_min)      time_ns_min     = ph[i].time_ns;
            if(ph[i].latitude   < latitude_min)     latitude_min    = ph[i].latitude;
            if(ph[i].longitude  < longitude_min)    longitude_min   = ph[i].longitude;
            if(ph[i].x_atc      < x_atc_min)        x_atc_min       = ph[i].x_atc;

            if(ph[i].time_ns    > time_ns_max)      time_ns_max     = ph[i].time_ns;
            if(ph[i].latitude   > latitude_max)     latitude_max    = ph[i].latitude;
            if(ph[i].longitude  > longitude_max)    longitude_max   = ph[i].longitude;
            if(ph[i].x_atc      > x_atc_max)        x_atc_max       = ph[i].x_atc;
        }

        /* Calculate Averages */
        result.time_ns = (int64_t)((time_ns_min + time_ns_max) / 2.0);
        result.latitude = (latitude_min + latitude_max) / 2.0;
        result.longitude = (longitude_min + longitude_max) / 2.0;
        result.x_atc = ((x_atc_min + x_atc_max) / 2.0) + extent->segment_distance;
    }
    else if(parms->phoreal.geoloc == Icesat2Parms::PHOREAL_MEAN)
    {
        /* Calculate Sums */
        double sum_time_ns = 0.0;
        double sum_latitude = 0.0;
        double sum_longitude = 0.0;
        double sum_x_atc = 0.0;
        for(uint32_t i = 0; i < num_ph; i++)
        {
            sum_time_ns += ph[i].time_ns;
            sum_latitude += ph[i].latitude;
            sum_longitude += ph[i].longitude;
            sum_x_atc += ph[i].x_atc + extent->segment_distance;
        }

        /* Calculate Averages */
        result.time_ns = (int64_t)(sum_time_ns / num_ph);
        result.latitude = sum_latitude / num_ph;
        result.longitude = sum_longitude / num_ph;
        result.x_atc = sum_x_atc / num_ph;
    }
    else if(parms->phoreal.geoloc == Icesat2Parms::PHOREAL_MEDIAN)
    {
        uint32_t center_ph = num_ph / 2;
        if(num_ph % 2 == 1) // Odd Number of Photons
        {
            result.time_ns = ph[center_ph].time_ns;
            result.latitude = ph[center_ph].latitude;
            result.longitude = ph[center_ph].longitude;
            result.x_atc = ph[center_ph].x_atc + extent->segment_distance;
        }
        else // Even Number of Photons
        {
            result.time_ns = (ph[center_ph].time_ns + ph[center_ph - 1].time_ns) / 2;
            result.latitude = (ph[center_ph].latitude + ph[center_ph - 1].latitude) / 2;
            result.longitude = (ph[center_ph].longitude + ph[center_ph - 1].longitude) / 2;
            result.x_atc = ((ph[center_ph].x_atc + ph[center_ph - 1].x_atc) / 2) + extent->segment_distance;
        }
    }

    /* Land and Snow Cover Flags */
    if(num_ph == 0)
    {
        result.landcover = Icesat2Parms::INVALID_FLAG;
        result.snowcover = Icesat2Parms::INVALID_FLAG;
    }
    else
    {
        /* Find Center Ph */
        uint32_t center_ph = 0;
        double diff_min = DBL_MAX;
        for(uint32_t i = 0; i < num_ph; i++)
        {
            double diff = abs(ph[i].time_ns - result.time_ns);
            if(diff < diff_min)
            {
                diff_min = diff;
                center_ph = i;
            }
        }
        result.landcover = ph[center_ph].landcover;
        result.snowcover = ph[center_ph].snowcover;
    }
}

/*----------------------------------------------------------------------------
 * phorealAlgorithm
 *----------------------------------------------------------------------------*/
void Atl08Dispatch::phorealAlgorithm (Atl03Reader::extent_t* extent, vegetation_t& result)
{
    /* Determine Starting Photon and Number of Photons */
    Atl03Reader::photon_t* ph = extent->photons;
    uint32_t num_ph = extent->photon_count;

    /* Determine Number of Ground and Vegetation Photons */
    long gnd_cnt = 0;
    long veg_cnt = 0;
    for(uint32_t i = 0; i < num_ph; i++)
    {
        if(isGround(&ph[i]) || parms->phoreal.use_abs_h)
        {
            gnd_cnt++;
        }
        else if(isVegetation(&ph[i]) || parms->phoreal.use_abs_h)
        {
            veg_cnt++;
        }
    }
    result.ground_photon_count = gnd_cnt;
    result.vegetation_photon_count = veg_cnt;

    /* Create Ground and Vegetation Photon Index Arrays */
    long* gnd_index = new long [gnd_cnt];
    long* veg_index = new long [veg_cnt];
    long g = 0, v = 0;
    for(long i = 0; i < num_ph; i++)
    {
        if(isGround(&ph[i]) || parms->phoreal.use_abs_h)
        {
            gnd_index[g++] = i;
        }
        else if(isVegetation(&ph[i]) || parms->phoreal.use_abs_h)
        {
            veg_index[v++] = i;
        }
    }

    /* Sort Ground and Vegetation Photon Index Arrays */
    quicksort(gnd_index, ph, &Atl03Reader::photon_t::height, 0, gnd_cnt - 1);
    quicksort(veg_index, ph, &Atl03Reader::photon_t::relief, 0, veg_cnt - 1);

    /* Determine Min,Max,Avg Heights */
    double min_h = DBL_MAX;
    double max_h = -DBL_MAX;
    double sum_h = 0.0;
    if(veg_cnt > 0)
    {
        for(long i = 0; i < veg_cnt; i++)
        {
            sum_h += ph[veg_index[i]].relief;
            if(ph[veg_index[i]].relief > max_h)
            {
                max_h = ph[veg_index[i]].relief;
            }
            if(ph[veg_index[i]].relief < min_h)
            {
                min_h = ph[veg_index[i]].relief;
            }
        }
    }
    else
    {
        max_h = 0.0;
        min_h = 0.0;
    }
    result.h_max_canopy = max_h;
    result.h_min_canopy = min_h;
    result.h_mean_canopy = sum_h / (double)veg_cnt;

    /* Calculate Stdev of Heights */
    double std_h = 0.0;
    for(long i = 0; i < veg_cnt; i++)
    {
        double delta = (ph[veg_index[i]].relief - result.h_mean_canopy);
        std_h += delta * delta;
    }
    result.canopy_openness = sqrt(std_h / (double)veg_cnt);

    /* Calculate Number of Bins */
    int num_bins = (int)ceil((max_h - min_h) / parms->phoreal.binsize);
    if(num_bins > MAX_BINS)
    {
        mlog(WARNING, "Maximum number of bins truncated from %d to maximum allowed of %d", num_bins, MAX_BINS);
        result.pflags |= BIN_OVERFLOW_FLAG;
        num_bins = MAX_BINS;
    }
    else if(num_bins <= 0)
    {
        result.pflags |= BIN_UNDERFLOW_FLAG;
        num_bins = 1;
    }

    /* Bin Photons */
    long* bins = new long[num_bins];
    memset(bins, 0, num_bins * sizeof(long));
    for(long i = 0; i < veg_cnt; i++)
    {
        int bin = (int)floor((ph[veg_index[i]].relief - min_h) / parms->phoreal.binsize);
        if(bin < 0) bin = 0;
        else if(bin >= num_bins) bin = num_bins - 1;
        bins[bin]++;
    }

    /* Send Waveforms */
    if(parms->phoreal.send_waveform && num_bins > 0)
    {
        int recsize = offsetof(waveform_t, waveform) + (num_bins * sizeof(float));
        RecordObject waverec(waveRecType, recsize, false);
        waveform_t* data = (waveform_t*)waverec.getRecordData();
        data->extent_id = extent->extent_id | Icesat2Parms::EXTENT_ID_ELEVATION;
        data->num_bins = num_bins;
        data->binsize = parms->phoreal.binsize;
        for(int b = 0; b < num_bins; b++)
        {
            data->waveform[b] = (float)((double)bins[b] / (double)num_ph);
        }
        waverec.post(outQ);
    }

    /* Generate Cumulative Bins */
    long* cbins = new long[num_bins];
    cbins[0] = bins[0];
    for(int b = 1; b < num_bins; b++)
    {
        cbins[b] = cbins[b - 1] + bins[b];
    }

    /* Find Median Terrain Height */
    float h_te_median = 0.0;
    if(gnd_cnt > 0)
    {
        if(gnd_cnt % 2 == 0) // even
        {
            long i0 = (gnd_cnt - 1) / 2;
            long i1 = ((gnd_cnt - 1) / 2) + 1;
            h_te_median = (ph[gnd_index[i0]].height + ph[gnd_index[i1]].height) / 2.0;
        }
        else // odd
        {
            long i0 = (gnd_cnt - 1) / 2;
            h_te_median = ph[gnd_index[i0]].height;
        }
    }
    result.h_te_median = h_te_median;

    /* Calculate Percentiles */
    {
        int b = 0; // bin index
        for(int p = 0; p < NUM_PERCENTILES; p++)
        {
            while(b < num_bins)
            {
                double percentage = ((double)cbins[b] / (double)veg_cnt) * 100.0;
                if(percentage >= PercentileInterval[p] && cbins[b] > 0)
                {
                    result.canopy_h_metrics[p] = ph[veg_index[cbins[b] - 1]].relief;
                    break;
                }
                b++;
            }
        }
        /* Find 98th Percentile */
        while(b < num_bins)
        {
            double percentage = ((double)cbins[b] / (double)veg_cnt) * 100.0;
            if(percentage >= 98.0 && cbins[b] > 0)
            {
                result.h_canopy = ph[veg_index[cbins[b] - 1]].relief;
                break;
            }
            b++;
        }
    }

    /* Clean Up */
    delete [] gnd_index;
    delete [] veg_index;
    delete [] bins;
    delete [] cbins;
}

/*----------------------------------------------------------------------------
 * postResult
 *----------------------------------------------------------------------------*/
void Atl08Dispatch::postResult (vegetation_t* result)
{
    batchMutex.lock();
    {
        /* Populate Batch Record */
        if(result) recData->vegetation[batchIndex++] = *result;

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

/*----------------------------------------------------------------------------
 * postResult
 *----------------------------------------------------------------------------*/
void Atl08Dispatch::quicksort (long* index_array, Atl03Reader::photon_t* ph_array, float Atl03Reader::photon_t::*field, int start, int end)
{
    if(start < end)
    {
        int partition = quicksortpartition(index_array, ph_array, field, start, end);
        quicksort(index_array, ph_array, field, start, partition);
        quicksort(index_array, ph_array, field, partition + 1, end);
    }
}

/*----------------------------------------------------------------------------
 * postResult
 *----------------------------------------------------------------------------*/
int Atl08Dispatch::quicksortpartition (long* index_array, Atl03Reader::photon_t* ph_array, float Atl03Reader::photon_t::*field, int start, int end)
{
    double pivot = ph_array[index_array[(start + end) / 2]].*field;

    start--;
    end++;
    while(true)
    {
        while (ph_array[index_array[++start]].*field < pivot);
        while (ph_array[index_array[--end]].*field > pivot);
        if (start >= end) return end;

        long tmp = index_array[start];
        index_array[start] = index_array[end];
        index_array[end] = tmp;
    }
}
