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

#include <math.h>
#include <float.h>

#include "core.h"
#include "icesat2.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const double Atl06Dispatch::SPEED_OF_LIGHT = 299792458.0; // meters per second
const double Atl06Dispatch::PULSE_REPITITION_FREQUENCY = 10000.0; // 10Khz
const double Atl06Dispatch::RDE_SCALE_FACTOR = 1.3490;
const double Atl06Dispatch::SIGMA_BEAM = 4.25; // meters
const double Atl06Dispatch::SIGMA_XMIT = 0.00000000068; // seconds

/* Compact Record Definitions */

const char* Atl06Dispatch::elCompactRecType = "atl06rec-compact.elevation"; // elevation measurement record
const RecordObject::fieldDef_t Atl06Dispatch::elCompactRecDef[] = {
    {"delta_time",              RecordObject::DOUBLE,   offsetof(elevation_compact_t, delta_time),  1,  NULL, NATIVE_FLAGS},
    {"lat",                     RecordObject::DOUBLE,   offsetof(elevation_compact_t, latitude),    1,  NULL, NATIVE_FLAGS},
    {"lon",                     RecordObject::DOUBLE,   offsetof(elevation_compact_t, longitude),   1,  NULL, NATIVE_FLAGS},
    {"h_mean",                  RecordObject::DOUBLE,   offsetof(elevation_compact_t, h_mean),      1,  NULL, NATIVE_FLAGS}
};

const char* Atl06Dispatch::atCompactRecType = "atl06rec-compact";
const RecordObject::fieldDef_t Atl06Dispatch::atCompactRecDef[] = {
    {"elevation",               RecordObject::USER,     offsetof(atl06_compact_t, elevation),       0,  elCompactRecType, NATIVE_FLAGS}
};

/* (Normal) Record Definitions */

const char* Atl06Dispatch::elRecType = "atl06rec.elevation"; // extended elevation measurement record
const RecordObject::fieldDef_t Atl06Dispatch::elRecDef[] = {
    {"extent_id",               RecordObject::UINT64,   offsetof(elevation_t, extent_id),           1,  NULL, NATIVE_FLAGS},
    {"segment_id",              RecordObject::UINT32,   offsetof(elevation_t, segment_id),          1,  NULL, NATIVE_FLAGS},
    {"n_fit_photons",           RecordObject::INT32,    offsetof(elevation_t, photon_count),        1,  NULL, NATIVE_FLAGS},
    {"pflags",                  RecordObject::UINT16,   offsetof(elevation_t, pflags),              1,  NULL, NATIVE_FLAGS},
    {"rgt",                     RecordObject::UINT16,   offsetof(elevation_t, rgt),                 1,  NULL, NATIVE_FLAGS},
    {"cycle",                   RecordObject::UINT16,   offsetof(elevation_t, cycle),               1,  NULL, NATIVE_FLAGS},
    {"spot",                    RecordObject::UINT8,    offsetof(elevation_t, spot),                1,  NULL, NATIVE_FLAGS},
    {"gt",                      RecordObject::UINT8,    offsetof(elevation_t, gt),                  1,  NULL, NATIVE_FLAGS},
    {"distance",                RecordObject::DOUBLE,   offsetof(elevation_t, distance),            1,  NULL, NATIVE_FLAGS},
    {"delta_time",              RecordObject::DOUBLE,   offsetof(elevation_t, delta_time),          1,  NULL, NATIVE_FLAGS},
    {"lat",                     RecordObject::DOUBLE,   offsetof(elevation_t, latitude),            1,  NULL, NATIVE_FLAGS},
    {"lon",                     RecordObject::DOUBLE,   offsetof(elevation_t, longitude),           1,  NULL, NATIVE_FLAGS},
    {"h_mean",                  RecordObject::DOUBLE,   offsetof(elevation_t, h_mean),              1,  NULL, NATIVE_FLAGS},
    {"dh_fit_dx",               RecordObject::DOUBLE,   offsetof(elevation_t, along_track_slope),   1,  NULL, NATIVE_FLAGS},
    {"dh_fit_dy",               RecordObject::DOUBLE,   offsetof(elevation_t, across_track_slope),  1,  NULL, NATIVE_FLAGS},
    {"w_surface_window_final",  RecordObject::DOUBLE,   offsetof(elevation_t, window_height),       1,  NULL, NATIVE_FLAGS},
    {"rms_misfit",              RecordObject::DOUBLE,   offsetof(elevation_t, rms_misfit),          1,  NULL, NATIVE_FLAGS},
    {"h_sigma",                 RecordObject::DOUBLE,   offsetof(elevation_t, h_sigma),             1,  NULL, NATIVE_FLAGS}
};

