/*
 * Copyright (c) 2025, University of Washington
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

#include "Gedi02aDataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Gedi02aDataFrame::LUA_META_NAME = "Gedi02aDataFrame";
const struct luaL_Reg Gedi02aDataFrame::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * GEDI02A DATAFRAME CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<beam>, <parms>, <hdf02a>, <outq_name>)
 *----------------------------------------------------------------------------*/
int Gedi02aDataFrame::luaCreate (lua_State* L)
{
    GediFields* _parms = NULL;
    H5Object* _hdf02a = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<GediFields*>(getLuaObject(L, 2, GediFields::OBJECT_TYPE));
        _hdf02a = dynamic_cast<H5Object*>(getLuaObject(L, 3, H5Object::OBJECT_TYPE));
        const char* outq_name = getLuaString(L, 4, true, NULL);

        /* Return DataFrame Object */
        return createLuaObject(L, new Gedi02aDataFrame(L, beam_str, _parms, _hdf02a, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf02a) _hdf02a->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Gedi02aDataFrame::Gedi02aDataFrame (lua_State* L, const char* beam_str, GediFields* _parms, H5Object* _hdf02a, const char* outq_name):
    GediDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"shot_number",         &shot_number},
        {"time_ns",             &time_ns},
        {"latitude",            &latitude},
        {"longitude",           &longitude},
        {"elevation_lm",        &elevation_lm},
        {"elevation_hr",        &elevation_hr},
        {"solar_elevation",     &solar_elevation},
        {"sensitivity",         &sensitivity},
        {"flags",               &flags}
    },
    _parms,
    _hdf02a,
    beam_str,
    outq_name)
{
    /* Call Parent Class Initialization of GeoColumns */
    populateDataframe();

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId (traceId);

    /* Kickoff Reader Thread */
    active.store(true);
    readerPid = new Thread(subsettingThread, this);
}

/*----------------------------------------------------------------------------
 * Gedi02aData::Constructor
 *----------------------------------------------------------------------------*/
Gedi02aDataFrame::Gedi02aData::Gedi02aData (Gedi02aDataFrame* df, const AreaOfInterestGedi& aoi):
    shot_number         (df->hdf, FString("%s/shot_number",        df->group).c_str(), 0, aoi.first_index, aoi.count),
    delta_time          (df->hdf, FString("%s/delta_time",         df->group).c_str(), 0, aoi.first_index, aoi.count),
    elev_lowestmode     (df->hdf, FString("%s/elev_lowestmode",    df->group).c_str(), 0, aoi.first_index, aoi.count),
    elev_highestreturn  (df->hdf, FString("%s/elev_highestreturn", df->group).c_str(), 0, aoi.first_index, aoi.count),
    solar_elevation     (df->hdf, FString("%s/solar_elevation",    df->group).c_str(), 0, aoi.first_index, aoi.count),
    sensitivity         (df->hdf, FString("%s/sensitivity",        df->group).c_str(), 0, aoi.first_index, aoi.count),
    degrade_flag        (df->hdf, FString("%s/degrade_flag",       df->group).c_str(), 0, aoi.first_index, aoi.count),
    quality_flag        (df->hdf, FString("%s/quality_flag",       df->group).c_str(), 0, aoi.first_index, aoi.count),
    surface_flag        (df->hdf, FString("%s/surface_flag",       df->group).c_str(), 0, aoi.first_index, aoi.count),
    anc_data            (df->parms->anc_fields, df->hdf, df->group, H5Coro::ALL_COLS, aoi.first_index, aoi.count)
{
    /* Join Hardcoded Reads */
    shot_number.join(df->readTimeoutMs, true);
    delta_time.join(df->readTimeoutMs, true);
    elev_lowestmode.join(df->readTimeoutMs, true);
    elev_highestreturn.join(df->readTimeoutMs, true);
    solar_elevation.join(df->readTimeoutMs, true);
    sensitivity.join(df->readTimeoutMs, true);
    degrade_flag.join(df->readTimeoutMs, true);
    quality_flag.join(df->readTimeoutMs, true);
    surface_flag.join(df->readTimeoutMs, true);

    /* Join and Add Ancillary Columns */
    anc_data.joinToGDF(df, df->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Gedi02aDataFrame::subsettingThread (void* parm)
{
    Gedi02aDataFrame* df = static_cast<Gedi02aDataFrame*>(parm);
    const GediFields& parms = *df->parms;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, df->traceId, "gedi02a_dataframe", "{\"context\":\"%s\", \"beam\":%s}", df->hdf->name, df->beamStr);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Area of Interest */
        const AreaOfInterestGedi aoi(df->hdf, df->group, "lat_lowestmode", "lon_lowestmode", df->parms, df->readTimeoutMs);

        /* Read GEDI Datasets */
        const Gedi02aData gedi02a(df, aoi);

        /* Traverse All Footprints In Dataset */
        for(long footprint = 0; df->active.load() && footprint < aoi.count; footprint++)
        {
            /* Check Degrade Filter */
            if(parms.degrade_filter)
            {
                if(gedi02a.degrade_flag[footprint])
                {
                    continue;
                }
            }

            /* Check L2 Quality Filter */
            if(parms.l2_quality_filter)
            {
                if(!gedi02a.quality_flag[footprint])
                {
                    continue;
                }
            }

            /* Check Surface Filter */
            if(parms.surface_filter)
            {
                if(!gedi02a.surface_flag[footprint])
                {
                    continue;
                }
            }

            /* Check Region */
            if(aoi.inclusion_ptr)
            {
                if(!aoi.inclusion_ptr[footprint])
                {
                    continue;
                }
            }

            /* Add Row */
            df->addRow();

            /* Populate Columns */
            df->shot_number.append(gedi02a.shot_number[footprint]);
            df->time_ns.append(GediFields::deltatime2timestamp(gedi02a.delta_time[footprint]));
            df->latitude.append(aoi.latitude[footprint]);
            df->longitude.append(aoi.longitude[footprint]);
            df->elevation_lm.append(gedi02a.elev_lowestmode[footprint]);
            df->elevation_hr.append(gedi02a.elev_highestreturn[footprint]);
            df->solar_elevation.append(gedi02a.solar_elevation[footprint]);
            df->sensitivity.append(gedi02a.sensitivity[footprint]);

            uint8_t row_flags = 0;
            if(gedi02a.degrade_flag[footprint]) row_flags |= GediFields::DEGRADE_FLAG_MASK;
            if(gedi02a.quality_flag[footprint]) row_flags |= GediFields::L2_QUALITY_FLAG_MASK;
            if(gedi02a.surface_flag[footprint]) row_flags |= GediFields::SURFACE_FLAG_MASK;
            df->flags.append(row_flags);

            /* Ancillary Data */
            if(gedi02a.anc_data.length() > 0)
            {
                gedi02a.anc_data.addToGDF(df, footprint);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s beam %s: %s", df->hdf->name, df->beamStr, e.what());
    }

    /* Dataframe Complete */
    df->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
