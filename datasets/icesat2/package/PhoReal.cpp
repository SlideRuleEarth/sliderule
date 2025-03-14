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

#include "OsApi.h"
#include "GeoLib.h"
#include "PhoReal.h"
#include "Icesat2Fields.h"
#include "Atl03DataFrame.h"
#include "FieldColumn.h"
#include "FieldArray.h"

/******************************************************************************
 * DATA
 ******************************************************************************/

 const double PhoReal::PercentileInterval[NUM_PERCENTILES] = {
    5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95
};

const char* PhoReal::LUA_META_NAME = "PhoReal";
const struct luaL_Reg PhoReal::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

 /*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int PhoReal::luaCreate (lua_State* L)
{
    Icesat2Fields* _parms = NULL;

    try
    {
        _parms = dynamic_cast<Icesat2Fields*>(getLuaObject(L, 1, Icesat2Fields::OBJECT_TYPE));
        return createLuaObject(L, new PhoReal(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
PhoReal::PhoReal (lua_State* L, Icesat2Fields* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
PhoReal::~PhoReal (void)
{
    if(parms) parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool PhoReal::run (GeoDataFrame* dataframe)
{
    const double start = TimeLib::latchtime();
    Atl03DataFrame& df = *dynamic_cast<Atl03DataFrame*>(dataframe);

    // check df not empty
    if(df.length() <= 0) return true;

    // create new dataframe columns
    FieldColumn<time8_t>*   time_ns                 = new FieldColumn<time8_t>(Field::TIME_COLUMN); // nanoseconds from GPS epoch
    FieldColumn<double>*    latitude                = new FieldColumn<double>(Field::Y_COLUMN);     // EPSG:7912
    FieldColumn<double>*    longitude               = new FieldColumn<double>(Field::X_COLUMN);     // EPSG:7912
    FieldColumn<double>*    x_atc                   = new FieldColumn<double>;                      // distance from the equator
    FieldColumn<float>*     y_atc                   = new FieldColumn<float>;                       // distance from reference track
    FieldColumn<uint32_t>*  photon_start            = new FieldColumn<uint32_t>;                    // photon index of start of extent
    FieldColumn<uint32_t>*  photon_count            = new FieldColumn<uint32_t>;                    // number of photons used in final elevation calculation
    FieldColumn<uint16_t>*  pflags                  = new FieldColumn<uint16_t>;                    // processing flags
    FieldColumn<uint32_t>*  ground_photon_count     = new FieldColumn<uint32_t>;                    // number of photons labeled as ground in extent
    FieldColumn<uint32_t>*  vegetation_photon_count = new FieldColumn<uint32_t>;                    // number of photons labeled as canopy or top of canopy in extent
    FieldColumn<uint8_t>*   landcover               = new FieldColumn<uint8_t>;                     // atl08 land_segments/segments_landcover
    FieldColumn<uint8_t>*   snowcover               = new FieldColumn<uint8_t>;                     // atl08 land_segments/segments_snowcover
    FieldColumn<float>*     solar_elevation         = new FieldColumn<float>;                       // atl03 solar elevation
    FieldColumn<float>*     h_te_median             = new FieldColumn<float>;                       // median terrain height for ground photons
    FieldColumn<float>*     h_max_canopy            = new FieldColumn<float>;                       // maximum relief height for canopy photons
    FieldColumn<float>*     h_min_canopy            = new FieldColumn<float>;                       // minimum relief height for canopy photons
    FieldColumn<float>*     h_mean_canopy           = new FieldColumn<float>;                       // average relief height for canopy photons
    FieldColumn<float>*     h_canopy                = new FieldColumn<float>;                       // 98th percentile relief height for canopy photons
    FieldColumn<float>*     canopy_openness         = new FieldColumn<float>;                       // standard deviation of relief height for canopy photons
    FieldColumn<FieldArray<float,NUM_PERCENTILES>>* canopy_h_metrics = new FieldColumn<FieldArray<float,NUM_PERCENTILES>>;  // relief height at given percentile for canopy photons

    // create new ancillary dataframe columns
    Dictionary<GeoDataFrame::ancillary_t>* ancillary_columns = NULL;
    GeoDataFrame::createAncillaryColumns(&ancillary_columns, parms->atl03GeoFields);
    GeoDataFrame::createAncillaryColumns(&ancillary_columns, parms->atl03CorrFields);
    GeoDataFrame::createAncillaryColumns(&ancillary_columns, parms->atl03PhFields);
    GeoDataFrame::createAncillaryColumns(&ancillary_columns, parms->atl08Fields);

    // for each photon
    int32_t i0 = 0; // start row
    while(i0 < df.length())
    {
        uint32_t _pflags = 0;

        // find end of extent
        int32_t i1 = i0; // end row
        while( (i1 < df.length()) &&
               ((df.x_atc[i1] - df.x_atc[i0]) < parms->extentLength.value) )
        {
            i1++;
        }

        // check for end of dataframe
        if(i1 == df.length()) i1--;

        // check for valid extent
        if (i1 < i0)
        {
            mlog(CRITICAL, "Invalid extent (%d, %d)\n", i0, i1);
            break;
        }

        // calculate number of photons in extent
        const int32_t num_photons = i1 - i0 + 1;

        // check minimum extent length
        if((df.x_atc[i1] - df.x_atc[i0]) < parms->minAlongTrackSpread)
        {
            _pflags |= Icesat2Fields::PFLAG_SPREAD_TOO_SHORT;
        }

        // check minimum number of photons
        if(num_photons < parms->minPhotonCount)
        {
            _pflags |= Icesat2Fields::PFLAG_TOO_FEW_PHOTONS;
        }

        // run phoreal algorithm
        if(_pflags == 0 || parms->passInvalid)
        {
            result_t result;
            geolocate(df, i0, num_photons, result);
            algorithm(df, i0, num_photons, result);

            pflags->append(result.pflags | _pflags);
            time_ns->append(static_cast<time8_t>(result.time_ns));
            latitude->append(result.latitude);
            longitude->append(result.longitude);
            x_atc->append(result.x_atc);
            y_atc->append(result.y_atc);

            ground_photon_count->append(result.ground_photon_count);
            vegetation_photon_count->append(result.vegetation_photon_count);
            h_te_median->append(result.h_te_median);
            h_max_canopy->append(result.h_max_canopy);
            h_min_canopy->append(result.h_min_canopy);
            h_mean_canopy->append(result.h_mean_canopy);
            h_canopy->append(result.h_canopy);
            canopy_openness->append(result.canopy_openness);
            canopy_h_metrics->append(result.canopy_h_metrics);

            const uint32_t center_ph = i0 + (num_photons / 2);
            photon_start->append(df.ph_index[i0]);
            photon_count->append(static_cast<uint32_t>(num_photons));
            landcover->append(df.landcover[center_ph]);
            snowcover->append(df.snowcover[center_ph]);
            solar_elevation->append(df.solar_elevation[center_ph]);

            GeoDataFrame::populateAncillaryColumns(ancillary_columns, df, i0, num_photons);
        }

        // find start of next extent
        const int32_t prev_i0 = i0;
        while( (i0 < df.length()) &&
               ((df.x_atc[i0] - df.x_atc[prev_i0]) < parms->extentStep.value) )
        {
            i0++;
        }

        // check extent moved
        if(i0 == prev_i0)
        {
            mlog(CRITICAL, "Failed to move to next extent in track");
            break;
        }
    }

    // clear all columns from original dataframe
    dataframe->clear(); // frees memory

    // install new columns into dataframe
    dataframe->addExistingColumn("time_ns",                 time_ns);
    dataframe->addExistingColumn("latitude",                latitude);
    dataframe->addExistingColumn("longitude",               longitude);
    dataframe->addExistingColumn("x_atc",                   x_atc);
    dataframe->addExistingColumn("y_atc",                   y_atc);
    dataframe->addExistingColumn("photon_start",            photon_start);
    dataframe->addExistingColumn("photon_count",            photon_count);
    dataframe->addExistingColumn("pflags",                  pflags);
    dataframe->addExistingColumn("ground_photon_count",     ground_photon_count);
    dataframe->addExistingColumn("vegetation_photon_count", vegetation_photon_count);
    dataframe->addExistingColumn("landcover",               landcover);
    dataframe->addExistingColumn("snowcover",               snowcover);
    dataframe->addExistingColumn("solar_elevation",         solar_elevation);
    dataframe->addExistingColumn("h_te_median",             h_te_median);
    dataframe->addExistingColumn("h_max_canopy",            h_max_canopy);
    dataframe->addExistingColumn("h_min_canopy",            h_min_canopy);
    dataframe->addExistingColumn("h_mean_canopy",           h_mean_canopy);
    dataframe->addExistingColumn("h_canopy",                h_canopy);
    dataframe->addExistingColumn("canopy_openness",         canopy_openness);
    dataframe->addExistingColumn("canopy_h_metrics",        canopy_h_metrics);

    // install ancillary columns into dataframe
    GeoDataFrame::addAncillaryColumns (ancillary_columns, dataframe);
    delete ancillary_columns;

    // finalize dataframe
    dataframe->populateDataframe();

    // update runtime and return success
    updateRunTime(TimeLib::latchtime() - start);
    return true;
}

/*----------------------------------------------------------------------------
 * geolocate
 *----------------------------------------------------------------------------*/
