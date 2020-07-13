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

//  FOR each 40m segment:
//      (1) Photon-Classification Stage {3.1}
//
//          IF (at least 10 photons) AND (at least 20m horizontal spread) THEN
//              a. select the set of photons from ATL03 (2x20m segments) based on signal_conf_ph_t threshold [sig_thresh]
//              b. fit sloping line segment to photons
//              c. calculate robust spread of the residuals [sigma_r]
//              d. select the set of photons used to fit line AND that fall within max(+/- 1.5m, 3*sigma_r) of line
//          ELSE
//              a. add 20m to beginning and end of segment to create 80m segment
//              b. histogram all photons into 10m vertical bins
//              c. select the set of photons in the maximum (Nmax) bin AND photons that fall in bins with a count that is Nmax - sqrt(Nmax)
//              d. select subset of photons above that are within the original 40m segment
//
//          FINALY identify height of photons selected by above steps [h_widnow]
//
//      (2) Photon-Selection-Refinement Stage {3.2}
//
//          WHILE iterations are less than 20 AND subset of photons changes each iteration
//              a. least-squares fit current set of photons: x = curr_photon - segment_center, y = photon_height
//                  i.  calculate mean height [h_mean]
//                  ii. calculate slope [dh/dx]
//              b. calculate robust estimator (similar to standard deviation) of residuals
//                  i.  calculate the median height (i.e. middle of the window at given point) [r_med]
//                  ii. calculate background-corrected spread of distribution [r_o]; force r_o to be at most 5m
//                  iii.calculate expected spread of return photons [h_expected_rms]
//              c. select subset of photons that fall within new window
//                  i.  determine new window: h_window = MAX(6*r_o, 6*h_expected_rms, 0.75 * h_window_last, 3m)
//                  ii. select photon if distance from r_med falls within h_window/2
//
//      (3) Surface Height Quality Stage {3.2.1}
//
//          CALCULATE signal to noise significance

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Atl06Dispatch.h"
#include "Atl03Device.h"
#include "MathLib.h"
#include "core.h"

#include <math.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double Atl06Dispatch::PULSE_REPITITION_FREQUENCY = .0001; // 10Khz
const double Atl06Dispatch::SPACECRAFT_GROUND_SPEED = 7000; // meters per second
const double Atl06Dispatch::RDE_SCALE_FACTOR = 1.3490;
const double Atl06Dispatch::SIGMA_BEAM = 4.25; // meters
const double Atl06Dispatch::SIGMA_XMIT = 0.000000068; // seconds
const double Atl06Dispatch::H_WIN_MIN = 3.0; // meters

const char* Atl06Dispatch::elRecType = "atl06rec.elevation";
const RecordObject::fieldDef_t Atl06Dispatch::elRecDef[] = {
    {"SEG_ID",  RecordObject::UINT32,   offsetof(elevation_t, segment_id),          1,  NULL, NATIVE_FLAGS},
    {"GRT",     RecordObject::UINT16,   offsetof(elevation_t, grt),                 1,  NULL, NATIVE_FLAGS},
    {"CYCLE",   RecordObject::UINT16,   offsetof(elevation_t, cycle),               1,  NULL, NATIVE_FLAGS},
    {"GPS",     RecordObject::DOUBLE,   offsetof(elevation_t, gps_time),            1,  NULL, NATIVE_FLAGS},
    {"LAT",     RecordObject::DOUBLE,   offsetof(elevation_t, latitude),            1,  NULL, NATIVE_FLAGS},
    {"LON",     RecordObject::DOUBLE,   offsetof(elevation_t, longitude),           1,  NULL, NATIVE_FLAGS},
    {"HEIGHT",  RecordObject::DOUBLE,   offsetof(elevation_t, height),              1,  NULL, NATIVE_FLAGS},
    {"ALTS",    RecordObject::DOUBLE,   offsetof(elevation_t, along_track_slope),   1,  NULL, NATIVE_FLAGS},
    {"ACTS",    RecordObject::DOUBLE,   offsetof(elevation_t, across_track_slope),  1,  NULL, NATIVE_FLAGS}
};

const char* Atl06Dispatch::atRecType = "atl06rec";
const RecordObject::fieldDef_t Atl06Dispatch::atRecDef[] = {
    {"ELEVATION",   RecordObject::USER, offsetof(atl06_t, elevation),               0, elRecType, NATIVE_FLAGS}
};

const char* Atl06Dispatch::LuaMetaName = "Atl06Dispatch";
const struct luaL_Reg Atl06Dispatch::LuaMetaTable[] = {
    {"stats",       luaStats},
    {"select",      luaSelect},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :atl06(<outq name>)
 *----------------------------------------------------------------------------*/
int Atl06Dispatch::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);

        /* Create ATL06 Dispatch */
        return createLuaObject(L, new Atl06Dispatch(L, outq_name));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::init (void)
{
    RecordObject::recordDefErr_t el_rc = RecordObject::defineRecord(elRecType, NULL, sizeof(elevation_t), elRecDef, sizeof(elRecDef) / sizeof(RecordObject::fieldDef_t), 16);
    if(el_rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d\n", elRecType, el_rc);
    }

    /*
     * Note: the size associated with this record is only one elevation_t;
     * this forces any software accessing more than one elevation to manage
     * the size of the record manually.
     */
    RecordObject::recordDefErr_t at_rc = RecordObject::defineRecord(atRecType, NULL, sizeof(elevation_t), atRecDef, sizeof(atRecDef) / sizeof(RecordObject::fieldDef_t), 16);
    if(at_rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d\n", atRecType, at_rc);
    }
}