const char* Atl06Dispatch::atRecType = "atl06rec";
const RecordObject::fieldDef_t Atl06Dispatch::atRecDef[] = {
    {"elevation",               RecordObject::USER,     offsetof(atl06_t, elevation),               0,  elRecType, NATIVE_FLAGS}
};

/* Lua Functions */

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
        icesat2_parms_t* atl06_parms = getLuaIcesat2Parms(L, 2);

        /* Create ATL06 Dispatch */
        return createLuaObject(L, new Atl06Dispatch(L, outq_name, atl06_parms));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::init (void)
{
    RecordObject::recordDefErr_t rc;

    rc = RecordObject::defineRecord(elRecType, NULL, sizeof(elevation_t), elRecDef, sizeof(elRecDef) / sizeof(RecordObject::fieldDef_t));
    if(rc != RecordObject::SUCCESS_DEF) mlog(CRITICAL, "Failed to define %s: %d", elRecType, rc);

    rc = RecordObject::defineRecord(elCompactRecType, NULL, sizeof(elevation_compact_t), elCompactRecDef, sizeof(elCompactRecDef) / sizeof(RecordObject::fieldDef_t));
    if(rc != RecordObject::SUCCESS_DEF) mlog(CRITICAL, "Failed to define %s: %d", elCompactRecType, rc);

    /*
     * Note: the size associated with these records includes only one elevation;
     * this forces any software accessing more than one elevation to manage
     * the size of the record manually.
     */

    rc = RecordObject::defineRecord(atRecType, NULL, offsetof(atl06_t, elevation[1]), atRecDef, sizeof(atRecDef) / sizeof(RecordObject::fieldDef_t));
    if(rc != RecordObject::SUCCESS_DEF) mlog(CRITICAL, "Failed to define %s: %d", atRecType, rc);

    rc = RecordObject::defineRecord(atCompactRecType, NULL, offsetof(atl06_compact_t, elevation[1]), atCompactRecDef, sizeof(atCompactRecDef) / sizeof(RecordObject::fieldDef_t));
    if(rc != RecordObject::SUCCESS_DEF) mlog(CRITICAL, "Failed to define %s: %d", atCompactRecType, rc);
}

/******************************************************************************
 * PRIVATE METOHDS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl06Dispatch::Atl06Dispatch (lua_State* L, const char* outq_name, const icesat2_parms_t* _parms):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(outq_name);
    assert(_parms);

    /* Initialize Parameters */
    parms = _parms;

    /*
     * Note: when allocating memory for this record, the full record size is used;
     * this extends the memory available past the one elevation provided in the
     * definition.
     */
    if(!parms->compact)
    {
        recObj = new RecordObject(atRecType, sizeof(atl06_t));
        recData = (atl06_t*)recObj->getRecordData();
    }
    else
    {
        recObj = new RecordObject(atCompactRecType, sizeof(atl06_compact_t));
        recCompactData = (atl06_compact_t*)recObj->getRecordData();
    }

    /* Initialize Publisher */
    outQ = new Publisher(outq_name);
    elevationIndex = 0;

    /* Initialize Statistics */
    LocalLib::set(&stats, 0, sizeof(stats));
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Atl06Dispatch::~Atl06Dispatch(void)
{
    delete outQ;
    delete recObj;

    freeLuaIcesat2Parms(parms);
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    result_t result[PAIR_TRACKS_PER_GROUND_TRACK];
    Atl03Reader::extent_t* extent = (Atl03Reader::extent_t*)record->getRecordData();

    /* Bump Statistics */
    stats.h5atl03_rec_cnt++;

    /* Execute Algorithm Stages */
    initializationStage(extent, result); // allocates photons[]
    if(parms->stages[STAGE_LSF]) iterativeFitStage(extent, result);
    postResult(result); // deallocates memory

    /* Return Status */
    return true;
}

