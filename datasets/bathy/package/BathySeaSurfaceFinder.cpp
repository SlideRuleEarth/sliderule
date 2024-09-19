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
#include <limits>
#include <algorithm>

#include "OsApi.h"
#include "BathyFields.h"
#include "BathySeaSurfaceFinder.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathySeaSurfaceFinder::LUA_META_NAME = "BathySeaSurfaceFinder";
const struct luaL_Reg BathySeaSurfaceFinder::LUA_META_TABLE[] = {
    {NULL,  NULL}
};

/*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int BathySeaSurfaceFinder::luaCreate (lua_State* L)
{
    BathyFields* _parms = NULL;
    try
    {
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 1, BathyFields::OBJECT_TYPE));
        return createLuaObject(L, new BathySeaSurfaceFinder(L, _parms));
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
BathySeaSurfaceFinder::BathySeaSurfaceFinder (lua_State* L, BathyFields* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathySeaSurfaceFinder::~BathySeaSurfaceFinder (void)
{
    if(parms) parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool BathySeaSurfaceFinder::run(GeoDataFrame* dataframe)
{
    BathyDataFrame& df = *dynamic_cast<BathyDataFrame*>(dataframe);
    const SurfaceFields& surface_parms = parms->surface.value;

    /* for each extent (p0 = start photon) */
    for(long p0 = 0; p0 < df.length(); p0 += parms->phInExtent.value)
    {
        /* calculate last photon in extent */
        const long p1 = MIN(df.length(), p0 + parms->phInExtent.value);

        try
        {
            /* initialize stats on photons */
            double min_h = std::numeric_limits<double>::max();
            double max_h = std::numeric_limits<double>::min();
            double min_t = std::numeric_limits<double>::max();
            double max_t = std::numeric_limits<double>::min();
            double avg_bckgnd = 0.0;

            /* build list photon heights */
            vector<double> heights;
            for(long i = p0; i < p1; i++)
            {
                const double height = df.ortho_h[i];
                const double time_secs = static_cast<double>(df.time_ns[i].nanoseconds) / 1000000000.0;

                /* get min and max height */
                if(height < min_h) min_h = height;
                if(height > max_h) max_h = height;

                /* get min and max time */
                if(time_secs < min_t) min_t = time_secs;
                if(time_secs > max_t) max_t = time_secs;

                /* accumulate background (divided out below) */
                avg_bckgnd = df.background_rate[i];

                /* add to list of photons to process */
                heights.push_back(height);
            }

            /* check if photons are left to process */
            if(heights.empty())
            {
                throw RunTimeException(WARNING, RTE_INFO, "No valid photons when determining sea surface");
            }

            /* calculate and check range */
            const double range_h = max_h - min_h;
            if(range_h <= 0 || range_h > surface_parms.maxRange.value)
            {
                throw RunTimeException(ERROR, RTE_ERROR, "Invalid range <%lf> when determining sea surface", range_h);
            }

            /* calculate and check number of bins in histogram
            *  - the number of bins is increased by 1 in case the ceiling and the floor
            *    of the max range is both the same number */
            const long num_bins = static_cast<long>(std::ceil(range_h / surface_parms.binSize.value)) + 1;
            if(num_bins <= 0 || num_bins > surface_parms.maxBins.value)
            {
                throw RunTimeException(ERROR, RTE_ERROR, "Invalid combination of range <%lf> and bin size <%lf> produced out of range histogram size <%ld>", range_h, surface_parms.binSize.value, num_bins);
            }

            /* calculate average background */
            avg_bckgnd /= heights.size();

            /* build histogram of photon heights */
            vector<long> histogram(num_bins);
            std::for_each (std::begin(heights), std::end(heights), [&](const double h) {
                const long bin = static_cast<long>(std::floor((h - min_h) / surface_parms.binSize.value));
                histogram[bin]++;
            });

            /* calculate mean and standard deviation of histogram */
            double bckgnd = 0.0;
            double stddev = 0.0;
            if(surface_parms.modelAsPoisson.value)
            {
                const long num_shots = std::round((max_t - min_t) / 0.0001);
                const double bin_t = surface_parms.binSize.value * 0.00000002 / 3.0; // bin size from meters to seconds
                const double bin_pe = bin_t * num_shots * avg_bckgnd; // expected value
                bckgnd = bin_pe;
                stddev = sqrt(bin_pe);
            }
            else
            {
                const double bin_avg = static_cast<double>(heights.size()) / static_cast<double>(num_bins);
                double accum = 0.0;
                std::for_each (std::begin(histogram), std::end(histogram), [&](const double h) {
                    accum += (h - bin_avg) * (h - bin_avg);
                });
                bckgnd = bin_avg;
                stddev = sqrt(accum / heights.size());
            }

            /* build guassian kernel (from -k to k)*/
            const double kernel_size = 6.0 * stddev + 1.0;
            const long k = (static_cast<long>(std::ceil(kernel_size / surface_parms.binSize.value)) & ~0x1) / 2;
            const long kernel_bins = 2 * k + 1;
            double kernel_sum = 0.0;
            vector<double> kernel(kernel_bins);
            for(long x = -k; x <= k; x++)
            {
                const long i = x + k;
                const double r = x / stddev;
                kernel[i] = exp(-0.5 * r * r);
                kernel_sum += kernel[i];
            }
            for(int i = 0; i < kernel_bins; i++)
            {
                kernel[i] /= kernel_sum;
            }

            /* build filtered histogram */
            vector<double> smoothed_histogram(num_bins);
            for(long i = 0; i < num_bins; i++)
            {
                double output = 0.0;
                long num_samples = 0;
                for(long j = -k; j <= k; j++)
                {
                    const long index = i + k;
                    if(index >= 0 && index < num_bins)
                    {
                        output += kernel[j + k] * static_cast<double>(histogram[index]);
                        num_samples++;
                    }
                }
                smoothed_histogram[i] = output * static_cast<double>(kernel_bins) / static_cast<double>(num_samples);
            }

            /* find highest peak */
            long highest_peak_bin = 0;
            double highest_peak = smoothed_histogram[0];
            for(int i = 1; i < num_bins; i++)
            {
                if(smoothed_histogram[i] > highest_peak)
                {
                    highest_peak = smoothed_histogram[i];
                    highest_peak_bin = i;
                }
            }

            /* find second highest peak */
            const long peak_separation_in_bins = static_cast<long>(std::ceil(surface_parms.minPeakSeparation.value / surface_parms.binSize.value));
            long second_peak_bin = -1; // invalid
            double second_peak = std::numeric_limits<double>::min();
            for(int i = 0; i < num_bins; i++)
            {
                if(std::abs(i - highest_peak_bin) > peak_separation_in_bins)
                {
                    if(smoothed_histogram[i] > second_peak)
                    {
                        second_peak = smoothed_histogram[i];
                        second_peak_bin = i;
                    }
                }
            }

            /* determine which peak is sea surface */
            if( (second_peak_bin != -1) &&
                (second_peak * surface_parms.highestPeakRatio.value >= highest_peak) ) // second peak is close in size to highest peak
            {
                /* select peak that is highest in elevation */
                if(highest_peak_bin < second_peak_bin)
                {
                    highest_peak = second_peak;
                    highest_peak_bin = second_peak_bin;
                }
            }

            /* check if sea surface signal is significant */
            const double signal_threshold = bckgnd + (stddev * surface_parms.signalThreshold.value);
            if(highest_peak < signal_threshold)
            {
                throw RunTimeException(WARNING, RTE_INFO, "Unable to determine sea surface (%lf < %lf)", highest_peak, signal_threshold);
            }

            /* calculate width of highest peak */
            const double peak_above_bckgnd = smoothed_histogram[highest_peak_bin] - bckgnd;
            const double peak_half_max = (peak_above_bckgnd * 0.4) + bckgnd;
            long peak_width = 1;
            for(long i = highest_peak_bin + 1; i < num_bins; i++)
            {
                if(smoothed_histogram[i] > peak_half_max) peak_width++;
                else break;
            }
            for(long i = highest_peak_bin - 1; i >= 0; i--)
            {
                if(smoothed_histogram[i] > peak_half_max) peak_width++;
                else break;
            }
            const double peak_stddev = (peak_width * surface_parms.binSize.value) / 2.35;

            /* calculate sea surface height and label sea surface photons */
            const float cur_surface_h = min_h + (highest_peak_bin * surface_parms.binSize.value) + (surface_parms.binSize.value / 2.0);
            const double min_surface_h = cur_surface_h - (peak_stddev * surface_parms.surfaceWidth.value);
            const double max_surface_h = cur_surface_h + (peak_stddev * surface_parms.surfaceWidth.value);
            for(long i = p0; i < p1; i++)
            {
                df.surface_h[i] = cur_surface_h;
                if( df.ortho_h[i] >= min_surface_h &&
                    df.ortho_h[i] <= max_surface_h )
                {
                    df.class_ph[i] = BathyFields::SEA_SURFACE;
                }
            }
        }
        catch(const RunTimeException& e)
        {
            mlog(e.level(), "Failed to find sea surface for beam %s at photon %ld: %s", df.beam.value.c_str(), p0, e.what());
            for(long i = p0; i < p1; i++)
            {
                df.processing_flags[i] = df.processing_flags[i] | BathyFields::SEA_SURFACE_UNDETECTED;
            }
        }
    }

    /* Mark Completion */
    return true;
}
