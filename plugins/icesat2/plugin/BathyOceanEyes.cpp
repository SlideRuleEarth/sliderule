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
#include "GeoLib.h"
#include "BathyOceanEyes.h"
#include "BathyFields.h"

using BathyFields::extent_t;
using BathyFields::photon_t;
using BathyFields::classifier_t;
using BathyFields::bathy_class_t;

/******************************************************************************
 * OPENOCEANS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * parameters names
 *----------------------------------------------------------------------------*/
static const char*  OCEANEYES_PARMS_ASSET_KD               = "asset_kd";
static const char*  OCEANEYES_PARMS_DEFAULT_ASSETKD        = "viirsj1-s3";
static const char*  OCEANEYES_PARMS_RESOURCE_KD            = "resource_kd";
static const char*  OCEANEYES_PARMS_RI_AIR                 = "ri_air";
static const char*  OCEANEYES_PARMS_RI_WATER               = "ri_water";
static const char*  OCEANEYES_PARMS_DEM_BUFFER             = "dem_buffer";
static const char*  OCEANEYES_PARMS_BIN_SIZE               = "bin_size";
static const char*  OCEANEYES_PARMS_MAX_RANGE              = "max_range";
static const char*  OCEANEYES_PARMS_MAX_BINS               = "max_bins";
static const char*  OCEANEYES_PARMS_SIGNAL_THRESHOLD       = "signal_threshold"; // sigmas
static const char*  OCEANEYES_PARMS_MIN_PEAK_SEPARATION    = "min_peak_separation";
static const char*  OCEANEYES_PARMS_HIGHEST_PEAK_RATIO     = "highest_peak_ratio";
static const char*  OCEANEYES_PARMS_SURFACE_WIDTH          = "surface_width"; // sigmas
static const char*  OCEANEYES_PARMS_MODEL_AS_POISSON       = "model_as_poisson"; // sigmas

/*----------------------------------------------------------------------------
 * static data
 *----------------------------------------------------------------------------*/
const char* BathyOceanEyes::OCEANEYES_PARMS = "oceaneyes";

const char* BathyOceanEyes::TU_FILENAMES[NUM_UNCERTAINTY_DIMENSIONS][NUM_POINTING_ANGLES] = {
   {"/data/ICESat2_0deg_500000_AGL_0.022_mrad_THU.csv",
    "/data/ICESat2_1deg_500000_AGL_0.022_mrad_THU.csv",
    "/data/ICESat2_2deg_500000_AGL_0.022_mrad_THU.csv",
    "/data/ICESat2_3deg_500000_AGL_0.022_mrad_THU.csv",
    "/data/ICESat2_4deg_500000_AGL_0.022_mrad_THU.csv",
    "/data/ICESat2_5deg_500000_AGL_0.022_mrad_THU.csv"},
   {"/data/ICESat2_0deg_500000_AGL_0.022_mrad_TVU.csv",
    "/data/ICESat2_1deg_500000_AGL_0.022_mrad_TVU.csv",
    "/data/ICESat2_2deg_500000_AGL_0.022_mrad_TVU.csv",
    "/data/ICESat2_3deg_500000_AGL_0.022_mrad_TVU.csv",
    "/data/ICESat2_4deg_500000_AGL_0.022_mrad_TVU.csv",
    "/data/ICESat2_5deg_500000_AGL_0.022_mrad_TVU.csv"}
};

const int BathyOceanEyes::POINTING_ANGLES[NUM_POINTING_ANGLES] = {0, 1, 2, 3, 4, 5};

const int BathyOceanEyes::WIND_SPEEDS[NUM_WIND_SPEEDS] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

const double BathyOceanEyes::KD_RANGES[NUM_KD_RANGES][2] = {
//       0             1             2             3            4
//     clear     clear-moderate   moderate    moderate-high    high
    {0.06, 0.10}, {0.11, 0.17}, {0.18, 0.25}, {0.26, 0.32}, {0.33, 0.36}
};