/*----------------------------------------------------------------------------
 * processTimeout
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processTimeout (void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * processTermination
 *
 *  Note that RecordDispatcher will only call this once
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processTermination (void)
{
    postResult(NULL);
    return true;
}

/*----------------------------------------------------------------------------
 * initializationStage
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::initializationStage (Atl03Reader::extent_t* extent, result_t* result)
{
    /* Clear Results */
    LocalLib::set(result, 0, sizeof(result_t) * PAIR_TRACKS_PER_GROUND_TRACK);

    /* Initialize Results */
    int first_photon = 0;
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        /* Elevation Attributes */
        result[t].elevation.extent_id = extent->extent_id | EXTENT_ID_ELEVATION | t;
        result[t].elevation.segment_id = extent->segment_id[t];
        result[t].elevation.rgt = extent->reference_ground_track_start;
        result[t].elevation.cycle = extent->cycle_start;
        result[t].elevation.distance = extent->segment_distance[t];

        /* Copy In Initial Set of Photons */
        result[t].elevation.photon_count = extent->photon_count[t];
        if(result[t].elevation.photon_count > 0)
        {
            result[t].photons = new point_t[result[t].elevation.photon_count];
            for(int p = 0; p < result[t].elevation.photon_count; p++)
            {
                result[t].photons[p].p = first_photon + p;  // extent->photons[]
            }
            first_photon += result[t].elevation.photon_count;
        }
    }

    /* Calcualte Beam Numbers */
    sc_orient_t sc_orient = (sc_orient_t)extent->spacecraft_orientation;
    track_t track = (track_t)extent->reference_pair_track;
    if(sc_orient == SC_BACKWARD)
    {
        if(track == RPT_1)
        {
            result[PRT_LEFT].elevation.spot = SPOT_1;
            result[PRT_RIGHT].elevation.spot = SPOT_2;
            result[PRT_LEFT].elevation.gt = (uint8_t)GT1L;
            result[PRT_RIGHT].elevation.gt = (uint8_t)GT1R;
        }
        else if(track == RPT_2)
        {
            result[PRT_LEFT].elevation.spot = SPOT_3;
            result[PRT_RIGHT].elevation.spot = SPOT_4;
            result[PRT_LEFT].elevation.gt = (uint8_t)GT2L;
            result[PRT_RIGHT].elevation.gt = (uint8_t)GT2R;
        }
        else if(track == RPT_3)
        {
            result[PRT_LEFT].elevation.spot = SPOT_5;
            result[PRT_RIGHT].elevation.spot = SPOT_6;
            result[PRT_LEFT].elevation.gt = (uint8_t)GT3L;
            result[PRT_RIGHT].elevation.gt = (uint8_t)GT3R;
        }
    }
    else if(sc_orient == SC_FORWARD)
    {
        if(track == RPT_1)
        {
            result[PRT_LEFT].elevation.spot = SPOT_6;
            result[PRT_RIGHT].elevation.spot = SPOT_5;
            result[PRT_LEFT].elevation.gt = (uint8_t)GT1L;
            result[PRT_RIGHT].elevation.gt = (uint8_t)GT1R;
        }
        else if(track == RPT_2)
        {
            result[PRT_LEFT].elevation.spot = SPOT_4;
            result[PRT_RIGHT].elevation.spot = SPOT_3;
            result[PRT_LEFT].elevation.gt = (uint8_t)GT2L;
            result[PRT_RIGHT].elevation.gt = (uint8_t)GT2R;
        }
        else if(track == RPT_3)
        {
            result[PRT_LEFT].elevation.spot = SPOT_2;
            result[PRT_RIGHT].elevation.spot = SPOT_1;
            result[PRT_LEFT].elevation.gt = (uint8_t)GT3L;
            result[PRT_RIGHT].elevation.gt = (uint8_t)GT3R;
        }
    }
}

