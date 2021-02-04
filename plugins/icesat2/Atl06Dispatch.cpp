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

#include "icesat2.h"
#include "core.h"

#include <math.h>
#include <float.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double Atl06Dispatch::SPEED_OF_LIGHT = 299792458.0; // meters per second
const double Atl06Dispatch::PULSE_REPITITION_FREQUENCY = .0001; // 10Khz
const double Atl06Dispatch::SPACECRAFT_GROUND_SPEED = 7000; // meters per second
const double Atl06Dispatch::RDE_SCALE_FACTOR = 1.3490;
const double Atl06Dispatch::SIGMA_BEAM = 4.25; // meters
const double Atl06Dispatch::SIGMA_XMIT = 0.000000068; // seconds

const char* Atl06Dispatch::elRecType = "atl06rec.elevation";
const RecordObject::fieldDef_t Atl06Dispatch::elRecDef[] = {
    {"segment_id",  RecordObject::UINT32,   offsetof(elevation_t, segment_id),          1,  NULL, NATIVE_FLAGS},
    {"rgt",         RecordObject::UINT16,   offsetof(elevation_t, rgt),                 1,  NULL, NATIVE_FLAGS},
    {"cycle",       RecordObject::UINT16,   offsetof(elevation_t, cycle),               1,  NULL, NATIVE_FLAGS},
    {"spot",        RecordObject::UINT8,    offsetof(elevation_t, spot),                1,  NULL, NATIVE_FLAGS},
    {"delta_time",  RecordObject::DOUBLE,   offsetof(elevation_t, gps_time),            1,  NULL, NATIVE_FLAGS},
    {"lat",         RecordObject::DOUBLE,   offsetof(elevation_t, latitude),            1,  NULL, NATIVE_FLAGS},
    {"lon",         RecordObject::DOUBLE,   offsetof(elevation_t, longitude),           1,  NULL, NATIVE_FLAGS},
    {"h_mean",      RecordObject::DOUBLE,   offsetof(elevation_t, h_mean),              1,  NULL, NATIVE_FLAGS},
    {"dh_fit_dx",   RecordObject::DOUBLE,   offsetof(elevation_t, along_track_slope),   1,  NULL, NATIVE_FLAGS},
    {"dh_fit_dy",   RecordObject::DOUBLE,   offsetof(elevation_t, across_track_slope),  1,  NULL, NATIVE_FLAGS}
};

const char* Atl06Dispatch::atRecType = "atl06rec";
const RecordObject::fieldDef_t Atl06Dispatch::atRecDef[] = {
    {"elevation",   RecordObject::USER,     offsetof(atl06_t, elevation),               0,  elRecType, NATIVE_FLAGS}
};

