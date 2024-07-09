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

#include "GeoLib.h"
#include "BathyOpenOceans.h"
#include "BathyFields.h"

using BathyFields::extent_t;
using BathyFields::photon_t;
using BathyFields::classifier_t;
using BathyFields::bathy_class_t;

/******************************************************************************
 * OPENOCEANS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * static data
 *----------------------------------------------------------------------------*/
const char* BathyOpenOceans::OPENOCEANS_PARMS = "openoceans";

/*----------------------------------------------------------------------------
 * parameters names
 *----------------------------------------------------------------------------*/
static const char*  OPENOCEANS_PARMS_RI_AIR                 = "ri_air";
static const char*  OPENOCEANS_PARMS_RI_WATER               = "ri_water";
static const char*  OPENOCEANS_PARMS_DEM_BUFFER             = "dem_buffer";
static const char*  OPENOCEANS_PARMS_BIN_SIZE               = "bin_size";
static const char*  OPENOCEANS_PARMS_MAX_RANGE              = "max_range";
static const char*  OPENOCEANS_PARMS_MAX_BINS               = "max_bins";
static const char*  OPENOCEANS_PARMS_SIGNAL_THRESHOLD       = "signal_threshold"; // sigmas
static const char*  OPENOCEANS_PARMS_MIN_PEAK_SEPARATION    = "min_peak_separation";
static const char*  OPENOCEANS_PARMS_HIGHEST_PEAK_RATIO     = "highest_peak_ratio";
static const char*  OPENOCEANS_PARMS_SURFACE_WIDTH          = "surface_width"; // sigmas
static const char*  OPENOCEANS_PARMS_MODEL_AS_POISSON       = "model_as_poisson"; // sigmas

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyOpenOceans::BathyOpenOceans (lua_State* L, int index)
{
    /* Get Algorithm Parameters */
    if(lua_istable(L, index))
    {
        /* refraction index of air */
        lua_getfield(L, index, OPENOCEANS_PARMS_RI_AIR);
        parms.ri_air = LuaObject::getLuaFloat(L, -1, true, parms.ri_air, NULL);
        lua_pop(L, 1);

        /* prefraction index of water */
        lua_getfield(L, index, OPENOCEANS_PARMS_RI_WATER);
        parms.ri_water = LuaObject::getLuaFloat(L, -1, true, parms.ri_water, NULL);
        lua_pop(L, 1);

        /* DEM buffer */
        lua_getfield(L, index, OPENOCEANS_PARMS_DEM_BUFFER);
        parms.dem_buffer = LuaObject::getLuaFloat(L, -1, true, parms.dem_buffer, NULL);
        lua_pop(L, 1);

        /* bin size */
        lua_getfield(L, index, OPENOCEANS_PARMS_BIN_SIZE);
        parms.bin_size = LuaObject::getLuaFloat(L, -1, true, parms.bin_size, NULL);
        lua_pop(L, 1);

        /* max range */
        lua_getfield(L, index, OPENOCEANS_PARMS_MAX_RANGE);
        parms.max_range = LuaObject::getLuaFloat(L, -1, true, parms.max_range, NULL);
        lua_pop(L, 1);

        /* max bins */
        lua_getfield(L, index, OPENOCEANS_PARMS_MAX_BINS);
        parms.max_bins = LuaObject::getLuaInteger(L, -1, true, parms.max_bins, NULL);
        lua_pop(L, 1);

        /* signal threshold */
        lua_getfield(L, index, OPENOCEANS_PARMS_SIGNAL_THRESHOLD);
        parms.signal_threshold = LuaObject::getLuaFloat(L, -1, true, parms.signal_threshold, NULL);
        lua_pop(L, 1);

        /* minimum peak separation */
        lua_getfield(L, index, OPENOCEANS_PARMS_MIN_PEAK_SEPARATION);
        parms.min_peak_separation = LuaObject::getLuaFloat(L, -1, true, parms.min_peak_separation, NULL);
        lua_pop(L, 1);

        /* highest peak ratio */
        lua_getfield(L, index, OPENOCEANS_PARMS_HIGHEST_PEAK_RATIO);
        parms.highest_peak_ratio = LuaObject::getLuaFloat(L, -1, true, parms.highest_peak_ratio, NULL);
        lua_pop(L, 1);

        /* surface width */
        lua_getfield(L, index, OPENOCEANS_PARMS_SURFACE_WIDTH);
        parms.surface_width = LuaObject::getLuaFloat(L, -1, true, parms.surface_width, NULL);
        lua_pop(L, 1);

        /* model as poisson */
        lua_getfield(L, index, OPENOCEANS_PARMS_MODEL_AS_POISSON);
        parms.model_as_poisson = LuaObject::getLuaBoolean(L, -1, true, parms.model_as_poisson, NULL);
        lua_pop(L, 1);        
    }
}

