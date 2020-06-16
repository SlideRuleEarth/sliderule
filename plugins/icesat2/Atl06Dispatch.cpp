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
#include "Hdf5Atl03Handle.h"
#include "MathLib.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

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

    outQ = new Publisher(outq_name);
    LocalLib::set(&stats, 0, sizeof(stats));
    stages = STAGE_AVG;
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
    bool    status = false;
    void*   rsps = NULL;
    int     size = 0;

    /* Bump Statistics */
    stats.h5atl03_rec_cnt++;

    /* Execute Algorithm Stages */
    if(stages == STAGE_AVG)
    {
        double height;
        status = averageHeightStage(record, key, &height);
        rsps = (void*)&height;
        size = sizeof(height);
    }
    else if(stages == STAGE_LSF)
    {
        MathLib::lsf_t fit;
        status = leastSquaresFitStage(record, key, &fit);
        rsps = (void*)&fit.intercept;
        size = sizeof(fit.intercept);
    }

    /* Post Response */
    if(status)
    {
        if(outQ->postCopy(rsps, size, SYS_TIMEOUT) > 0)
        {
            stats.post_success_cnt++;
        }
        else
        {
            stats.post_dropped_cnt++;
            status = false;
        }
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * averageHeightStage
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::averageHeightStage (RecordObject* record, okey_t key, double* height)
{
    (void)key;

    Hdf5Atl03Handle::extent_t* segment = (Hdf5Atl03Handle::extent_t*)record->getRecordData();
    double num_heights = 0.0;
    double height_l = 0.0;
    double height_r = 0.0;

    /* Count Execution Statistic */
    stats.algo_out_cnt[STAGE_AVG]++;

    /* Calculate Left Track Height */
    if(segment->photon_count[PRT_LEFT] > 0)
    {
        num_heights += 1.0;
        for(unsigned int ph = 0; ph < segment->photon_count[PRT_LEFT]; ph++)
        {
            height_l += (segment->photons[ph].height_y / segment->photon_count[PRT_LEFT]);
        }
    }

    /* Calculate Right Track Height */
    if(segment->photon_count[PRT_RIGHT] > 0)
    {
        num_heights += 1.0;
        for(unsigned int ph = segment->photon_count[PRT_LEFT]; ph < (segment->photon_count[PRT_LEFT] + segment->photon_count[PRT_RIGHT]); ph++)
        {
            height_r += (segment->photons[ph].height_y / segment->photon_count[PRT_RIGHT]);
        }
    }

    /* Return Average Track Height */
    *height = (height_l + height_r) / num_heights;
    return true;
}

/*----------------------------------------------------------------------------
 * leastSquaresFitStage
 *----------------------------------------------------------------------------*/
bool Atl06Dispatch::leastSquaresFitStage (RecordObject* record, okey_t key, MathLib::lsf_t* lsf)
{
    (void)key;

    bool status = false;
    Hdf5Atl03Handle::extent_t* segment = (Hdf5Atl03Handle::extent_t*)record->getRecordData();

    /* Count Execution Statistic */
    stats.algo_out_cnt[STAGE_LSF]++;

    /* Calculate Least Squares Fit */
    if(segment->photon_count[PRT_LEFT] > 0)
    {
        *lsf = MathLib::lsf((MathLib::point_t*)&segment->photons[0], segment->photon_count[PRT_LEFT]);
        status = true;
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
        LuaEngine::setAttrInt(L, "h5atl03_rec_cnt",         lua_obj->stats.h5atl03_rec_cnt);
        LuaEngine::setAttrInt(L, "avgheight_out_cnt",       lua_obj->stats.algo_out_cnt[STAGE_AVG]);
        LuaEngine::setAttrInt(L, "leastsquares_out_cnt",    lua_obj->stats.algo_out_cnt[STAGE_LSF]);
        LuaEngine::setAttrInt(L, "post_success_cnt",        lua_obj->stats.post_success_cnt);
        LuaEngine::setAttrInt(L, "post_dropped_cnt",        lua_obj->stats.post_dropped_cnt);

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
 * luaSelect - :select(<algorithm stage>)
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

        /* Set Stage */
        if(algo_stage >= 0 && algo_stage < NUM_STAGES)
        {
            mlog(INFO, "Selecting stage: %d\n", algo_stage);
            lua_obj->stages = (stages_t)algo_stage;
            status = true;
        }
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error selecting algorithm stage: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
