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

const char* Atl06Dispatch::elRecType = "atl06rec.elevation"; // extended elevation measurement record
const RecordObject::fieldDef_t Atl06Dispatch::elRecDef[] = {
    {"extent_id",               RecordObject::UINT64,   offsetof(elevation_t, extent_id),           1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"segment_id",              RecordObject::UINT32,   offsetof(elevation_t, segment_id),          1,  NULL, NATIVE_FLAGS},
    {"n_fit_photons",           RecordObject::INT32,    offsetof(elevation_t, photon_count),        1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"pflags",                  RecordObject::UINT16,   offsetof(elevation_t, pflags),              1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"rgt",                     RecordObject::UINT16,   offsetof(elevation_t, rgt),                 1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"cycle",                   RecordObject::UINT8,    offsetof(elevation_t, cycle),               1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"region",                  RecordObject::UINT8,    offsetof(elevation_t, region),              1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"spot",                    RecordObject::UINT8,    offsetof(elevation_t, spot),                1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"gt",                      RecordObject::UINT8,    offsetof(elevation_t, gt),                  1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"time",                    RecordObject::TIME8,    offsetof(elevation_t, time_ns),             1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"latitude",                RecordObject::DOUBLE,   offsetof(elevation_t, latitude),            1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"longitude",               RecordObject::DOUBLE,   offsetof(elevation_t, longitude),           1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"h_mean",                  RecordObject::DOUBLE,   offsetof(elevation_t, h_mean),              1,  NULL, NATIVE_FLAGS | RecordObject::Z_COORD},
    {"dh_fit_dx",               RecordObject::FLOAT,    offsetof(elevation_t, dh_fit_dx),           1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"x_atc",                   RecordObject::FLOAT,    offsetof(elevation_t, x_atc),               1,  NULL, NATIVE_FLAGS},
    {"y_atc",                   RecordObject::FLOAT,    offsetof(elevation_t, y_atc),               1,  NULL, NATIVE_FLAGS},
    {"w_surface_window_final",  RecordObject::FLOAT,    offsetof(elevation_t, window_height),       1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"rms_misfit",              RecordObject::FLOAT,    offsetof(elevation_t, rms_misfit),          1,  NULL, NATIVE_FLAGS | RecordObject::AUX},
    {"h_sigma",                 RecordObject::FLOAT,    offsetof(elevation_t, h_sigma),             1,  NULL, NATIVE_FLAGS | RecordObject::AUX}
};

const char* Atl06Dispatch::atRecType = "atl06rec";
const RecordObject::fieldDef_t Atl06Dispatch::atRecDef[] = {
    {"elevation",               RecordObject::USER,     offsetof(atl06_t, elevation),               0,  elRecType, NATIVE_FLAGS | RecordObject::BATCH}
};