void PhoReal::geolocate (const Atl03DataFrame& df, uint32_t start_photon, uint32_t num_photons, result_t& result)
{
    if(parms->phoreal.geoloc == PhorealFields::CENTER)
    {
        /* Calculate Sums */
        double time_ns_min = DBL_MAX;
        double time_ns_max = -DBL_MAX;
        double latitude_min = DBL_MAX;
        double latitude_max = -DBL_MAX;
        double longitude_min = DBL_MAX;
        double longitude_max = -DBL_MAX;
        double x_atc_min = DBL_MAX;
        double x_atc_max = -DBL_MAX;
        double y_atc_min = DBL_MAX;
        double y_atc_max = -DBL_MAX;
        for(uint32_t p = 0; p < num_photons; p++)
        {
            const uint32_t i = start_photon + p;

            if(df.time_ns[i].nanoseconds < time_ns_min) time_ns_min = df.time_ns[i].nanoseconds;
            if(df.latitude[i]   < latitude_min)     latitude_min    = df.latitude[i];
            if(df.longitude[i]  < longitude_min)    longitude_min   = df.longitude[i];
            if(df.x_atc[i]      < x_atc_min)        x_atc_min       = df.x_atc[i];
            if(df.y_atc[i]      < y_atc_min)        y_atc_min       = df.y_atc[i];

            if(df.time_ns[i].nanoseconds > time_ns_max) time_ns_max = df.time_ns[i].nanoseconds;
            if(df.latitude[i]   > latitude_max)     latitude_max    = df.latitude[i];
            if(df.longitude[i]  > longitude_max)    longitude_max   = df.longitude[i];
            if(df.x_atc[i]      > x_atc_max)        x_atc_max       = df.x_atc[i];
            if(df.y_atc[i]      > y_atc_max)        y_atc_max       = df.y_atc[i];
        }

        /* Calculate Averages */
        result.time_ns = (int64_t)((time_ns_min + time_ns_max) / 2.0);
        result.latitude = (latitude_min + latitude_max) / 2.0;
        result.longitude = (longitude_min + longitude_max) / 2.0;
        result.x_atc = (x_atc_min + x_atc_max) / 2.0;
        result.y_atc = (y_atc_min + y_atc_max) / 2.0;
    }
    else if(parms->phoreal.geoloc == PhorealFields::MEAN)
    {
        /* Calculate Sums */
        double sum_time_ns = 0.0;
        double sum_latitude = 0.0;
        double sum_longitude = 0.0;
        double sum_x_atc = 0.0;
        double sum_y_atc = 0.0;
        for(uint32_t p = 0; p < num_photons; p++)
        {
            const uint32_t i = start_photon + p;

            sum_time_ns += df.time_ns[i].nanoseconds;
            sum_latitude += df.latitude[i];
            sum_longitude += df.longitude[i];
            sum_x_atc += df.x_atc[i];
            sum_y_atc += df.y_atc[i];
        }

        /* Calculate Averages */
        result.time_ns = (int64_t)(sum_time_ns / num_photons);
        result.latitude = sum_latitude / num_photons;
        result.longitude = sum_longitude / num_photons;
        result.x_atc = sum_x_atc / num_photons;
        result.y_atc = sum_y_atc / num_photons;
    }
    else if(parms->phoreal.geoloc == PhorealFields::MEDIAN)
    {
        const uint32_t center_ph = start_photon + (num_photons / 2);

        if(num_photons % 2 == 1) // Odd Number of Photons
        {
            result.time_ns = df.time_ns[center_ph];
            result.latitude = df.latitude[center_ph];
            result.longitude = df.longitude[center_ph];
            result.x_atc = df.x_atc[center_ph];
            result.y_atc = df.y_atc[center_ph];
        }
        else // Even Number of Photons
        {
            result.time_ns = (df.time_ns[center_ph].nanoseconds + df.time_ns[center_ph - 1].nanoseconds) / 2;
            result.latitude = (df.latitude[center_ph] + df.latitude[center_ph - 1]) / 2;
            result.longitude = (df.longitude[center_ph] + df.longitude[center_ph - 1]) / 2;
            result.x_atc = (df.x_atc[center_ph] + df.x_atc[center_ph - 1]) / 2;
            result.y_atc = (df.y_atc[center_ph] + df.y_atc[center_ph - 1]) / 2;
        }
    }
    else // unexpected geolocation setting
    {
        result.time_ns = 0;
        result.latitude = 0.0;
        result.longitude = 0.0;
        result.x_atc = 0.0;
        result.y_atc = 0.0;
    }
}

