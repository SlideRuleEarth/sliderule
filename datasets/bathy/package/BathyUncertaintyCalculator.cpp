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
#include "BathyUncertaintyCalculator.h"
#include "BathyFields.h"

using BathyFields::extent_t;
using BathyFields::photon_t;
using BathyFields::classifier_t;
using BathyFields::bathy_class_t;

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* BathyUncertaintyCalculator::OBJECT_TYPE = "BathyUncertaintyCalculator";
const char* BathyUncertaintyCalculator::LUA_META_NAME = "BathyUncertaintyCalculator";
const struct luaL_Reg BathyUncertaintyCalculator::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

const char* BathyUncertaintyCalculator::TU_FILENAMES[NUM_UNCERTAINTY_DIMENSIONS][NUM_POINTING_ANGLES] = {
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

const int BathyUncertaintyCalculator::POINTING_ANGLES[NUM_POINTING_ANGLES] = {0, 1, 2, 3, 4, 5};

const int BathyUncertaintyCalculator::WIND_SPEEDS[NUM_WIND_SPEEDS] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

const double BathyUncertaintyCalculator::KD_RANGES[NUM_KD_RANGES][2] = {
//       0             1             2             3            4
//     clear     clear-moderate   moderate    moderate-high    high
    {0.06, 0.10}, {0.11, 0.17}, {0.18, 0.25}, {0.26, 0.32}, {0.33, 0.36}
};

BathyUncertaintyCalculator::uncertainty_coeff_t BathyUncertaintyCalculator::UNCERTAINTY_COEFF_MAP[NUM_UNCERTAINTY_DIMENSIONS][NUM_POINTING_ANGLES][NUM_WIND_SPEEDS][NUM_KD_RANGES];

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void BathyUncertaintyCalculator::init (void)
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
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int BathyUncertaintyCalculator::luaCreate (lua_State* L)
{
    BathyParms* parms = NULL;

    try
    {
        parms = dynamic_cast<BathyParms*>(getLuaObject(L, 1, BathyParms::OBJECT_TYPE));
        if(!parms->uncertainty.assetKd) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to open Kd resource, no asset provided");
        else if(!parms->uncertainty.resourceKd) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to open Kd resource, no filename provided");
        return createLuaObject(L, new BathyUncertaintyCalculator(L, parms));
    }
    catch(const RunTimeException& e)
    {
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyUncertaintyCalculator::BathyUncertaintyCalculator (lua_State* L, BathyParms* _parms):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    context(NULL),
    Kd_490(NULL)
{
    try
    {
        context = new H5Coro::Context(parms->uncertainty.assetKd, parms->uncertainty.resourceKd);
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
BathyUncertaintyCalculator::~BathyUncertaintyCalculator (void)
{
    delete Kd_490;
    delete context;
}

/*----------------------------------------------------------------------------
 * uncertainty calculation
 *----------------------------------------------------------------------------*/
void BathyUncertaintyCalculator::run (extent_t& extent,
                                      const H5Array<float>& sigma_across,
                                      const H5Array<float>& sigma_along,
                                      const H5Array<float>& sigma_h,
                                      const H5Array<float>& ref_el) const
{
    if(extent.photon_count == 0) return; // nothing to do

    /* join kd resource read */
    Kd_490->join(parms->read_timeout * 1000, true);

    /* get y offset */
    const double degrees_of_latitude = extent.photons[0].lat_ph + 90.0;
    const double latitude_pixels = degrees_of_latitude / 24.0;
    const int32_t y = static_cast<int32_t>(latitude_pixels);

    /* get x offset */
    const double degrees_of_longitude =  extent.photons[0].lat_ph + 180.0;
    const double longitude_pixels = degrees_of_longitude / 24.0;
    const int32_t x = static_cast<int32_t>(longitude_pixels);

    /* calculate total offset */
    if(x < 0 || x >= 4320 || y < 0 || y >= 8640)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid Kd coordinates: %d, %d", x, y);
    }
    const long offset = (x * 4320) + y;
    const double kd = static_cast<double>((*Kd_490)[offset]) * 0.0002;

    /* segment level variables */
    int32_t previous_segment = -1;
    float pointing_angle = 0.0;

    /* for each photon in extent */
    photon_t* photons = extent.photons;
    for(uint32_t i = 0; i < extent.photon_count; i++)
    {
        const int32_t s = extent.photons[i].index_seg;

        /* calculate pointing angle */
        if(previous_segment != s)
        {
            previous_segment = s;
            pointing_angle = 90.0 - ((180.0 / M_PI) * ref_el[s]);
        }

        /* initialize total uncertainty to aerial uncertainty */
        photons[i].sigma_thu = sqrtf((sigma_across[s] * sigma_across[s]) + (sigma_along[s] * sigma_along[s]));
        photons[i].sigma_tvu = sigma_h[s];
        
        /* calculate subaqueous uncertainty */
        const double depth = photons[i].surface_h - photons[i].ortho_h;
        if(depth > 0.0)
        {
            /* get pointing angle index */
            int pointing_angle_index = static_cast<int>(roundf(pointing_angle));
            if(pointing_angle_index < 0) pointing_angle_index = 0;
            else if(pointing_angle_index >= NUM_POINTING_ANGLES) pointing_angle_index = NUM_POINTING_ANGLES - 1;

            /* get wind speed index */
            int wind_speed_index = static_cast<int>(roundf(extent.wind_v)) - 1;
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