const char* Atl06Dispatch::LuaMetaName = "Atl06Dispatch";
const struct luaL_Reg Atl06Dispatch::LuaMetaTable[] = {
    {"stats",       luaStats},
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
        atl06_parms_t parms = getLuaAtl06Parms(L, 2);

        /* Create ATL06 Dispatch */
        return createLuaObject(L, new Atl06Dispatch(L, outq_name, parms));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
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
     * Note: the size associated with this record includes only one elevation_t;
     * this forces any software accessing more than one elevation to manage
     * the size of the record manually.
     */
    RecordObject::recordDefErr_t at_rc = RecordObject::defineRecord(atRecType, NULL, offsetof(atl06_t, elevation[1]), atRecDef, sizeof(atRecDef) / sizeof(RecordObject::fieldDef_t), 16);
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
Atl06Dispatch::Atl06Dispatch (lua_State* L, const char* outq_name, const atl06_parms_t _parms):
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

    /* Initialize Publisher */
    outQ = new Publisher(outq_name);
    elevationIndex = 0;

    /* Initialize Statistics */
    LocalLib::set(&stats, 0, sizeof(stats));

    /* Initialize Parameters */
    parms = _parms;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Atl06Dispatch::~Atl06Dispatch(void)
{
    if(outQ) delete outQ;
    if(recObj) delete recObj;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    result_t result[PAIR_TRACKS_PER_GROUND_TRACK];

    /* Bump Statistics */
    stats.h5atl03_rec_cnt++;

    /* Get Extent */
    Atl03Reader::extent_t* extent = (Atl03Reader::extent_t*)record->getRecordData();

    /* Clear Results */
    LocalLib::set(&result, 0, sizeof(result_t) * PAIR_TRACKS_PER_GROUND_TRACK);

    /* Initialize Results */
    int first_photon = 0;
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        /* Elevation Attributes */
        result[t].elevation.segment_id = extent->segment_id[t];
        result[t].elevation.rgt = extent->reference_ground_track_start;
        result[t].elevation.cycle = extent->cycle_start;
        result[t].elevation.gps_time = extent->gps_time[t];
        result[t].elevation.latitude = extent->latitude[t];
        result[t].elevation.longitude = extent->longitude[t];

        /* Copy In Initial Set of Photons */
        result[t].photon_count = extent->photon_count[t];
        if(result[t].photon_count > 0)
        {
            result[t].photons = new point_t[result[t].photon_count];
            for(int p = 0; p < result[t].photon_count; p++)
            {
                result[t].photons[p].x = extent->photons[first_photon + p].distance_x;
                result[t].photons[p].y = extent->photons[first_photon + p].height_y;
            }
            first_photon += result[t].photon_count;
        }
    }
    /* Calcualte Beam Number */
    calculateBeam((sc_orient_t)extent->spacecraft_orientation, (track_t)extent->reference_pair_track, result);

    /* Execute Algorithm Stages */
    if(parms.stages[STAGE_LSF]) iterativeFitStage(extent, result);

    /* Post Elevation  */
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        if(result[t].provided)
        {
            postResult(&result[t].elevation);
        }
    }

    /* Clean Up Results */
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        if(result[t].photons)
        {
            delete [] result[t].photons;
        }
    }

    /* Return Status */
    return true;
}

