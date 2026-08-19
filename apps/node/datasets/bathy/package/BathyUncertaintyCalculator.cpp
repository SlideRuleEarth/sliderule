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
#include "BathyParameters.h"
#include "BathyDataFrame.h"

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* BathyUncertaintyCalculator::LUA_META_NAME = "BathyUncertaintyCalculator";
const struct luaL_Reg BathyUncertaintyCalculator::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

const char* BathyUncertaintyCalculator::UNCERTAINTY_FILENAMES[NUM_DIMS][NUM_POINTING_ANGLES] = {
   {"/data/SNR_ATLAS_1_deg.csv",
    "/data/SNR_ATLAS_2_deg.csv",
    "/data/SNR_ATLAS_3_deg.csv",
    "/data/SNR_ATLAS_4_deg.csv",
    "/data/SNR_ATLAS_5_deg.csv"},
   {"/data/THU_ATLAS_1_deg.csv",
    "/data/THU_ATLAS_2_deg.csv",
    "/data/THU_ATLAS_3_deg.csv",
    "/data/THU_ATLAS_4_deg.csv",
    "/data/THU_ATLAS_5_deg.csv"},
   {"/data/Transport_ATLAS_1_deg.csv",
    "/data/Transport_ATLAS_2_deg.csv",
    "/data/Transport_ATLAS_3_deg.csv",
    "/data/Transport_ATLAS_4_deg.csv",
    "/data/Transport_ATLAS_5_deg.csv"}
};

// WIND_SPEED_LUT[wind_speed] --> index
const int BathyUncertaintyCalculator::WIND_SPEED_INDEX[NUM_WIND_SPEEDS] = {
    0, // 0
    0, // 1
    1, // 2
    1, // 3
    2, // 4
    2, // 5
    3, // 6
    3, // 7
    4, // 8
    4  // 9
};

// KD_INDEX[kd] --> index
//       0             1             2             3            4
//      III           IC            3C            5C           7C
//  [0.00, 0.12]  (0.12, 0.15]  (0.15, 0.21]  (0.21, 0.27]  (0.27, 0.47]
const int BathyUncertaintyCalculator::KD_INDEX[NUM_KDS] = {
//   0   1   2   3   4   5   6   7   8   9
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // 0
     0,  0,  0,  1,  1,  1,  2,  2,  2,  2, // 1
     2,  2,  3,  3,  3,  3,  3,  3,  4,  4, // 2
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4, // 3
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4  // 4
};

vector<BathyUncertaintyCalculator::entry_t> BathyUncertaintyCalculator::SNR[NUM_POINTING_ANGLES];
vector<BathyUncertaintyCalculator::entry_t> BathyUncertaintyCalculator::THU[NUM_POINTING_ANGLES];
vector<BathyUncertaintyCalculator::entry_t> BathyUncertaintyCalculator::TRANSPORT[NUM_POINTING_ANGLES];

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * discretize - turns floating point value into an index
 *              (mode - 0: round, 1: floor, 2: ceiling)
 *----------------------------------------------------------------------------*/
typedef enum { D_ROUND, D_FLOOR, D_CEILING } dicsretize_t;
static int discretize(float value, int min, int max, dicsretize_t mode=D_ROUND)
{
    int index = 0;
    if(mode == D_ROUND) index = static_cast<int>(roundf(value));
    else if(mode == D_FLOOR) index = static_cast<int>(floorf(value));
    else if(mode == D_CEILING) index = static_cast<int>(ceilf(value));
    if(index < min) index = min;
    else if(index >= max) index = max - 1;
    return index;
}

/*----------------------------------------------------------------------------
 * elrad2deg - turns elevation radians into degrees
 *----------------------------------------------------------------------------*/
static float elrad2deg(float rad)
{
    return fabs(90.0 - ((180.0 / M_PI) * rad));
}

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parms>)
 *----------------------------------------------------------------------------*/