/*----------------------------------------------------------------------------
 * iterativeFitStage
 *
 *  Note: Section 5.5 - Signal selection based on ATL03 flags
 *        Procedures 4b and after
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::iterativeFitStage (Atl03Reader::extent_t* extent, result_t* result)
{
    /* Process Tracks */
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        /* Check Valid Extent */
        if(extent->valid[t] && result[t].elevation.photon_count > 0)
        {
            result[t].provided = true;
        }
        else
        {
            // the check for photon count is redundent with the check for
            // a valid extent, but given that the code below is invalid
            // if the number of photons is less than or equal to zero,
            // the check is provided explicitly
            continue;
        }

        /* Initial Conditions */
        bool done = false;
        bool invalid = false;
        int iteration = 0;

        /* Initial Per Track Calculations */
        double pulses_in_extent     = (extent->extent_length[t] * PULSE_REPITITION_FREQUENCY) / extent->spacecraft_velocity[t]; // N_seg_pulses, section 5.4, procedure 1d
        double background_density   = pulses_in_extent * extent->background_rate[t] / (SPEED_OF_LIGHT / 2.0); // BG_density, section 5.7, procedure 1c

        /* Iterate Processing of Photons */
        while(!done)
        {
            int num_photons = result[t].elevation.photon_count;

            /* Calculate Least Squares Fit */
            lsf_t fit = lsf(extent, result[t].photons, num_photons, false);
            result[t].elevation.h_mean = fit.height;
            result[t].elevation.along_track_slope = fit.slope;
            result[t].elevation.h_sigma = fit.y_sigma; // scaled by rms below

            /* Calculate Residuals */
            for(int p = 0; p < num_photons; p++)
            {
                double x = extent->photons[result[t].photons[p].p].distance;
                double y = extent->photons[result[t].photons[p].p].height;
                result[t].photons[p].r = y - (fit.height + (x * fit.slope));
            }

            /* Sort Points by Residuals */
            quicksort(result[t].photons, 0, num_photons-1);

            /* Calculate Inputs to Robust Dispersion Estimate */
            double  background_count;       // N_BG
            double  window_lower_bound;     // zmin
            double  window_upper_bound;     // zmax;
            if(iteration == 0)
            {
                window_lower_bound  = result[t].photons[0].r; // section 5.5, procedure 4c
                window_upper_bound  = result[t].photons[num_photons - 1].r; // section 5.5, procedure 4c
                background_count    = background_density * (window_upper_bound - window_lower_bound); // section 5.5, procedure 4b; pe_select_mod.f90 initial_select()
            }
            else
            {
                background_count    = background_density * result[t].elevation.window_height; // section 5.7, procedure 2c
                window_lower_bound  = -(result[t].elevation.window_height / 2.0); // section 5.7, procedure 2c
                window_upper_bound  = result[t].elevation.window_height / 2.0; // section 5.7, procedure 2c
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
                    if( (((double)i0) + 1.0 - 0.5 + 1.0) < spp )    i0++;   // +1 adjusts for 0 vs 1 based indices, -.5 rounds, +1 looks ahead
                    else                                            break;
                }

                /* Find Smallest Potential Percentiles (1) */
                int32_t i1 = num_photons - 1;
                while(i1 >= 0)
                {
                    double spp = (0.75 * signal_count) + ((result[t].photons[i1].r - window_lower_bound) * background_rate); // section 5.9, procedure 4a
                    if( (((double)i1) + 1.0 - 0.5 - 1.0) > spp )    i1--;   // +1 adjusts for 0 vs 1 based indices, -.5 rounds, +1 looks ahead
                    else                                            break;
                }

                /* Check Need to Refind Percentiles */
                if(i1 < i0)
                {
                    /* Find Spread of Central Values (0) */
                    double spp0 = (num_photons / 2.0) - (signal_count / 4.0); // section 5.9, procedure 5a
                    i0 = (int32_t)(spp0 + 0.5) - 1;

                    /* Find Spread of Central Values (1) */
                    double spp1 = (num_photons / 2.0) + (signal_count / 4.0); // section 5.9, procedure 5b
                    i1 = (int32_t)(spp1 + 0.5);
                }

                /* Check Validity of Percentiles */
                if(i0 >= 0 && i1 < num_photons)
                {
                    /* Calculate Robust Dispersion Estimate */
                    sigma_r = (result[t].photons[i1].r - result[t].photons[i0].r) / RDE_SCALE_FACTOR; // section 5.9, procedure 6
                }
                else
                {
                    mlog(CRITICAL, "Out of bounds condition caught: %d, %d, %d", i0, i1, num_photons);
                    result[t].elevation.pflags |= PFLAG_OUT_OF_BOUNDS;
                    invalid = true;
                }
            }

            /* Calculate Sigma Expected */
            double se1 = pow((SPEED_OF_LIGHT / 2.0) * SIGMA_XMIT, 2);
            double se2 = pow(SIGMA_BEAM, 2) * pow(result[t].elevation.along_track_slope, 2);
            double sigma_expected = sqrt(se1 + se2); // sigma_expected, section 5.5, procedure 4d

            /* Calculate Window Height */
            if(sigma_r > parms->maximum_robust_dispersion) sigma_r = parms->maximum_robust_dispersion;
            double new_window_height = MAX(MAX(parms->minimum_window, 6.0 * sigma_expected), 6.0 * sigma_r); // H_win, section 5.5, procedure 4e
            result[t].elevation.window_height = MAX(new_window_height, 0.75 * result[t].elevation.window_height); // section 5.7, procedure 2e
            double window_spread = result[t].elevation.window_height / 2.0;

            /* Precalculate Next Iteration's Conditions (section 5.7, procedure 2h) */
            int32_t next_num_photons = 0;
            double x_min = DBL_MAX;
            double x_max = DBL_MIN;
            for(int p = 0; p < num_photons; p++)
            {
                if(abs(result[t].photons[p].r) < window_spread)
                {
                    next_num_photons++;
                    double x = extent->photons[result[t].photons[p].p].distance;
                    if(x < x_min) x_min = x;
                    if(x > x_max) x_max = x;
                }
            }

            /* Check Photon Count */
            if(next_num_photons < parms->minimum_photon_count)
            {
                result[t].elevation.pflags |= PFLAG_TOO_FEW_PHOTONS;
                invalid = true;
                done = true;
            }
            /* Check Spread */
            else if((x_max - x_min) < parms->along_track_spread)
            {
                result[t].elevation.pflags |= PFLAG_SPREAD_TOO_SHORT;
                invalid = true;
                done = true;
            }
            /* Check Change in Number of Photons */
            else if(next_num_photons == num_photons)
            {
                done = true;
            }
            /* Check Iterations */
            else if(++iteration >= parms->max_iterations)
            {
                result[t].elevation.pflags |= PFLAG_MAX_ITERATIONS_REACHED;
                done = true;
            }
            /* Filtered Out Photons in Results and Iterate Again (section 5.5, procedure 4f) */
            else
            {
                int32_t ph_in = 0;
                for(int p = 0; p < num_photons; p++)
                {
                    if(abs(result[t].photons[p].r) < window_spread)
                    {
                        result[t].photons[ph_in++] = result[t].photons[p];
                    }
                }
                result[t].elevation.photon_count = ph_in;
            }
        }

        /*
         *  Note: Section 3.6 - Signal, Noise, and Error Estimates
         *        Section 5.7, procedure 5
         */

        /* Sum Deltas in Photon Heights */
        double delta_sum = 0.0;
        for(int p = 0; p < result[t].elevation.photon_count; p++)
        {
            delta_sum += (result[t].photons[p].r * result[t].photons[p].r);
        }

        /* Calculate RMS and Scale h_sigma */
        if(!invalid && result[t].elevation.photon_count > 0)
        {
            result[t].elevation.rms_misfit = sqrt(delta_sum / (double)result[t].elevation.photon_count);
            result[t].elevation.h_sigma = result[t].elevation.rms_misfit * result[t].elevation.h_sigma;
        }
        else
        {
            result[t].elevation.rms_misfit = 0.0;
            result[t].elevation.h_sigma = 0.0;
        }

        /* Calculate Latitude, Longitude, and GPS Time using Least Squares Fit */
        lsf_t fit = lsf(extent, result[t].photons, result[t].elevation.photon_count, true);
        result[t].elevation.latitude = fit.latitude;
        result[t].elevation.longitude = fit.longitude;
        result[t].elevation.delta_time = fit.delta_time;
    }
}