/*----------------------------------------------------------------------------
 * findSeaSurface
 *----------------------------------------------------------------------------*/
void BathyOpenOceans::findSeaSurface (extent_t& extent)
{
    /* initialize stats on photons */
    double min_h = std::numeric_limits<double>::max();
    double max_h = std::numeric_limits<double>::min();
    double min_t = std::numeric_limits<double>::max();
    double max_t = std::numeric_limits<double>::min();
    double avg_bckgnd = 0.0;

    /* build list photon heights */
    vector<double> heights;
    for(long i = 0; i < extent.photon_count; i++)
    {
        const double height = extent.photons[i].geoid_corr_h;
        const double time_secs = static_cast<double>(extent.photons[i].time_ns) / 1000000000.0;

        /* filter distance from DEM height
         *  TODO: does the DEM height need to be corrected by GEOID */
        if( (height > (extent.photons[i].dem_h + parms.dem_buffer)) ||
            (height < (extent.photons[i].dem_h - parms.dem_buffer)) )
            continue;

        /* get min and max height */
        if(height < min_h) min_h = height;
        if(height > max_h) max_h = height;

        /* get min and max time */
        if(time_secs < min_t) min_t = time_secs;
        if(time_secs > max_t) max_t = time_secs;

        /* accumulate background (divided out below) */
        avg_bckgnd = extent.photons[i].background_rate;

        /* add to list of photons to process */
        heights.push_back(height);
    }

    /* check if photons are left to process */
    if(heights.empty())
    {
        throw RunTimeException(DEBUG, RTE_INFO, "No valid photons when determining sea surface");
    }

    /* calculate and check range */
    const double range_h = max_h - min_h;
    if(range_h <= 0 || range_h > parms.max_range)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "Invalid range <%lf> when determining sea surface", range_h);
    }

    /* calculate and check number of bins in histogram
     *  - the number of bins is increased by 1 in case the ceiling and the floor
     *    of the max range is both the same number */
    const long num_bins = static_cast<long>(std::ceil(range_h / parms.bin_size)) + 1;
    if(num_bins <= 0 || num_bins > parms.max_bins)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "Invalid combination of range <%lf> and bin size <%lf> produced out of range histogram size <%ld>", range_h, parms.bin_size, num_bins);
    }

    /* calculate average background */
    avg_bckgnd /= heights.size();

    /* build histogram of photon heights */
    vector<long> histogram(num_bins);
    std::for_each (std::begin(heights), std::end(heights), [&](const double h) {
        const long bin = static_cast<long>(std::floor((h - min_h) / parms.bin_size));
        histogram[bin]++;
    });

    /* calculate mean and standard deviation of histogram */
    double bckgnd = 0.0;
    double stddev = 0.0;
    if(parms.model_as_poisson)
    {
        const long num_shots = std::round((max_t - min_t) / 0.0001);
        const double bin_t = parms.bin_size * 0.00000002 / 3.0; // bin size from meters to seconds
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
    const long k = (static_cast<long>(std::ceil(kernel_size / parms.bin_size)) & ~0x1) / 2;
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
    const long peak_separation_in_bins = static_cast<long>(std::ceil(parms.min_peak_separation / parms.bin_size));
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
        (second_peak * parms.highest_peak_ratio >= highest_peak) ) // second peak is close in size to highest peak
    {
        /* select peak that is highest in elevation */
        if(highest_peak_bin < second_peak_bin)
        {
            highest_peak = second_peak;
            highest_peak_bin = second_peak_bin;
        }
    }

    /* check if sea surface signal is significant */
    const double signal_threshold = bckgnd + (stddev * parms.signal_threshold);
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
    const double peak_stddev = (peak_width * parms.bin_size) / 2.35;

    /* calculate sea surface height and label sea surface photons */
    extent.surface_h = min_h + (highest_peak_bin * parms.bin_size) + (parms.bin_size / 2.0);
    const double min_surface_h = extent.surface_h - (peak_stddev * parms.surface_width);
    const double max_surface_h = extent.surface_h + (peak_stddev * parms.surface_width);
    for(long i = 0; i < extent.photon_count; i++)
    {
        if( extent.photons[i].geoid_corr_h >= min_surface_h &&
            extent.photons[i].geoid_corr_h <= max_surface_h )
        {
            extent.photons[i].class_ph = BathyFields::SEA_SURFACE;
        }
    }
}