BathyOceanEyes::uncertainty_coeff_t BathyOceanEyes::UNCERTAINTY_COEFF_MAP[NUM_UNCERTAINTY_DIMENSIONS][NUM_POINTING_ANGLES][NUM_WIND_SPEEDS][NUM_KD_RANGES];

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void BathyOceanEyes::init (void)
{
    /*************************/
    /* UNCERTAINTY_COEFF_MAP */
    /*************************/

    /* for each dimension */
    for(int tu_dimension_index = 0; tu_dimension_index < NUM_UNCERTAINTY_DIMENSIONS; tu_dimension_index++)
    {
        /* for each pointing angle */
        for(int pointing_angle_index = 0; pointing_angle_index < NUM_POINTING_ANGLES; pointing_angle_index++)
        {
            /* get uncertainty filename */
            const char* uncertainty_filename = TU_FILENAMES[tu_dimension_index][pointing_angle_index];

            /* open csv file */
            fileptr_t file = fopen(uncertainty_filename, "r");
            if(!file)
            {
                char err_buf[256];
                mlog(CRITICAL, "Failed to open file %s with error: %s", uncertainty_filename, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
                return;
            }

            /* read header line */
            char columns[5][10];
            if(fscanf(file, "%9s,%9s,%9s,%9s,%9s\n", columns[0], columns[1], columns[2], columns[3], columns[4]) != 5)
            {
                mlog(CRITICAL, "Failed to read header from uncertainty file: %s", uncertainty_filename);
                return;
            }

            /* read all rows */
            vector<uncertainty_entry_t> tu(INITIAL_UNCERTAINTY_ROWS);
            uncertainty_entry_t entry;
            while(fscanf(file, "%d,%lf,%lf,%lf,%lf\n", &entry.Wind, &entry.Kd, &entry.a, &entry.b, &entry.c) == 5)
            {
                tu.push_back(entry);
            }

            /* close file */
            fclose(file);

            /* for each wind speed */
            for(int wind_speed_index = 0; wind_speed_index < NUM_WIND_SPEEDS; wind_speed_index++)
            {
                /* for each kd range */
                for(int kd_range_index = 0; kd_range_index < NUM_KD_RANGES; kd_range_index++)
                {
                    /* sum coefficients for each entry in table */
                    uncertainty_coeff_t coeff_sum = {
                        .a = 0.0,
                        .b = 0.0,
                        .c = 0.0
                    };
                    double count = 0;
                    for(size_t i = 0; i < tu.size(); i++)
                    {
                        if( (tu[i].Wind == WIND_SPEEDS[wind_speed_index]) &&
                            (tu[i].Kd >= KD_RANGES[kd_range_index][0]) &&
                            (tu[i].Kd <= KD_RANGES[kd_range_index][1]) )
                        {
                            coeff_sum.a += tu[i].a;
                            coeff_sum.b += tu[i].b;
                            coeff_sum.c += tu[i].c;
                            count += 1.0;
                        }
                    }

                    /* check count */
                    if(count <= 0)
                    {
                        mlog(CRITICAL, "Failed to average coefficients from uncertainty file: %s", uncertainty_filename);
                        return;
                    }

                    /* set average coefficients */
                    uncertainty_coeff_t& coeff = UNCERTAINTY_COEFF_MAP[tu_dimension_index][pointing_angle_index][wind_speed_index][kd_range_index];
                    coeff.a = coeff_sum.a / count;
                    coeff.b = coeff_sum.b / count;
                    coeff.c = coeff_sum.c / count;
                }
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyOceanEyes::BathyOceanEyes (lua_State* L, int index):
    context(NULL),
    Kd_490(NULL)
{
    /* Get Algorithm Parameters */
    if(lua_istable(L, index))
    {
        /* assetKd */
        lua_getfield(L, index, OCEANEYES_PARMS_ASSET_KD);
        const char* assetkd_name = LuaObject::getLuaString(L, -1, true, OCEANEYES_PARMS_DEFAULT_ASSETKD);
        parms.assetKd = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(assetkd_name, Asset::OBJECT_TYPE));
        if(!parms.assetKd) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to find asset %s", assetkd_name);
        lua_pop(L, 1);

        /* resource Kd */
        lua_getfield(L, index, OCEANEYES_PARMS_RESOURCE_KD);
        parms.resourceKd = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, parms.resourceKd, NULL));
        lua_pop(L, 1);

        /* refraction index of air */
        lua_getfield(L, index, OCEANEYES_PARMS_RI_AIR);
        parms.ri_air = LuaObject::getLuaFloat(L, -1, true, parms.ri_air, NULL);
        lua_pop(L, 1);

        /* prefraction index of water */
        lua_getfield(L, index, OCEANEYES_PARMS_RI_WATER);
        parms.ri_water = LuaObject::getLuaFloat(L, -1, true, parms.ri_water, NULL);
        lua_pop(L, 1);

        /* DEM buffer */
        lua_getfield(L, index, OCEANEYES_PARMS_DEM_BUFFER);
        parms.dem_buffer = LuaObject::getLuaFloat(L, -1, true, parms.dem_buffer, NULL);
        lua_pop(L, 1);

        /* bin size */
        lua_getfield(L, index, OCEANEYES_PARMS_BIN_SIZE);
        parms.bin_size = LuaObject::getLuaFloat(L, -1, true, parms.bin_size, NULL);
        lua_pop(L, 1);

        /* max range */
        lua_getfield(L, index, OCEANEYES_PARMS_MAX_RANGE);
        parms.max_range = LuaObject::getLuaFloat(L, -1, true, parms.max_range, NULL);
        lua_pop(L, 1);

        /* max bins */
        lua_getfield(L, index, OCEANEYES_PARMS_MAX_BINS);
        parms.max_bins = LuaObject::getLuaInteger(L, -1, true, parms.max_bins, NULL);
        lua_pop(L, 1);

        /* signal threshold */
        lua_getfield(L, index, OCEANEYES_PARMS_SIGNAL_THRESHOLD);
        parms.signal_threshold = LuaObject::getLuaFloat(L, -1, true, parms.signal_threshold, NULL);
        lua_pop(L, 1);

        /* minimum peak separation */
        lua_getfield(L, index, OCEANEYES_PARMS_MIN_PEAK_SEPARATION);
        parms.min_peak_separation = LuaObject::getLuaFloat(L, -1, true, parms.min_peak_separation, NULL);
        lua_pop(L, 1);

        /* highest peak ratio */
        lua_getfield(L, index, OCEANEYES_PARMS_HIGHEST_PEAK_RATIO);
        parms.highest_peak_ratio = LuaObject::getLuaFloat(L, -1, true, parms.highest_peak_ratio, NULL);
        lua_pop(L, 1);

        /* surface width */
        lua_getfield(L, index, OCEANEYES_PARMS_SURFACE_WIDTH);
        parms.surface_width = LuaObject::getLuaFloat(L, -1, true, parms.surface_width, NULL);
        lua_pop(L, 1);

        /* model as poisson */
        lua_getfield(L, index, OCEANEYES_PARMS_MODEL_AS_POISSON);
        parms.model_as_poisson = LuaObject::getLuaBoolean(L, -1, true, parms.model_as_poisson, NULL);
        lua_pop(L, 1);
    }

    /* Open Kd Resource */
    if(!parms.assetKd) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to open Kd resource, no asset provided");
    else if(!parms.resourceKd) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to open Kd resource, no filename provided");
    try
    {
        context = new H5Coro::Context(parms.assetKd, parms.resourceKd);
        Kd_490 = new H5Array<int16_t>(context, "Kd_490", H5Coro::ALL_COLS, 0, H5Coro::ALL_ROWS);
    }
    catch (const RunTimeException& e)
    {
        delete context;
        delete Kd_490;
        throw;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyOceanEyes::~BathyOceanEyes (void)
{
    delete Kd_490;
    delete context;
}

/*----------------------------------------------------------------------------
 * findSeaSurface
 *----------------------------------------------------------------------------*/
void BathyOceanEyes::findSeaSurface (extent_t& extent) const
{
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
        for(long i = 0; i < extent.photon_count; i++)
        {
            const double height = extent.photons[i].ortho_h;
            const double time_secs = static_cast<double>(extent.photons[i].time_ns) / 1000000000.0;

            /* filter distance from DEM height */
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
            throw RunTimeException(WARNING, RTE_INFO, "No valid photons when determining sea surface");
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
            if( extent.photons[i].ortho_h >= min_surface_h &&
                extent.photons[i].ortho_h <= max_surface_h )
            {
                extent.photons[i].class_ph = BathyFields::SEA_SURFACE;
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failed to find sea surface for spot %d [extend_id=0x%16lX]: %s", extent.spot, extent.extent_id, e.what());
        for(long i = 0; i < extent.photon_count; i++)
        {
            extent.photons[i].processing_flags |= BathyFields::SEA_SURFACE_UNDETECTED;
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
void BathyOceanEyes::correctRefraction(extent_t& extent) const
{
    GeoLib::UTMTransform transform(extent.utm_zone, extent.region < 8);

    photon_t* photons = extent.photons;
    for(uint32_t i = 0; i < extent.photon_count; i++)
    {
        const double depth = extent.surface_h - photons[i].ortho_h;      // compute un-refraction-corrected depths
        if(depth > 0)
        {
            /* Calculate Refraction Corrections */
            const double n1 = parms.ri_air;
            const double n2 = parms.ri_water;
            const double theta_1 = (M_PI / 2.0) - photons[i].ref_el;    // angle of incidence (without Earth curvature)
            const double theta_2 = asin(n1 * sin(theta_1) / n2);        // angle of refraction
            const double phi = theta_1 - theta_2;
            const double s = depth / cos(theta_1);                      // uncorrected slant range to the uncorrected seabed photon location
            const double r = s * n1 / n2;                               // corrected slant range
            const double p = sqrt((r*r) + (s*s) - (2*r*s*cos(theta_1 - theta_2)));
            const double gamma = (M_PI / 2.0) - theta_1;
            const double alpha = asin(r * sin(phi) / p);
            const double beta = gamma - alpha;
            const double dZ = p * sin(beta);                            // vertical offset
            const double dY = p * cos(beta);                            // cross-track offset
            const double dE = dY * sin(static_cast<double>(photons[i].ref_az));              // UTM offsets
            const double dN = dY * cos(static_cast<double>(photons[i].ref_az));

            /* Apply Refraction Corrections */
            photons[i].x_ph += dE;
            photons[i].y_ph += dN;
            photons[i].ortho_h += dZ;

            /* Correct Latitude and Longitude */
            const GeoLib::point_t point = transform.calculateCoordinates(photons[i].x_ph, photons[i].y_ph);
            photons[i].latitude = point.y;
            photons[i].longitude = point.x;
        }
    }
}

/*----------------------------------------------------------------------------
 * uncertainty calculation
 *----------------------------------------------------------------------------*/
void BathyOceanEyes::calculateUncertainty (extent_t& extent) const
{
    if(extent.photon_count == 0) return; // nothing to do

    /* join kd resource read */
    Kd_490->join(parms.read_timeout_ms, true);

    /* get y offset */
    const double degrees_of_latitude = extent.photons[0].latitude + 90.0;
    const double latitude_pixels = degrees_of_latitude / 24.0;
    const int32_t y = static_cast<int32_t>(latitude_pixels);

    /* get x offset */
    const double degrees_of_longitude =  extent.photons[0].longitude + 180.0;
    const double longitude_pixels = degrees_of_longitude / 24.0;
    const int32_t x = static_cast<int32_t>(longitude_pixels);

    /* calculate total offset */
    if(x < 0 || x >= 4320 || y < 0 || y >= 8640)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid Kd coordinates: %d, %d", x, y);
    }
    const long offset = (x * 4320) + y;
    const double kd = static_cast<double>((*Kd_490)[offset]) * 0.0002;

    /* for each photon in extent */
    photon_t* photons = extent.photons;
    for(uint32_t i = 0; i < extent.photon_count; i++)
    {
        /* initialize total uncertainty to aerial uncertainty */
        photons[i].sigma_thu = sqrtf((photons[i].sigma_across * photons[i].sigma_across) + (photons[i].sigma_along * photons[i].sigma_along));
        photons[i].sigma_tvu = photons[i].sigma_h;

        /* calculate subaqueous uncertainty */
        const double depth = extent.surface_h - photons[i].ortho_h;
        if(depth > 0.0)
        {
            /* get pointing angle index */
            int pointing_angle_index = static_cast<int>(roundf(photons[i].pointing_angle));
            if(pointing_angle_index < 0) pointing_angle_index = 0;
            else if(pointing_angle_index >= NUM_POINTING_ANGLES) pointing_angle_index = NUM_POINTING_ANGLES - 1;

            /* get wind speed index */
            int wind_speed_index = static_cast<int>(roundf(photons[i].wind_v)) - 1;
            if(wind_speed_index < 0) wind_speed_index = 0;
            else if(wind_speed_index >= NUM_WIND_SPEEDS) wind_speed_index = NUM_WIND_SPEEDS - 1;

            /* get kd range index */
            int kd_range_index = 0;
            while(kd_range_index < (NUM_KD_RANGES - 1) && KD_RANGES[kd_range_index][1] < kd)
            {
                kd_range_index++;
            }

            /* uncertainty coefficients */
            const uncertainty_coeff_t horizontal_coeff = UNCERTAINTY_COEFF_MAP[THU][pointing_angle_index][wind_speed_index][kd_range_index];
            const uncertainty_coeff_t vertical_coeff = UNCERTAINTY_COEFF_MAP[TVU][pointing_angle_index][wind_speed_index][kd_range_index];

            /* subaqueous uncertainties */
            const double subaqueous_horizontal_uncertainty = (horizontal_coeff.a * depth * depth) + (horizontal_coeff.b * depth) + horizontal_coeff.c;
            const double subaqueous_vertical_uncertainty = (vertical_coeff.a * depth * depth) + (vertical_coeff.b * depth) + vertical_coeff.c;

            /* add subaqueous uncertainties to total uncertainties */
            photons[i].sigma_thu += subaqueous_horizontal_uncertainty;
            photons[i].sigma_tvu += subaqueous_vertical_uncertainty;

            /* set maximum sensor depth processing flag */
            if(kd > 0)
            {
                const double max_sensor_depth = 1.8 / kd;
                if(depth > max_sensor_depth)
                {
                    photons[i].processing_flags |= BathyFields::SENSOR_DEPTH_EXCEEDED;
                }
            }
        }
    }
}