/*----------------------------------------------------------------------------
 * phorealAlgorithm
 *----------------------------------------------------------------------------*/
void PhoReal::algorithm (const Atl03DataFrame& df, uint32_t start_photon, uint32_t num_photons, result_t& result)
{
    /* Determine Number of Ground and Vegetation Photons */
    long gnd_cnt = 0;
    long veg_cnt = 0;
    for(uint32_t p = 0; p < num_photons; p++)
    {
        const uint32_t i = start_photon + p;

        if(isGround(df.atl08_class[i]))
        {
            gnd_cnt++;
        }
        else if(isVegetation(df.atl08_class[i]))
        {
            veg_cnt++;
        }
    }
    result.ground_photon_count = gnd_cnt;
    result.vegetation_photon_count = veg_cnt;

    /* Create Ground and Vegetation Photon Index Arrays */
    uint32_t* gnd_index = new uint32_t [gnd_cnt];
    uint32_t* veg_index = new uint32_t [veg_cnt];
    uint32_t g = 0;
    uint32_t v = 0;
    for(uint32_t p = 0; p < num_photons; p++)
    {
        const uint32_t i = start_photon + p;

        if(isGround(df.atl08_class[i]))
        {
            assert(g < gnd_cnt);
            gnd_index[g++] = i;
        }
        else if(isVegetation(df.atl08_class[i]))
        {
            assert(v < veg_cnt);
            veg_index[v++] = i;
        }
    }

    /* Sort Ground and Vegetation Photon Index Arrays */
    quicksort(gnd_index, df.height, 0, gnd_cnt - 1);
    quicksort(veg_index, df.relief, 0, veg_cnt - 1);

    /* Determine Min,Max,Avg Heights */
    double min_h = DBL_MAX;
    double max_h = -DBL_MAX;
    double sum_h = 0.0;
    if(veg_cnt > 0)
    {
        for(uint32_t p = 0; p < veg_cnt; p++)
        {
            sum_h += df.relief[veg_index[p]];
            if(df.relief[veg_index[p]] > max_h)
            {
                max_h = df.relief[veg_index[p]];
            }
            if(df.relief[veg_index[p]] < min_h)
            {
                min_h = df.relief[veg_index[p]];
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
    for(uint32_t p = 0; p < veg_cnt; p++)
    {
        const double delta = (df.relief[veg_index[p]] - result.h_mean_canopy);
        std_h += delta * delta;
    }
    result.canopy_openness = sqrt(std_h / (double)veg_cnt);

    /* Calculate Number of Bins */
    int num_bins = (int)ceil((max_h - min_h) / parms->phoreal.binsize);
    if(num_bins > MAX_BINS)
    {
        mlog(WARNING, "Maximum number of bins truncated from %d to maximum allowed of %d", num_bins, MAX_BINS);
        result.pflags |= Icesat2Fields::PFLAG_BIN_OVERFLOW;
        num_bins = MAX_BINS;
    }
    else if(num_bins <= 0)
    {
        result.pflags |= Icesat2Fields::PFLAG_BIN_UNDERFLOW;
        num_bins = 1;
    }

    /* Bin Photons */
    uint32_t* bins = new uint32_t[num_bins];
    memset(bins, 0, num_bins * sizeof(uint32_t));
    for(uint32_t p = 0; p < veg_cnt; p++)
    {
        int bin = (int)floor((df.relief[veg_index[p]] - min_h) / parms->phoreal.binsize);
        if(bin < 0) bin = 0;
        else if(bin >= num_bins) bin = num_bins - 1;
        bins[bin]++;
    }

    /* Generate Cumulative Bins */
    uint32_t* cbins = new uint32_t[num_bins];
    cbins[0] = bins[0];
    for(uint32_t b = 1; b < static_cast<uint32_t>(num_bins); b++)
    {
        cbins[b] = cbins[b - 1] + bins[b];
    }

    /* Find Median Terrain Height */
    float h_te_median = 0.0;
    if(gnd_cnt > 0)
    {
        if(gnd_cnt % 2 == 0) // even
        {
            const uint32_t p0 = (gnd_cnt - 1) / 2;
            const uint32_t p1 = ((gnd_cnt - 1) / 2) + 1;
            h_te_median = (df.height[gnd_index[p0]] + df.height[gnd_index[p1]]) / 2.0;
        }
        else // odd
        {
            const long p0 = (gnd_cnt - 1) / 2;
            h_te_median = df.height[gnd_index[p0]];
        }
    }
    result.h_te_median = h_te_median;

    /* Calculate Percentiles */
    if(veg_cnt > 0)
    {
        int b = 0; // bin index
        for(int p = 0; p < NUM_PERCENTILES; p++)
        {
            while(b < num_bins)
            {
                const double percentage = ((double)cbins[b] / (double)veg_cnt) * 100.0;
                if(percentage >= PercentileInterval[p] && cbins[b] > 0)
                {
                    result.canopy_h_metrics[p] = df.relief[veg_index[cbins[b] - 1]];
                    break;
                }
                b++;
            }
        }
        /* Find 98th Percentile */
        while(b < num_bins)
        {
            const double percentage = ((double)cbins[b] / (double)veg_cnt) * 100.0;
            if(percentage >= 98.0 && cbins[b] > 0)
            {
                result.h_canopy = df.relief[veg_index[cbins[b] - 1]];
                break;
            }
            b++;
        }
    }
    else // zero out results
    {
        result.h_canopy = 0.0;
        for(int p = 0; p < NUM_PERCENTILES; p++)
        {
            result.canopy_h_metrics[p] = 0.0;
        }
    }

    /* Clean Up */
    delete [] gnd_index;
    delete [] veg_index;
    delete [] bins;
    delete [] cbins;
}

/*----------------------------------------------------------------------------
 * quicksort
 *----------------------------------------------------------------------------*/
void PhoReal::quicksort (uint32_t* index_array, const FieldColumn<float>& column, int start, int end) // NOLINT(misc-no-recursion)
{
    if(start < end)
    {
        const int partition = quicksortpartition(index_array, column, start, end);
        quicksort(index_array, column, start, partition);
        quicksort(index_array, column, partition + 1, end);
    }
}

/*----------------------------------------------------------------------------
 * quicksortpartition
 *----------------------------------------------------------------------------*/
int PhoReal::quicksortpartition (uint32_t* index_array, const FieldColumn<float>& column, int start, int end)
{
    const double pivot = column[index_array[(start + end) / 2]];

    start--;
    end++;
    while(true)
    {
        while (column[index_array[++start]] < pivot); // NOLINT(clang-analyzer-core.uninitialized.ArraySubscript)
        while (column[index_array[--end]] > pivot);   // NOLINT(clang-analyzer-core.uninitialized.ArraySubscript)
        if (start >= end) return end;

        const long tmp = index_array[start];
        index_array[start] = index_array[end];
        index_array[end] = tmp;
    }
}
