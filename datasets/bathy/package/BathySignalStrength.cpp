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

#include <cmath>
#include <numeric>
#include <algorithm>

#include "OsApi.h"
#include "List.h"
#include "GeoLib.h"
#include "BathyFields.h"
#include "BathySignalStrength.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathySignalStrength::LUA_META_NAME = "BathySignalStrength";
const struct luaL_Reg BathySignalStrength::LUA_META_TABLE[] = {
    {NULL,  NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int BathySignalStrength::luaCreate (lua_State* L)
{
    BathyFields* _parms = NULL;

    try
    {
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 1, BathyFields::OBJECT_TYPE));
        return createLuaObject(L, new BathySignalStrength(L, _parms));
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
BathySignalStrength::BathySignalStrength (lua_State* L, BathyFields* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathySignalStrength::~BathySignalStrength (void)
{
    if(parms) parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run -
 *----------------------------------------------------------------------------*/
bool BathySignalStrength::run(GeoDataFrame* dataframe)
{
    const double start = TimeLib::latchtime();

    BathyDataFrame& df = *dynamic_cast<BathyDataFrame*>(dataframe);

    // initialize parameters
    const double max_shot_pe = 1.0; // maximum expected photon-electrons per shot
    const double histo_binsize = 0.2; // meters
    const double histo_width = 20.0; // parms->extentLength.value;
    const double histo_step = 10.0; // parms->extentStep.value;
    const int32_t histo_min_numbins = 10; // for checking and for background removal
    const bool use_background_rate = false; // selects how background is removed from histogram
    const double histo_start = parms->minGeoidDelta.value;
    const double histo_stop = parms->maxGeoidDelta.value;
    const double histo_binstep = histo_binsize / 2.0;
    const int32_t histo_numbins = static_cast<int32_t>(std::ceil((histo_stop - histo_start) / histo_binstep));
    const double histo_bintime = histo_binsize * 0.00000002 / 3.0; // binsize from meters to seconds

    // sanity check parameters
    if(histo_numbins <= histo_min_numbins)
    {
        mlog(CRITICAL, "Insufficient histogram size to generate signal statistics: %d", histo_numbins);
        return false;
    }

    // histogram processing
    long next_extent_start_i = 0;
    while(true)
    {
        // allocate and initialize histogram
        uint32_t* histogram = new uint32_t[histo_numbins];
        memset(histogram, 0, sizeof(uint32_t) * histo_numbins);

        // initialize accumulated background for calculating average below
        double background_acc = 0.0;

        // bin photons in extent
        const long start_i = next_extent_start_i;
        long i = start_i;
        while(i < df.length())
        {
            // check extent step
            if((df.x_atc[i] - df.x_atc[start_i]) <= histo_step)
            {
                next_extent_start_i = i + 1;
            }

            // check extent size
            if((df.x_atc[i] - df.x_atc[start_i]) > histo_width)
            {
                // check for corner-case at end of track
                // keep going with current histogram if the remaining portion of the track
                // is not long enough to support another full histogram width
                if((i < (df.length() - 1)) &&
                   ((df.x_atc[df.length() - 1] - df.x_atc[next_extent_start_i]) > histo_width))
                {
                    break;
                }
            }

            // bin the photon
            const long bin = static_cast<long>(std::floor((df.geoid_corr_h[i] - histo_start) / histo_binstep));
            if(bin >= 0 && bin <= histo_numbins) histogram[bin]++;
            else mlog(CRITICAL, "Invalid bin in histogram: %ld <== %.3f %.3f %.3f", bin, df.geoid_corr_h[i], histo_start,  histo_binstep);

            // accumulate background rate
            background_acc += df.background_rate[i];

            // go to next photon
            i++;
        }

        // check if valid histogram
        const long photons_in_histogram = i - start_i;
        if(photons_in_histogram > 0)
        {
            // initialize background photon-electrons
            double background_pe = 0.0;

            // determine number of shots
            const time8_t start_time = df.time_ns[start_i];
            const time8_t stop_time = df.time_ns[i - 1];
            const int32_t num_shots = ((stop_time.nanoseconds - start_time.nanoseconds) / 100000) + 1;

            if(use_background_rate)
            {
                // determine expected values for each bin
                const double background_avg = background_acc / photons_in_histogram;
                background_pe = background_avg * histo_bintime * num_shots;
            }
            else
            {
                // smooth histogram and created sorted values
                const long smoothed_numbins = histo_numbins - 1;
                List<uint32_t> sorted_smoothed_histogram(smoothed_numbins);
                for(long bin = 0; bin < smoothed_numbins; bin++)
                {
                    histogram[bin] = histogram[bin] + histogram[bin + 1];
                    sorted_smoothed_histogram.add(histogram[bin]);
                }

                // sum the lowest N bins
                sorted_smoothed_histogram.sort();
                uint32_t bin_acc = 0;
                for(long bin = 0; bin < smoothed_numbins - histo_min_numbins; bin++)
                {
                    bin_acc += sorted_smoothed_histogram[bin];
                }

                // calculate average background contribution to each bin
                background_pe = bin_acc / (smoothed_numbins - histo_min_numbins);
            }

            // calculate signal pe normalized to 255
            // across the range of 0 to max_shot_pe
            for(long bin = 0; bin < histo_numbins; bin++)
            {
                const double shot_pe = MAX(MIN((static_cast<double>(histogram[bin]) - background_pe) / num_shots, max_shot_pe), 0.0);
                histogram[bin] = (shot_pe / max_shot_pe) * 0xFF;
            }

            // traverse all photons in histogram and assign signal score
            for(long k = start_i; k < i; k++)
            {
                const long bin = static_cast<long>(std::floor((df.geoid_corr_h[k] - histo_start) / histo_binstep));
                if(bin >= 0 && bin <= histo_numbins)
                {
                    if(df.signal_score[k] == 0)
                    {
                        df.signal_score[k] = histogram[bin];
                    }
                    else
                    {
                        df.signal_score[k] = MAX(histogram[bin], df.signal_score[k]);
                    }
                }
                else
                {
                    mlog(CRITICAL, "Invalid bin in histogram: %ld <== %.3f %.3f %.3f", bin, df.geoid_corr_h[i], histo_start,  histo_binstep);
                }
            }
        }

        // clean up histogram memory
        delete [] histogram;

        // check for completion
        if(i >= df.length())
        {
            break;
        }
    }

    // mark completion
    updateRunTime(TimeLib::latchtime() - start);
    return true;
}