/*----------------------------------------------------------------------------
 * processTimeout
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processTimeout (void)
{
    postResult(NULL);
    return true;
}

/*----------------------------------------------------------------------------
 * processTermination
 *
 *  Note that RecordDispatcher will only call this once
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processTermination (void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * calculateBeam
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::calculateBeam (sc_orient_t sc_orient, track_t track, result_t* result)
{
    if(sc_orient == SC_BACKWARD)
    {
        if(track == RPT_1)
        {
            result[PRT_LEFT].elevation.spot = SPOT_5;
            result[PRT_RIGHT].elevation.spot = SPOT_6;
        }
        else if(track == RPT_2)
        {
            result[PRT_LEFT].elevation.spot = SPOT_3;
            result[PRT_RIGHT].elevation.spot = SPOT_4;
        }
        else if(track == RPT_3)
        {
            result[PRT_LEFT].elevation.spot = SPOT_1;
            result[PRT_RIGHT].elevation.spot = SPOT_2;
        }
    }
    else if(sc_orient == SC_FORWARD)
    {
        if(track == RPT_1)
        {
            result[PRT_LEFT].elevation.spot = SPOT_2;
            result[PRT_RIGHT].elevation.spot = SPOT_1;
        }
        else if(track == RPT_2)
        {
            result[PRT_LEFT].elevation.spot = SPOT_4;
            result[PRT_RIGHT].elevation.spot = SPOT_3;
        }
        else if(track == RPT_3)
        {
            result[PRT_LEFT].elevation.spot = SPOT_6;
            result[PRT_RIGHT].elevation.spot = SPOT_5;
        }
    }
}

/*----------------------------------------------------------------------------
 * postResult
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::postResult (elevation_t* elevation)
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
 * iterativeFitStage
 *
 *  Note: Section 5.5 - Signal selection based on ATL03 flags
 *        Procedures 4b and after
 *
 *  TODO: replace spacecraft ground speed constant with value provided in ATL03
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::iterativeFitStage (Atl03Reader::extent_t* extent, result_t* result)
{
    /* Process Tracks */
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        bool done = false;
        int iteration = 0;

        /* Initial Per Track Calculations */
        double pulses_in_segment    = (extent->segment_size[t] * PULSE_REPITITION_FREQUENCY) / SPACECRAFT_GROUND_SPEED; // N_seg_pulses, section 5.4, procedure 1d
        double background_density   = pulses_in_segment * extent->background_rate[t] / (SPEED_OF_LIGHT / 2.0); // BG_density, section 5.7, procedure 1c

        /* Iterate Processing of Photons */
        while(!done)
        {
            int num_photons = result[t].photon_count;

            /* Check Photon Count */
            if(num_photons < parms.minimum_photon_count)
            {
                result[t].violated_count = true;
                done = true;
                continue;
            }

            /* Calculate Least Squares Fit */
            lsf_t fit = lsf(result[t].photons, num_photons);
            result[t].elevation.h_mean = fit.intercept;
            result[t].elevation.along_track_slope = fit.slope;
            result[t].provided = true;

            /* Check Spread */
            if( (fit.x_max - fit.x_min) < parms.along_track_spread )
            {
                result[t].violated_spread = true;
                done = true;
                continue;
            }

            /* Check Iterations */
            if(iteration++ > parms.max_iterations)
            {
                result[t].violated_iterations = true;
                done = true;
                continue;
            }

            /* Calculate Residuals */
            for(int p = 0; p < num_photons; p++)
            {
                result[t].photons[p].r = result[t].photons[p].y - (fit.intercept + (result[t].photons[p].x * fit.slope));
            }

            /* Sort Points by Residuals */
            quicksort(result[t].photons, 0, num_photons-1);

            /* Calculate Inputs to Robust Dispersion Estimate */
            double  background_count;       // N_BG
            double  window_lower_bound;     // zmin
            double  window_upper_bound;     // zmax;
            if(iteration == 1)
            {
                background_count    = background_density; // TODO: not scaled by vertical range of photons???
                window_lower_bound  = result[t].photons[0].r; // section 5.5, procedure 4c
                window_upper_bound  = result[t].photons[num_photons - 1].r; // section 5.5, procedure 4c
            }
            else
            {
                background_count    = result[t].window_height * background_density; // section 5.7, procedure 2c
                window_lower_bound  = -(result[t].window_height / 2.0); // section 5.7, procedure 2c
                window_upper_bound  = result[t].window_height / 2.0; // section 5.7, procedure 2c
            }

            /* Continued Inputs to Robust Dispersion Estimate */
            double background_rate  = background_count / (window_upper_bound - window_lower_bound); // bckgrd, section 5.9, procedure 1a
            double signal_count     = num_photons - background_count; // N_sig, section 5.9, procedure 1b
            double sigma_r          = 0.0; // sigma_r

            /* Calculate Robust Dispersion Estimate */
            if(signal_count <= 1)
            {
                sigma_r = (window_upper_bound - window_lower_bound) / num_photons; // section 5.9, procedure 1c
            }
            else
            {
                /* Find Smallest Potential Percentiles (0) */
                int32_t i0 = 0;
                while(i0 < num_photons)
                {
                    double spp = (0.25 * signal_count) + ((result[t].photons[i0].r - window_lower_bound) * background_rate); // section 5.9, procedure 4a
                    if( (((double)i0) + 1.0 - 0.5) < spp )  i0++;
                    else                                    break;
                }

                /* Find Smallest Potential Percentiles (1) */
                int32_t i1 = num_photons;
                while(i1 >= 0)
                {
                    double spp = (0.75 * signal_count) + ((result[t].photons[i1].r - window_lower_bound) * background_rate); // section 5.9, procedure 4a
                    if( (((double)i1) - 1.0 - 0.5) > spp )  i1--;
                    else                                    break;
                }

                /* Check Need to Refind Percentiles */
                if(i1 < i0)
                {
                    double spp0 = (num_photons / 4.0) - (signal_count / 2.0); // section 5.9, procedure 5a
                    double spp1 = (num_photons / 4.0) + (signal_count / 2.0); // section 5.9, procedure 5b

                    /* Find Spread of Central Values (0) */
                    i0 = 0;
                    while(i0 < num_photons)
                    {
                        if( (((double)i0) + 1.0 - 0.5) < spp0 ) i0++;
                        else                                    break;
                    }

                    /* Find Spread of Central Values (1) */
                    i1 = num_photons;
                    while(i1 >= 0)
                    {
                        if( (((double)i1) - 1.0 - 0.5) > spp1 ) i1--;
                        else                                    break;
                    }
                }

                /* Calculate Robust Dispersion Estimate */
                sigma_r = (result[t].photons[i1].r - result[t].photons[i0].r) / RDE_SCALE_FACTOR; // section 5.9, procedure 6
            }

            /* Calculate Sigma Expected */
            double se1 = pow((SPEED_OF_LIGHT / 2.0) * SIGMA_XMIT, 2);
            double se2 = pow(SIGMA_BEAM, 2) * pow(result[t].elevation.along_track_slope, 2);
            double sigma_expected = sqrt(se1 + se2); // sigma_expected, section 5.5, procedure 4d

            /* Calculate Window Height */
            if(sigma_r > parms.maximum_robust_dispersion) sigma_r = parms.maximum_robust_dispersion;
            double new_window_height = MAX(MAX(parms.minimum_window, 6.0 * sigma_expected), 6.0 * sigma_r); // H_win, section 5.5, procedure 4e
            result[t].window_height = MAX(new_window_height, 0.75 * result[t].window_height); // section 5.7, procedure 2e
            double window_spread = result[t].window_height / 2.0;

            /* Filtered Out Photons in Results (section 5.5, procedure 4f) */
            int32_t ph_in = 0;
            for(int p = 0; p < num_photons; p++)
            {
                if(abs(result[t].photons[p].r) < window_spread)
                {
                    result[t].photons[ph_in++] = result[t].photons[p];
                }
            }

            /*
             * TODO: section 5.7, procedure 2h
             * undo the window_height and selected PE
             * if along_track_spread and minimum_photon_count not reached
             */

            /* Set New Number of Photons */
            if(ph_in != result[t].photon_count)
            {
                result[t].photon_count = ph_in; // from filtering above
            }
            else // no change in photons
            {
                done = true;
            }
        }
    }
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
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error configuring %s: %s\n", LuaMetaName, e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * lsf - least squares fit
 *
 *  TODO: currently no protections against divide-by-zero
 *----------------------------------------------------------------------------*/