/******************************************************************************
 * PRIVATE METOHDS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl06Dispatch::Atl06Dispatch (lua_State* L, const char* outq_name):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(outq_name);

    /*
     * Note: when allocating memory for this record, the full atl06_t size is used;
     * this extends the memory available past the one elevation_t provided in the
     * definition.
     */
    recObj = new RecordObject(atRecType, sizeof(atl06_t));
    recData = (atl06_t*)recObj->getRecordData();

    /* Initialize class members */
    outQ = new Publisher(outq_name);
    elevationIndex = 0;
    LocalLib::set(&stats, 0, sizeof(stats));

    /* Provide default stages */
    stages[STAGE_LSF] = true;
    stages[STAGE_RSR] = true;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Atl06Dispatch::~Atl06Dispatch(void)
{
    if(outQ) delete outQ;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    result_t result;
    bool status = false;

    /* Bump Statistics */
    stats.h5atl03_rec_cnt++;

    /* Get Extent */
    Atl03Device::extent_t* extent = (Atl03Device::extent_t*)record->getRecordData();

    /* Clear Results */
    LocalLib::set(&result, 0, sizeof(result_t));

    /* Execute Algorithm Stages */
    if(stages[STAGE_LSF])           status = leastSquaresFitStage(extent, &result);
    if(status && stages[STAGE_RSR]) status = robustSpreadOfResidualsStage(extent, &result);

    /* Populate Elevation  */
    if(result.status[PRT_LEFT])     populateElevation(&result.elevation[PRT_LEFT]);
    if(result.status[PRT_RIGHT])    populateElevation(&result.elevation[PRT_RIGHT]);

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * processTimeout
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processTimeout (void)
{
    populateElevation(NULL);
    return true;
}

/*----------------------------------------------------------------------------
 * populateElevation
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::populateElevation (elevation_t* elevation)
{
    elevationMutex.lock();
    {
        /* Populate Elevation */
        if(elevation)
        {
            recData->elevation[elevationIndex++] = *elevation;
        }

        /* Check If ATL06 Record Should Be Posted*/
        if((!elevation && elevationIndex > 0) || elevationIndex == BATCH_SIZE)
        {
            /* Serialize Record */
            unsigned char* buffer;
            int size = recObj->serialize(&buffer, RecordObject::REFERENCE);

            /* Adjust Size (according to number of elevations) */
            size -= (BATCH_SIZE - elevationIndex) * sizeof(elevation_t);

            /* Reset Elevation Index */
            elevationIndex = 0;

            /* Post Record */
            if(outQ->postCopy(buffer, size, SYS_TIMEOUT) > 0)
            {
                stats.post_success_cnt++;
            }
            else
            {
                stats.post_dropped_cnt++;
            }
        }
    }
    elevationMutex.unlock();
}