/*----------------------------------------------------------------------------
 * postResult
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::postResult (result_t* result)
{
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        /* Pull Out Elevation */
        elevation_t* elevation = NULL;
        if(result)
        {
            if(result[t].provided)
            {
                elevation = &result[t].elevation;
            }
            else
            {
                stats.filtered_cnt++;
            }
        }

        /* Copy Elevation from Results into Buffer that's Posted */
        elevationMutex.lock();
        {
            /* Populate Elevation */
            if(elevation)
            {
                if(!parms->compact)
                {
                    recData->elevation[elevationIndex++] = *elevation;
                }
                else
                {
                    recCompactData->elevation[elevationIndex].delta_time = elevation->delta_time;
                    recCompactData->elevation[elevationIndex].latitude = elevation->latitude;
                    recCompactData->elevation[elevationIndex].longitude = elevation->longitude;
                    recCompactData->elevation[elevationIndex].h_mean = elevation->h_mean;
                    elevationIndex++;
                }
            }

            /* Check If ATL06 Record Should Be Posted*/
            if((!result && elevationIndex > 0) || elevationIndex == BATCH_SIZE)
            {
                /* Calculate Record Size (according to number of elevations) */
                int size;
                if(!parms->compact) size = elevationIndex * sizeof(elevation_t);
                else                size = elevationIndex * sizeof(elevation_compact_t);

                /* Serialize Record */
                unsigned char* buffer;
                int bufsize = recObj->serialize(&buffer, RecordObject::REFERENCE, size);

                /* Post Record */
                if(outQ->postCopy(buffer, bufsize, SYS_TIMEOUT) > 0)
                {
                    stats.post_success_cnt += elevationIndex;
                }
                else
                {
                    stats.post_dropped_cnt += elevationIndex;
                }

                /* Reset Elevation Index */
                elevationIndex = 0;
            }
        }
        elevationMutex.unlock();

        /* Free Photons Array (allocated in initializationStage) */
        if(result && result[t].photons)
        {
            delete [] result[t].photons;
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
        LuaEngine::setAttrInt(L, "filtered",        lua_obj->stats.filtered_cnt);
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
        mlog(e.level(), "Error configuring %s: %s", LuaMetaName, e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * lsf - least squares fit
 *
 *  Notes:
 *  1. The matrix element notation is row/column; so xxx_12 is the element of
 *     matrix xxx at row 1, column 2
 *  2. If there are multiple elements specified, then the value represents both
 *     elements; so xxx_12_21 the value in matrix xxx of the elements at row 1,
 *     column 2, and row 2, column 1
 *
 * Algorithm:
 *  xi          distance of the photon from the start of the segment
 *  h_mean      height at the center of the segment
 *  dh/dx       along track slope of the segment
 *  n           number of photons in the segment
 *
 *  G = [1, xi]                 # n x 2 matrix of along track photon distances
 *  m = [h_mean, dh/dx]         # 2 x 1 matrix representing the line of best fit
 *  z = [hi]                    # 1 x n matrix of along track photon heights
 *
 *  G^-g = (G^T * G)^-1 * G^T   # 2 x 2 matrix which is the generalized inverse of G
 *  m = G^-g * z                # 1 x 2 matrix containing solution
 *
 *  y_sigma = sqrt((G^-g * G^-gT)[0,0]) # square root of first element (row 0, column 0) of covariance matrix
 *
 *  TODO: currently no protections against divide-by-zero
 *----------------------------------------------------------------------------*/
Atl06Dispatch::lsf_t Atl06Dispatch::lsf (Atl03Reader::extent_t* extent, point_t* array, int size, bool final)
{
    lsf_t fit;

    /* Initialize Fit */
    fit.height = 0.0;
    fit.slope = 0.0;
    fit.y_sigma = 0.0;

    /* Calculate G^T*G and GT*h*/
    double gtg_11 = size;
    double gtg_12_21 = 0.0;
    double gtg_22 = 0.0;
    for(int p = 0; p < size; p++)
    {
        double x = extent->photons[array[p].p].distance;

        /* Perform Matrix Operation */
        gtg_12_21 += x;
        gtg_22 += x * x;
    }

    /* Calculate (G^T*G)^-1 */
    double det = 1.0 / ((gtg_11 * gtg_22) - (gtg_12_21 * gtg_12_21));
    double igtg_11 = gtg_22 * det;
    double igtg_12_21 = -1 * gtg_12_21 * det;
    double igtg_22 = gtg_11 * det;

    if(!final) /* Height */
    {
        /* Calculate G^-g and m */
        for(int p = 0; p < size; p++)
        {
            Atl03Reader::photon_t* ph = &extent->photons[array[p].p];
            double x = ph->distance;
            double y = ph->height;

            /* Perform Matrix Operation */
            double gig_1 = igtg_11 + (igtg_12_21 * x);   // G^-g row 1 element
            double gig_2 = igtg_12_21 + (igtg_22 * x);   // G^-g row 2 element

            /* Calculate m */
            fit.height += gig_1 * y;
            fit.slope += gig_2 * y;

            /* Accumulate y_sigma */
            fit.y_sigma += gig_1 * gig_1;
        }

        /* Calculate y_sigma */
        fit.y_sigma = sqrt(fit.y_sigma);
    }
    else /* Latitude, Longitude, GPS Time */
    {
        fit.latitude = 0.0;
        fit.longitude = 0.0;
        fit.delta_time = 0.0;

        if(size > 0)
        {
            /* Check Need to Shift Longitudes
                    assumes that there isn't a set of photons with
                    longitudes that extend for more than 30 degrees */
            double shift_lon = false;
            double first_lon = extent->photons[array[0].p].longitude;
            if(first_lon < -150.0 || first_lon > 150.0)
            {
                shift_lon = true;
            }

            /* Calculate G^-g and m */
            for(int p = 0; p < size; p++)
            {
                Atl03Reader::photon_t* ph = &extent->photons[array[p].p];
                double x = ph->distance;
                double lat_y = ph->latitude;
                double lon_y = ph->longitude;
                double gps_y = ph->delta_time;

                /* Shift Longitudes */
                if(shift_lon)
                {
                    if(lon_y < 0.0) lon_y = -lon_y;
                    else            lon_y = 360.0 - lon_y;
                }

                /* Perform Matrix Operation */
                double gig_1 = igtg_11 + (igtg_12_21 * x);   // G^-g row 1 element

                /* Calculate m */
                fit.latitude += gig_1 * lat_y;
                fit.longitude += gig_1 * lon_y;
                fit.delta_time += gig_1 * gps_y;
            }

            /* Check if Longitude Needs to be Shifted Back */
            if(shift_lon)
            {
                if(fit.longitude < 180.0)   fit.longitude = -fit.longitude;
                else                        fit.longitude = 360.0 - fit.longitude;
            }

        }
    }

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