/* Lua Functions */
const char* Atl06Dispatch::LUA_META_NAME = "Atl06Dispatch";
const struct luaL_Reg Atl06Dispatch::LUA_META_TABLE[] = {
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :atl06(<outq name>, <parms>)
 *----------------------------------------------------------------------------*/
int Atl06Dispatch::luaCreate (lua_State* L)
{
    Icesat2Parms* parms = NULL;
    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);
        parms = dynamic_cast<Icesat2Parms*>(getLuaObject(L, 2, Icesat2Parms::OBJECT_TYPE));

        /* Create ATL06 Dispatch */
        return createLuaObject(L, new Atl06Dispatch(L, outq_name, parms));
    }
    catch(const RunTimeException& e)
    {
        if(parms) parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::init (void)
{
    /*
     * Note: the size associated with these records includes no elements;
     * this forces any software accessing an element to manage the size
     * of the record manually.
     */
    RECDEF(elRecType,       elRecDef,       sizeof(elevation_t),                NULL);
    RECDEF(atRecType,       atRecDef,       offsetof(atl06_t, elevation),       NULL);
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl06Dispatch::Atl06Dispatch (lua_State* L, const char* outq_name, Icesat2Parms* _parms):
    DispatchObject(L, LUA_META_NAME, LUA_META_TABLE),
    elevationRecord(atRecType, sizeof(atl06_t))
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
    elevationRecordData = reinterpret_cast<atl06_t*>(elevationRecord.getRecordData());
    ancillaryTotalSize = 0;

    /* Initialize Publisher */
    outQ = new Publisher(outq_name);
    elevationIndex = 0;
    ancillaryIndex = 0;

    /* Initialize Statistics */
    stats.h5atl03_rec_cnt = 0;
    stats.filtered_cnt = 0;
    stats.post_success_cnt = 0;
    stats.post_dropped_cnt = 0;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
Atl06Dispatch::~Atl06Dispatch(void)
{
    delete outQ;
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::processRecord (RecordObject* record, okey_t key, recVec_t* records)
{
    (void)key;

    /* Declare and Clear Results */
    result_t result;
    result.provided = false;
    result.photons = NULL;

    /* Get Input */
    Atl03Reader::extent_t* extent = reinterpret_cast<Atl03Reader::extent_t*>(record->getRecordData());

    /* Build Ancillary Inputs */
    if(records)
    {
        for(size_t i = 1; i < records->size(); i++) // start at one to skip atl03rec
        {
            RecordObject* rec = records->at(i);
            AncillaryFields::element_array_t* anc_rec = reinterpret_cast<AncillaryFields::element_array_t*>(rec->getRecordData());

            /* Build Array of Values
                * to be used by iterativeFitStage..lsf */
            double* values = AncillaryFields::extractAsDoubles(anc_rec); // `new` memory allocated here
            result.anc_values.push_back(values);

            /* Prepopulate Ancillary Field Structure
                * `value` is populated below in iterativeFitStage..lsf
                * using the value vector above */
            AncillaryFields::field_t anc_field;
            anc_field.anc_type = anc_rec->anc_type;
            anc_field.field_index = anc_rec->field_index;
            anc_field.data_type = RecordObject::DOUBLE;
            result.anc_fields.push_back(anc_field);
        }
    }

    /* Elevation Attributes */
    result.elevation.extent_id = extent->extent_id | Icesat2Parms::EXTENT_ID_ELEVATION;
    result.elevation.segment_id = extent->segment_id;
    result.elevation.rgt = extent->reference_ground_track;
    result.elevation.cycle = extent->cycle;
    result.elevation.region = extent->region;
    result.elevation.x_atc = extent->segment_distance;
    result.elevation.pflags = 0;

    /* Copy In Initial Set of Photons */
    result.elevation.photon_count = extent->photon_count;
    if(result.elevation.photon_count > 0)
    {
        result.photons = new point_t[result.elevation.photon_count];
        for(int p = 0; p < result.elevation.photon_count; p++)
        {
            result.photons[p].p = p;  // extent->photons[]
        }
    }

    /* Calcualte Beam Numbers */
    result.elevation.spot = Icesat2Parms::getSpotNumber((Icesat2Parms::sc_orient_t)extent->spacecraft_orientation, (Icesat2Parms::track_t)extent->track, extent->pair);
    result.elevation.gt = Icesat2Parms::getGroundTrack((Icesat2Parms::sc_orient_t)extent->spacecraft_orientation, (Icesat2Parms::track_t)extent->track, extent->pair);

    /* Execute Algorithm Stages */
    if(parms->stages[Icesat2Parms::STAGE_LSF]) iterativeFitStage(extent, result);

    /* Post Results */
    postResult(&result);

    /* Delete Ancillary Value Arrays */
    for(const auto& values: result.anc_values)
    {
        delete [] values;
    }

    /* Delete Photon Aray */
    delete [] result.photons;

    /* Bump Statistics */
    stats.h5atl03_rec_cnt++;

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
 * iterativeFitStage
 *
 *  Note: Section 5.5 - Signal selection based on ATL03 flags
 *        Procedures 4b and after
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::iterativeFitStage (Atl03Reader::extent_t* extent, result_t& result)
{
    /* Check Valid Extent */
    if(result.elevation.photon_count <= 0)
    {
        return;
    }

    /* Result is Provided */
    result.provided = true;

    /* Initial Conditions */
    bool done = false;
    bool invalid = false;
    int iteration = 0;

    /* Initial Per Track Calculations */
    const double pulses_in_extent     = (extent->extent_length * PULSE_REPITITION_FREQUENCY) / extent->spacecraft_velocity; // N_seg_pulses, section 5.4, procedure 1d
    const double background_density   = pulses_in_extent * extent->background_rate / (SPEED_OF_LIGHT / 2.0); // BG_density, section 5.7, procedure 1c

    /* Iterate Processing of Photons */
    while(!done)
    {
        const int num_photons = result.elevation.photon_count;

        /* Calculate Least Squares Fit */
        const lsf_t fit = lsf(extent, result, false);

        /* Calculate Residuals */
        for(int p = 0; p < num_photons; p++)
        {
            const double x = extent->photons[result.photons[p].p].x_atc;
            const double y = extent->photons[result.photons[p].p].height;
            result.photons[p].r = y - (fit.height + (x * fit.slope));
        }

        /* Sort Points by Residuals */
        quicksort(result.photons, 0, num_photons-1);

        /* Calculate Inputs to Robust Dispersion Estimate */
        double  background_count;       // N_BG
        double  window_lower_bound;     // zmin
        double  window_upper_bound;     // zmax;
        if(iteration == 0)
        {
            window_lower_bound  = result.photons[0].r; // section 5.5, procedure 4c
            window_upper_bound  = result.photons[num_photons - 1].r; // section 5.5, procedure 4c
            background_count    = background_density * (window_upper_bound - window_lower_bound); // section 5.5, procedure 4b; pe_select_mod.f90 initial_select()
        }
        else
        {
            background_count    = background_density * result.elevation.window_height; // section 5.7, procedure 2c
            window_lower_bound  = -(result.elevation.window_height / 2.0); // section 5.7, procedure 2c
            window_upper_bound  = result.elevation.window_height / 2.0; // section 5.7, procedure 2c
        }

        /* Continued Inputs to Robust Dispersion Estimate */
        const double background_rate  = background_count / (window_upper_bound - window_lower_bound); // bckgrd, section 5.9, procedure 1a
        const double signal_count     = num_photons - background_count; // N_sig, section 5.9, procedure 1b
        double sigma_r                = 0.0; // sigma_r

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
                const double spp = (0.25 * signal_count) + ((result.photons[i0].r - window_lower_bound) * background_rate); // section 5.9, procedure 4a
                if( (((double)i0) + 1.0 - 0.5 + 1.0) < spp )    i0++;   // +1 adjusts for 0 vs 1 based indices, -.5 rounds, +1 looks ahead
                else                                            break;
            }

            /* Find Smallest Potential Percentiles (1) */
            int32_t i1 = num_photons - 1;
            while(i1 >= 0)
            {
                const double spp = (0.75 * signal_count) + ((result.photons[i1].r - window_lower_bound) * background_rate); // section 5.9, procedure 4a
                if( (((double)i1) + 1.0 - 0.5 - 1.0) > spp )    i1--;   // +1 adjusts for 0 vs 1 based indices, -.5 rounds, +1 looks ahead
                else                                            break;
            }

            /* Check Need to Refind Percentiles */
            if(i1 < i0)
            {
                /* Find Spread of Central Values (0) */
                const double spp0 = (num_photons / 2.0) - (signal_count / 4.0); // section 5.9, procedure 5a
                i0 = (int32_t)(spp0 + 0.5) - 1;

                /* Find Spread of Central Values (1) */
                const double spp1 = (num_photons / 2.0) + (signal_count / 4.0); // section 5.9, procedure 5b
                i1 = (int32_t)(spp1 + 0.5);
            }

            /* Check Validity of Percentiles */
            if(i0 >= 0 && i1 < num_photons)
            {
                /* Calculate Robust Dispersion Estimate */
                sigma_r = (result.photons[i1].r - result.photons[i0].r) / RDE_SCALE_FACTOR; // section 5.9, procedure 6
            }
            else
            {
                mlog(CRITICAL, "Out of bounds condition caught: %d, %d, %d", i0, i1, num_photons);
                result.elevation.pflags |= PFLAG_OUT_OF_BOUNDS;
                invalid = true;
            }
        }

        /* Calculate Sigma Expected */
        const double se1 = pow((SPEED_OF_LIGHT / 2.0) * SIGMA_XMIT, 2);
        const double se2 = pow(SIGMA_BEAM, 2) * pow(result.elevation.dh_fit_dx, 2);
        const double sigma_expected = sqrt(se1 + se2); // sigma_expected, section 5.5, procedure 4d

        /* Calculate Window Height */
        if(sigma_r > parms->maximum_robust_dispersion) sigma_r = parms->maximum_robust_dispersion;
        const double new_window_height = MAX(MAX(parms->minimum_window, 6.0 * sigma_expected), 6.0 * sigma_r); // H_win, section 5.5, procedure 4e
        result.elevation.window_height = MAX(new_window_height, 0.75 * result.elevation.window_height); // section 5.7, procedure 2e
        const double window_spread = result.elevation.window_height / 2.0;

        /* Precalculate Next Iteration's Conditions (section 5.7, procedure 2h) */
        int32_t next_num_photons = 0;
        double x_min = DBL_MAX;
        double x_max = -DBL_MAX;
        for(int p = 0; p < num_photons; p++)
        {
            if(abs(result.photons[p].r) < window_spread)
            {
                next_num_photons++;
                const double x = extent->photons[result.photons[p].p].x_atc;
                if(x < x_min) x_min = x;
                if(x > x_max) x_max = x;
            }
        }

        /* Check Photon Count */
        if(next_num_photons < parms->minimum_photon_count)
        {
            result.elevation.pflags |= PFLAG_TOO_FEW_PHOTONS;
            invalid = true;
            done = true;
        }
        /* Check Spread */
        else if((x_max - x_min) < parms->along_track_spread)
        {
            result.elevation.pflags |= PFLAG_SPREAD_TOO_SHORT;
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
            result.elevation.pflags |= PFLAG_MAX_ITERATIONS_REACHED;
            done = true;
        }
        /* Filtered Out Photons in Results and Iterate Again (section 5.5, procedure 4f) */
        else
        {
            int32_t ph_in = 0;
            for(int p = 0; p < num_photons; p++)
            {
                if(abs(result.photons[p].r) < window_spread)
                {
                    result.photons[ph_in++] = result.photons[p];
                }
            }
            result.elevation.photon_count = ph_in;
        }
    }

    /*
        *  Note: Section 3.6 - Signal, Noise, and Error Estimates
        *        Section 5.7, procedure 5
        */

    /* Sum Deltas in Photon Heights */
    double delta_sum = 0.0;
    for(int p = 0; p < result.elevation.photon_count; p++)
    {
        delta_sum += (result.photons[p].r * result.photons[p].r);
    }

    /* Calculate RMS and Scale h_sigma */
    if(!invalid && result.elevation.photon_count > 0)
    {
        result.elevation.rms_misfit = sqrt(delta_sum / (double)result.elevation.photon_count);
        result.elevation.h_sigma = result.elevation.rms_misfit * result.elevation.h_sigma;
    }
    else
    {
        result.elevation.rms_misfit = 0.0;
        result.elevation.h_sigma = 0.0;
    }

    /* Calculate Latitude, Longitude, and GPS Time using Least Squares Fit */
    lsf(extent, result, true);
}

/*----------------------------------------------------------------------------
 * postResult
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::postResult (result_t* result)
{
    /* Copy Elevation from Results into Buffer that's Posted */
    postingMutex.lock();
    {
        /* Populate Elevation & Ancillary Fields */
        if(result && result->provided)
        {
            /* Elevation */
            elevationRecordData->elevation[elevationIndex++] = result->elevation;

            /* Ancillary */
            ancillaryRecords[ancillaryIndex] = AncillaryFields::createFieldArrayRecord(result->elevation.extent_id, result->anc_fields);
            if(ancillaryRecords[ancillaryIndex])
            {
                ancillaryTotalSize += ancillaryRecords[ancillaryIndex]->getUsedMemory();
                ancillaryIndex++;
            }
        }
        else
        {
            stats.filtered_cnt++;
        }

        /* Check If ATL06 Record Should Be Posted*/
        if((!result && elevationIndex > 0) || elevationIndex == BATCH_SIZE)
        {
            /* Adjust Size of Elevation Record */
            const int elevation_rec_size = elevationIndex * sizeof(elevation_t);
            elevationRecord.setUsedData(elevation_rec_size);

            /* Serialize Record(s) */
            ContainerRecord* container = NULL;
            unsigned char* buffer = NULL;
            int bufsize = 0;

            if(ancillaryIndex == 0)
            {
                /* Serialize Elevation Batch Record */
                bufsize = elevationRecord.serialize(&buffer, RecordObject::REFERENCE);
            }
            else // send container record
            {
                /* Build Container Record: num anc rec + one elev rec, total anc size + elev rec size */
                container = new ContainerRecord(ancillaryIndex + 1, ancillaryTotalSize + elevationRecord.getUsedMemory());
                container->addRecord(elevationRecord, elevation_rec_size);
                for(int i = 0; i < ancillaryIndex; i++)
                {
                    container->addRecord(*ancillaryRecords[i]);
                    delete ancillaryRecords[i];
                }

                /* Serialize Elevation Batch Record */
                bufsize = container->serialize(&buffer, RecordObject::REFERENCE);
            }

            /* Post Record */
            int post_status = MsgQ::STATE_TIMEOUT;
            while((post_status = outQ->postCopy(buffer, bufsize, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT);
            if(post_status > 0)
            {
                stats.post_success_cnt += elevationIndex;
            }
            else
            {
                stats.post_dropped_cnt += elevationIndex;
            }

            /* Clean Up Container (if necessary) */
            delete container;

            /* Reset Indices */
            elevationIndex = 0;
            ancillaryIndex = 0;
            ancillaryTotalSize = 0;
        }
    }
    postingMutex.unlock();
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
        Atl06Dispatch* lua_obj = dynamic_cast<Atl06Dispatch*>(getLuaSelf(L, 1));

        /* Get Clear Parameter */
        const bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "read",        lua_obj->stats.h5atl03_rec_cnt);
        LuaEngine::setAttrInt(L, "filtered",    lua_obj->stats.filtered_cnt);
        LuaEngine::setAttrInt(L, "sent",        lua_obj->stats.post_success_cnt);
        LuaEngine::setAttrInt(L, "dropped",     lua_obj->stats.post_dropped_cnt);

        /* Optionally Clear */
        if(with_clear)
        {
            lua_obj->stats.h5atl03_rec_cnt = 0;
            lua_obj->stats.filtered_cnt = 0;
            lua_obj->stats.post_success_cnt = 0;
            lua_obj->stats.post_dropped_cnt = 0;
        }

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error configuring %s: %s", LUA_META_NAME, e.what());
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
Atl06Dispatch::lsf_t Atl06Dispatch::lsf (Atl03Reader::extent_t* extent, result_t& result, bool final)
{
    lsf_t fit;
    point_t* array = result.photons;
    const int size = result.elevation.photon_count;

    /* Initialize Fit */
    fit.height = 0.0;
    fit.slope = 0.0;
    fit.y_sigma = 0.0;

    /* Calculate G^T*G and GT*h*/
    const double gtg_11 = size;
    double       gtg_12_21 = 0.0;
    double       gtg_22 = 0.0;
    for(int p = 0; p < size; p++)
    {
        const double x = extent->photons[array[p].p].x_atc;

        /* Perform Matrix Operation */
        gtg_12_21 += x;
        gtg_22 += x * x;
    }

    /* Calculate (G^T*G)^-1 */
    const double det = 1.0 / ((gtg_11 * gtg_22) - (gtg_12_21 * gtg_12_21));
    const double igtg_11 = gtg_22 * det;
    const double igtg_12_21 = -1 * gtg_12_21 * det;
    const double igtg_22 = gtg_11 * det;

    if(!final) /* Height */
    {
        /* Calculate G^-g and m */
        for(int p = 0; p < size; p++)
        {
            const Atl03Reader::photon_t* ph = &extent->photons[array[p].p];
            const double x = ph->x_atc;
            const double y = ph->height;

            /* Perform Matrix Operation */
            const double gig_1 = igtg_11 + (igtg_12_21 * x);   // G^-g row 1 element
            const double gig_2 = igtg_12_21 + (igtg_22 * x);   // G^-g row 2 element

            /* Calculate m */
            fit.height += gig_1 * y;
            fit.slope += gig_2 * y;

            /* Accumulate y_sigma */
            fit.y_sigma += gig_1 * gig_1;
        }

        /* Calculate y_sigma */
        fit.y_sigma = sqrt(fit.y_sigma);

        /* Populate Results */
        result.elevation.h_mean = fit.height;
        result.elevation.dh_fit_dx = fit.slope;
        result.elevation.h_sigma = fit.y_sigma; // scaled by rms below
    }
    else /* Latitude, Longitude, GPS Time, Across Track Coordinate, Ancillary Fields */
    {
        if(size > 0)
        {
            double latitude = 0.0;
            double longitude = 0.0;
            double time_ns = 0.0;
            double y_atc = 0.0;

            /* Check Need to Shift Longitudes
               assumes that there isn't a set of photons with
               longitudes that extend for more than 30 degrees */
            bool shift_lon = false;
            const double first_lon = extent->photons[array[0].p].longitude;
            if(first_lon < -150.0 || first_lon > 150.0)
            {
                shift_lon = true;
            }

            /* Fixed Fields - Calculate G^-g and m */
            for(int p = 0; p < size; p++)
            {
                const Atl03Reader::photon_t* ph = &extent->photons[array[p].p];
                double ph_longitude = ph->longitude;

                /* Shift Longitudes */
                if(shift_lon) ph_longitude = fmod((ph_longitude + 360.0), 360.0);

                /* Perform Matrix Operation */
                const double gig_1 = igtg_11 + (igtg_12_21 * ph->x_atc);   // G^-g row 1 element

                /* Calculate m */
                latitude += gig_1 * ph->latitude;
                longitude += gig_1 * ph_longitude;
                time_ns += gig_1 * ph->time_ns;
                y_atc += gig_1 * ph->y_atc;
            }

            /* Check if Longitude Needs to be Shifted Back */
            if(shift_lon) longitude = fmod((longitude + 180.0), 360.0) - 180.0;

            /* Populate Results */
            result.elevation.latitude = latitude;
            result.elevation.longitude = longitude;
            result.elevation.time_ns = (int64_t)time_ns;
            result.elevation.y_atc = (float)y_atc;

            /* Ancillary Fields - Calculate G^-g and m */
            for(size_t a = 0; a < result.anc_values.size(); a++)
            {
                const double* values = result.anc_values[a];
                double value = 0;
                for(int p = 0; p < size; p++)
                {
                    const Atl03Reader::photon_t* ph = &extent->photons[array[p].p];
                    const double gig_1 = igtg_11 + (igtg_12_21 * ph->x_atc);   // G^-g row 1 element
                    value += gig_1 * values[p];
                }
                AncillaryFields::setValueAsDouble(&result.anc_fields[a], value);
            }
        }
    }

    /* Return Fit */
    return fit;
}

/*----------------------------------------------------------------------------
 * quicksort
 *----------------------------------------------------------------------------*/
void Atl06Dispatch::quicksort(point_t* array, int start, int end) // NOLINT(misc-no-recursion)
{
    if(start < end)
    {
        const int partition = quicksortpartition(array, start, end);
        quicksort(array, start, partition);
        quicksort(array, partition + 1, end);
    }
}

/*----------------------------------------------------------------------------
 * quicksortpartition
 *----------------------------------------------------------------------------*/
int Atl06Dispatch::quicksortpartition(point_t* array, int start, int end)
{
    const double pivot = array[(start + end) / 2].r;

    start--;
    end++;
    while(true)
    {
        while (array[++start].r < pivot);
        while (array[--end].r > pivot);
        if (start >= end) return end;

        const point_t tmp = array[start];
        array[start] = array[end];
        array[end] = tmp;
    }
}