Atl06Dispatch::lsf_t Atl06Dispatch::lsf (point_t* array, int size)
{
    lsf_t fit;

    /* Initialize Min's and Max's */
    fit.x_min = DBL_MAX;
    fit.x_max = DBL_MIN;

    /* Calculate GT*G and GT*h*/
    double gtg_11 = size;
    double gtg_12_21 = 0.0;
    double gtg_22 = 0.0;
    double gth_1 = 0.0;
    double gth_2 = 0.0;
    for(int p = 0; p < size; p++)
    {
        gtg_12_21 += array[p].x;
        gtg_22 += array[p].x * array[p].x;
        gth_1 += array[p].y;
        gth_2 += array[p].x * array[p].y;

        /* Get Min's and Max's */
        if(array[p].x > fit.x_min)  fit.x_min = array[p].x;
        if(array[p].x < fit.x_max)  fit.x_max = array[p].x;
    }

    /* Calculate Inverse of GT*G */
    double det = 1.0 / ((gtg_11 * gtg_22) - (gtg_12_21 * gtg_12_21));
    double igtg_11 = gtg_22 * det;
    double igtg_12_21 = -1 * gtg_12_21 * det;
    double igtg_22 = gtg_11 * det;

    /* Calculate IGTG * GTh */
    fit.intercept = (igtg_11 * gth_1) + (igtg_12_21 * gth_2);
    fit.slope = (igtg_12_21 * gth_1) + (igtg_22 * gth_2);

    /* Return Fit */
    return fit;
}

/*----------------------------------------------------------------------------
 * quicksort
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::quicksort(point_t* array, int start, int end)
{
    if(start < end)
    {
        int partition = quicksortpartition(array, start, end);
        quicksort(array, start, partition);
        quicksort(array, partition + 1, end);
    }
}

/*----------------------------------------------------------------------------
 * quicksortpartition
 *----------------------------------------------------------------------------*/
int Atl06Dispatch::quicksortpartition(point_t* array, int start, int end)
{
    double pivot = array[(start + end) / 2].r;

    start--;
    end++;
    while(true)
    {
        while (array[++start].r < pivot);
        while (array[--end].r > pivot);
        if (start >= end) return end;

        point_t tmp = array[start];
        array[start] = array[end];
        array[end] = tmp;
    }
}