/*----------------------------------------------------------------------------
 * photon_refraction - 
 * 
 * ICESat-2 refraction correction implemented as outlined in Parrish, et al. 
 * 2019 for correcting photon depth data. Reference elevations are to geoid datum 
 * to remove sea surface variations.
 * 
 * https://www.mdpi.com/2072-4292/11/14/1634
 *
 * ----------------------------------------------------------------------------
 * The code below was adapted from https://github.com/ICESat2-Bathymetry/Information.git
 * with the associated license replicated here:
 * ----------------------------------------------------------------------------
 * 
 * Copyright (c) 2022, Jonathan Markel/UT Austin.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, 
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation 
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its 
 * contributors may be used to endorse or promote products derived from this 
 * software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR '
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *----------------------------------------------------------------------------*/
void BathyOpenOceans::refractionCorrection(extent_t& extent)
{
    GeoLib::UTMTransform transform(extent.utm_zone, extent.region < 8);

    photon_t* photons = extent.photons;
    for(uint32_t i = 0; i < extent.photon_count; i++)
    {
        double depth = extent.surface_h - photons[i].geoid_corr_h;      // compute un-refraction-corrected depths
        if(depth > 0)
        {
            /* Calculate Refraction Corrections */
            double n1 = parms.ri_air;
            double n2 = parms.ri_water;
            double theta_1 = (M_PI / 2.0) - photons[i].ref_el;          // angle of incidence (without Earth curvature)
            double theta_2 = asin(n1 * sin(theta_1) / n2);              // angle of refraction
            double phi = theta_1 - theta_2;
            double s = depth / cos(theta_1);                            // uncorrected slant range to the uncorrected seabed photon location
            double r = s * n1 / n2;                                     // corrected slant range
            double p = sqrt((r*r) + (s*s) - (2*r*s*cos(theta_1 - theta_2)));
            double gamma = (M_PI / 2.0) - theta_1;
            double alpha = asin(r * sin(phi) / p);
            double beta = gamma - alpha;
            double dZ = p * sin(beta);                                  // vertical offset
            double dY = p * cos(beta);                                  // cross-track offset
            double dE = dY * sin(photons[i].ref_az);                    // UTM offsets
            double dN = dY * cos(photons[i].ref_az); 

            /* Apply Refraction Corrections */
            photons[i].x_ph += dE;
            photons[i].y_ph += dN;
            photons[i].geoid_corr_h += dZ;
            depth -= dZ;

            /* Correct Latitude and Longitude */
            GeoLib::point_t point = transform.calculateCoordinates(photons[i].x_ph, photons[i].y_ph);
            photons[i].latitude = point.y;
            photons[i].longitude = point.x;
        }
    }
}