int BathyUncertaintyCalculator::luaCreate (lua_State* L)
{
    BathyParameters* _parms = NULL;

    try
    {
        _parms = dynamic_cast<BathyParameters*>(getLuaObject(L, 1, BathyParameters::OBJECT_TYPE, BathyParameters::LUA_META_NAME));
        return createLuaObject(L, new BathyUncertaintyCalculator(L, _parms));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaInit
 *----------------------------------------------------------------------------*/
int BathyUncertaintyCalculator::luaInit (lua_State* L)
{
    /* for each dimension */
    for(int dim = 0; dim < NUM_DIMS; dim++)
    {
        /* for each pointing angle */
        for(int pointing_angle_index = 0; pointing_angle_index < NUM_POINTING_ANGLES; pointing_angle_index++)
        {
            /* get uncertainty filename */
            const char* uncertainty_filename = UNCERTAINTY_FILENAMES[dim][pointing_angle_index];
            mlog(INFO, "Processing uncertainty file: %s", uncertainty_filename);

            /* open csv file */
            fileptr_t file = fopen(uncertainty_filename, "r");
            if(!file)
            {
                char err_buf[256];
                mlog(CRITICAL, "Failed to open file %s with error: %s", uncertainty_filename, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
                lua_pushboolean(L, false);
                return 1;
            }

            /* read header line */
            char header[40];
            if(fscanf(file, "%39s\n", header) <= 0)
            {
                mlog(CRITICAL, "Failed to read header from uncertainty file %s", uncertainty_filename);
                fclose(file);
                lua_pushboolean(L, false);
                return 1;
            }

            /* select destination table for this dimension and pointing angle */
            vector<entry_t>* dimension_tables[NUM_DIMS] = {SNR, THU, TRANSPORT};
            vector<entry_t>& tu = dimension_tables[dim][pointing_angle_index];
            tu.clear();

            /* read all rows */
            entry_t entry;
            if(dim == SNR_DIM)
            {
                /* SNR provides three coefficients: a, b, and c */
                while(fscanf(file, "%d,%15[^,],%lf,%lf,%lf\n", &entry.Wind, entry.JerlovType, &entry.a, &entry.b, &entry.c) == 5)
                {
                    tu.push_back(entry);
                }
            }
            else
            {
                /* THU and Transport provide only a and b; leave c unpopulated */
                entry.c = 0.0;
                while(fscanf(file, "%d,%15[^,],%lf,%lf\n", &entry.Wind, entry.JerlovType, &entry.a, &entry.b) == 4)
                {
                    tu.push_back(entry);
                }
            }

            /* close file */
            fclose(file);
        }
    }

    lua_pushboolean(L, true);
    return 1;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyUncertaintyCalculator::BathyUncertaintyCalculator (lua_State* L, BathyParameters* _parms):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyUncertaintyCalculator::~BathyUncertaintyCalculator (void)
{
    if(parms) parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool BathyUncertaintyCalculator::run (GeoDataFrame* dataframe)
{
    BathyDataFrame& df = *dynamic_cast<BathyDataFrame*>(dataframe);

    /* get input columns */
    FieldColumn<float>* surface_h = reinterpret_cast<FieldColumn<float>*>(df.getColumn("surface_h", true));
    FieldColumn<float>* kd = reinterpret_cast<FieldColumn<float>*>(df.getColumn("kd", true));
    FieldColumn<float>* surface_roughness = reinterpret_cast<FieldColumn<float>*>(df.getColumn("surface_roughness", true));
    if(!surface_h || !kd || !surface_roughness)
    {
        mlog(CRITICAL, "unable to find uncertainty input columns");
        return false;
    }

    /* create new columns */
    FieldColumn<float>* sigma_thu = new FieldColumn<float>;
    FieldColumn<float>* sigma_tvu = new FieldColumn<float>;

    /* for each photon in extent */
    for(long i = 0; i < df.length(); i++)
    {
        /* get lookup table entry index */
        const int pointing_angle_index = discretize(elrad2deg(df.ref_el[i]), 0, NUM_POINTING_ANGLES);
        const int wind_speed_index = discretize((*surface_roughness)[i], 0, NUM_WIND_SPEEDS);
        const int kd_index = discretize((*kd)[i], 0, NUM_KDS, D_CEILING);
        const int entry_index = wind_speed_index * 5 + kd_index;

        /* get coefficients */
        const entry_t& snr = SNR[pointing_angle_index][entry_index];
        const entry_t& thu = THU[pointing_angle_index][entry_index];
        const entry_t& transport = TRANSPORT[pointing_angle_index][entry_index];

        /**********************************************************************
            - Email from Keana Keif -
            - Dated August 17, 2026 -
            -------------------------------------------------------------------
         1   The kd ranges have changed slightly from ATL24 version 1.
         2       Kd: [0.00-0.12] m^-1 == Jerlov III,
         3       Kd: (0.12-0.15] m^-1 == Jerlov IC,
         4       Kd: (0.15-0.21] m^-1 == Jerlov 3C,
         5       Kd: (0.21-0.27] m^-1 == Jerlov 5C,
         6       Kd: (0.27-0.47] m^-1 == Jerlov 7C
         7
         8   Subaqueous TVU is now made up of two components that we are calling: Transport and signal uncertainties.
         9
         10  Transport uncertainty (previously just called tvu) follows this equation: (a^2+(b*x)^2)^0.5
         11      transport_uncertainty =  sqrt(a_transport^2 + (b_transport * depth)^2)
         12
         13  Signal uncertainty has a quadratic fit from the SNR LUTs, and then requires slightly more manipulation.
         14      SNR = a_snr*depth^2 + b_snr*depth + c_snr
         15      If SNR < 1, SNR = 1 #SNR Value cannot drop below 1 or there is no signal
         16      signal_uncertainty = 0.071 / sqrt(2 * SNR)
         17
         18  Then total vertical uncertainty is:
         19      sigma_tvu = sqrt(sigma_h^2 + transport_uncertainty^2  + signal_uncertainty^2)
         20
         21  For the horizontal uncertainty we want to add a gaussian adjustment, which means we want to multiply the THU LUT result by 0.577.
         22      sub_thu = 0.577*( a_thu + b_thu * depth)
         23      sigma_thu = sqrt (sigma_along ^2  + sigma_across^2 + sub_thu^2)
        **********************************************************************/

        /* calculate subaqueous uncertainty */
        double transport_uncertainty = 0.0;
        double signal_uncertainty = 0.0;
        double subaqueous_horizontal_uncertainty = 0.0;
        const double depth = (*surface_h)[i] - df.geoid_corr_h[i];
        if(depth > 0.0)
        {
            /* transport uncertainty */
            transport_uncertainty = sqrt(pow(transport.a, 2) + pow(transport.b * depth, 2)); // [11]

            /* signal uncertainty */
            double signal_to_noise = (snr.a * pow(depth, 2)) + (snr.b * depth) + snr.c; // [14]
            if(signal_to_noise < 1) signal_to_noise = 1; // [15]
            signal_uncertainty = 0.071 / sqrt(2 * signal_to_noise); // [16]

            /* subaqueous horizontal uncertainty */
            subaqueous_horizontal_uncertainty = 0.577 * (thu.a + (thu.b * depth)); // [22]
        }

        /* total uncertainties */
        const double total_vertical_uncertainty = sqrt(pow(df.sigma_h[i], 2) + pow(transport_uncertainty, 2) + pow(signal_uncertainty, 2)); // [19]
        const double total_horizontal_uncertainty = sqrt(pow(df.sigma_across[i], 2) + pow(df.sigma_along[i], 2) + pow(subaqueous_horizontal_uncertainty, 2));

        /* set uncertainties */
        sigma_tvu->append(static_cast<float>(total_vertical_uncertainty));
        sigma_thu->append(static_cast<float>(total_horizontal_uncertainty));
    }

    /* add columns */
    df.addExistingColumn("sigma_thu", sigma_thu, "Total horizontal uncertainty (in meters)");
    df.addExistingColumn("sigma_tvu", sigma_tvu, "Total vertical uncertainty (in meters)");

    /* mark completion */
    return true;
}