/*----------------------------------------------------------------------------
 * leastSquaresFitStage
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::leastSquaresFitStage (Atl03Device::extent_t* extent, result_t* result)
{
    bool status = false;

    /* Process Tracks */
    uint32_t first_photon = 0;
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        MathLib::lsf_t lsf;

        /* Calculate Least Squares Fit */
        if(extent->photon_count[t] > 0)
        {
            lsf = MathLib::lsf((MathLib::point_t*)&extent->photons[first_photon], extent->photon_count[t]);

            /* Populate Result */
            result->elevation[t].height = lsf.intercept;
            result->elevation[t].along_track_slope = lsf.slope;
            result->status[t] = true;

            /* At least one track processed */
            status = true;
        }

        /* Increment First Photon to Next Track */
        first_photon += extent->photon_count[t];
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * robustSpreadOfResidualsStage
 * 
 *  Note: Section 5.5 - Signal selection based on ATL03 flags
 *        Procedures 4b and after
 * 
 *  TODO: replace spacecraft ground speed constant with value provided in ATL03
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::robustSpreadOfResidualsStage (Atl03Device::extent_t* extent, result_t* result)
{
    bool status = false;

    /* Process Tracks */
    uint32_t first_photon = 0;
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        int size = extent->photon_count[t];

        /* Select Photons in Window */
        if(size > 0)
        {
            double robust_dispersion_estimate = 0.0;

            /* Calculate Background Density */
            double pulses_in_segment = (extent->segment_size[t] * PULSE_REPITITION_FREQUENCY) / SPACECRAFT_GROUND_SPEED; // N_seg_pulses, section 5.4, procedure 1d
            double background_density = pulses_in_segment * extent->background_rate[t] / (MathLib::SPEED_OF_LIGHT / 2.0); // BG_density 

            /* Calculate Residuals */
            MathLib::lsf_t lsf = { result->elevation[t].height, result->elevation[t].along_track_slope };
            double* residuals = new double[extent->photon_count[t]];
            MathLib::residuals(lsf, (MathLib::point_t*)&extent->photons[first_photon], extent->photon_count[t], residuals);

            /* Sort Residuals */
            int* indices = new int[size];
            for(int i = 0; i < size; i++) indices[i] = i;
            MathLib::sort(residuals, size, indices);

            /* Get Max and Min */
            double zmin = residuals[0];
            double zmax = residuals[size - 1];
            double zdelta = zmax - zmin;

            /* Get Background and Signal Estimates */
            double background_estimate = background_density / zdelta; // bckgrd, section 5.9, procedure 1a
            int32_t signal_count = size - background_density; // N_sig, section 5.9, procedure 1b

            /* Calculate Robust Dispersion Estimate */
            if(signal_count <= 1)
            {
                robust_dispersion_estimate = zdelta / size;
            }
            else
            {
                /* Find Smallest Potential Percentiles (0) */
                int32_t i0 = 0;
                while(i0 < size)
                {
                    double spp = (0.25 * signal_count) + ((residuals[i0] - zmin) * background_estimate); // section 5.9, procedure 4a
                    if( (((double)i0) + 1.0 - 0.5) < spp )  i0++;
                    else                                    break;
                }

                /* Find Smallest Potential Percentiles (1) */
                int32_t i1 = size;
                while(i1 >= 0)
                {
                    double spp = (0.75 * signal_count) + ((residuals[i1] - zmin) * background_estimate); // section 5.9, procedure 4a
                    if( (((double)i1) - 1.0 - 0.5) > spp )  i1--;
                    else                                    break;
                }

                /* Check Need to Refind Percentiles */
                if(i1 < i0)
                {
                    double spp0 = (size / 4.0) - (signal_count / 2.0); // section 5.9, procedure 5a
                    double spp1 = (size / 4.0) + (signal_count / 2.0); // section 5.9, procedure 5b

                    /* Find Spread of Central Values (0) */
                    i0 = 0;
                    while(i0 < size)
                    {
                        if( (((double)i0) + 1.0 - 0.5) < spp0 ) i0++;
                        else                                    break;
                    }

                    /* Find Spread of Central Values (1) */
                    i1 = size;
                    while(i1 >= 0)
                    {
                        if( (((double)i1) - 1.0 - 0.5) > spp1 ) i1--;
                        else                                    break;
                    }
                }

                /* Calculate Robust Dispersion Estimate */
                robust_dispersion_estimate = (residuals[i1] - residuals[i0]) / RDE_SCALE_FACTOR; // section 5.9, procedure 6
            }

            /* Calculate Sigma Expected */
            double se1 = pow((MathLib::SPEED_OF_LIGHT / 2.0) * SIGMA_XMIT, 2);
            double se2 = pow(SIGMA_BEAM, 2) * pow(result->elevation[t].along_track_slope, 2);
            double sigma_expected = sqrt(se1 + se2);

            /* Calculate Window Height */
            double window_height = MAX(MAX(H_WIN_MIN, 6.0 * sigma_expected), 6.0 * robust_dispersion_estimate); // H_win, section 5.5, procedure 4e

            /* Filter Out Photons */
            int32_t ph_in = 0;
            Atl03Device::photon_t* photons = new Atl03Device::photon_t[size];
            double window_spread = window_height / 2.0;
            for(int r = 0; r < size; r++)
            {
                if(abs(residuals[r]) < window_spread)
                {
                    int32_t r_index = indices[r];
                    photons[ph_in++] = extent->photons[first_photon + r_index];
                }
            }

            /* At least one track processed */
            status = true;
        }

        /* Increment First Photon to Next Track */
        first_photon += size;
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * luaStats
 *----------------------------------------------------------------------------*/
int Atl06Dispatch::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        Atl06Dispatch* lua_obj = (Atl06Dispatch*)getLuaSelf(L, 1);

        /* Get Clear Parameter */
        bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "h5atl03",         lua_obj->stats.h5atl03_rec_cnt);
        LuaEngine::setAttrInt(L, "posted",          lua_obj->stats.post_success_cnt);
        LuaEngine::setAttrInt(L, "dropped",         lua_obj->stats.post_dropped_cnt);

        /* Optionally Clear */
        if(with_clear) LocalLib::set(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring %s: %s\n", LuaMetaName, e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * luaSelect - :select(<algorithm stage>, <enable/disable>)
 *----------------------------------------------------------------------------*/
int Atl06Dispatch::luaSelect (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        Atl06Dispatch* lua_obj = (Atl06Dispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        int algo_stage = getLuaInteger(L, 2);
        bool enable = getLuaBoolean(L, 3);

        /* Set Stage */
        if(algo_stage >= 0 && algo_stage < NUM_STAGES)
        {
            mlog(INFO, "Selecting stage: %d\n", algo_stage);
            lua_obj->stages[algo_stage] = enable;
            status = true;
        }
        else
        {
            throw LuaException("Invalid stage specified: %d\n", algo_stage);
        }
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error selecting algorithm stage: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
