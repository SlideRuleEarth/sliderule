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

#include "OsApi.h"
#include "Gedi04aDataFrame.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Gedi04aDataFrame::LUA_META_NAME = "Gedi04aDataFrame";
const struct luaL_Reg Gedi04aDataFrame::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * GEDI04A DATAFRAME CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<beam>, <parms>, <hdf04a>, <outq_name>)
 *----------------------------------------------------------------------------*/
int Gedi04aDataFrame::luaCreate (lua_State* L)
{
    GediFields* _parms = NULL;
    H5Object* _hdf04a = NULL;

    try
    {
        /* Get Parameters */
        const char* beam_str = getLuaString(L, 1);
        _parms = dynamic_cast<GediFields*>(getLuaObject(L, 2, GediFields::OBJECT_TYPE));
        _hdf04a = dynamic_cast<H5Object*>(getLuaObject(L, 3, H5Object::OBJECT_TYPE));
        const char* outq_name = getLuaString(L, 4, true, NULL);

        /* Return DataFrame Object */
        return createLuaObject(L, new Gedi04aDataFrame(L, beam_str, _parms, _hdf04a, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf04a) _hdf04a->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Gedi04aDataFrame::Gedi04aDataFrame (lua_State* L, const char* beam_str, GediFields* _parms, H5Object* _hdf04a, const char* outq_name):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE,
    {
        {"shot_number",         &shot_number},
        {"time_ns",             &time_ns},
        {"latitude",            &latitude},
        {"longitude",           &longitude},
        {"agbd",                &agbd},
        {"elevation",           &elevation},
        {"solar_elevation",     &solar_elevation},
        {"sensitivity",         &sensitivity},
        {"flags",               &flags}
    },
    {
        {"beam",                &beam},
        {"orbit",               &orbit},
        {"track",               &track},
        {"granule",             &granule}
    },
    "\"EPSG:4326\""),
    beam(0, META_COLUMN),
    orbit(static_cast<uint32_t>(_parms->granule_fields.orbit.value), META_COLUMN),
    track(static_cast<uint16_t>(_parms->granule_fields.track.value), META_COLUMN),
    granule(_hdf04a->name, META_SOURCE_ID),
    active(false),
    readerPid(NULL),
    readTimeoutMs(_parms->readTimeout.value * 1000),
    outQ(NULL),
    parms(_parms),
    hdf04a(_hdf04a),
    dfKey(0),
    beamStr(StringLib::duplicate(beam_str)),
    group{0}
{
    assert(_parms);
    assert(_hdf04a);

    /* Resolve Beam */
    const int beam_index = beamIndexFromString(beam_str);
    StringLib::format(group, sizeof(group), "%s", GediFields::beam2group(beam_index));
    GediFields::beam_t beam_id;
    convertFromIndex(beam_index, beam_id);
    beam = static_cast<uint8_t>(beam_id);

    /* Set DataFrame Key */
    dfKey = static_cast<okey_t>(beam_index);

    /* Optional Output Queue (for messages) */
    if(outq_name) outQ = new Publisher(outq_name);

    /* Call Parent Class Initialization of GeoColumns */
    populateDataframe();

    /* Set Thread Specific Trace ID for H5Coro */
    EventLib::stashId (traceId);

    /* Kickoff Reader Thread */
    active.store(true);
    readerPid = new Thread(subsettingThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Gedi04aDataFrame::~Gedi04aDataFrame (void)
{
    active.store(false);
    delete readerPid;
    delete [] beamStr;
    delete outQ;
    parms->releaseLuaObject();
    hdf04a->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * getKey
 *----------------------------------------------------------------------------*/
okey_t Gedi04aDataFrame::getKey (void) const
{
    return dfKey;
}

/*----------------------------------------------------------------------------
 * Gedi04aData::Constructor
 *----------------------------------------------------------------------------*/
Gedi04aDataFrame::Gedi04aData::Gedi04aData (Gedi04aDataFrame* df, const AreaOfInterestGedi& aoi):
    shot_number     (df->hdf04a, FString("%s/shot_number",     df->group).c_str(), 0, aoi.first_index, aoi.count),
    delta_time      (df->hdf04a, FString("%s/delta_time",      df->group).c_str(), 0, aoi.first_index, aoi.count),
    agbd            (df->hdf04a, FString("%s/agbd",            df->group).c_str(), 0, aoi.first_index, aoi.count),
    elev_lowestmode (df->hdf04a, FString("%s/elev_lowestmode", df->group).c_str(), 0, aoi.first_index, aoi.count),
    solar_elevation (df->hdf04a, FString("%s/solar_elevation", df->group).c_str(), 0, aoi.first_index, aoi.count),
    sensitivity     (df->hdf04a, FString("%s/sensitivity",     df->group).c_str(), 0, aoi.first_index, aoi.count),
    degrade_flag    (df->hdf04a, FString("%s/degrade_flag",    df->group).c_str(), 0, aoi.first_index, aoi.count),
    l2_quality_flag (df->hdf04a, FString("%s/l2_quality_flag", df->group).c_str(), 0, aoi.first_index, aoi.count),
    l4_quality_flag (df->hdf04a, FString("%s/l4_quality_flag", df->group).c_str(), 0, aoi.first_index, aoi.count),
    surface_flag    (df->hdf04a, FString("%s/surface_flag",    df->group).c_str(), 0, aoi.first_index, aoi.count),
    anc_data        (df->parms->anc_fields, df->hdf04a, df->group, H5Coro::ALL_COLS, aoi.first_index, aoi.count)
{
    /* Join Hardcoded Reads */
    shot_number.join(df->readTimeoutMs, true);
    delta_time.join(df->readTimeoutMs, true);
    agbd.join(df->readTimeoutMs, true);
    elev_lowestmode.join(df->readTimeoutMs, true);
    solar_elevation.join(df->readTimeoutMs, true);
    sensitivity.join(df->readTimeoutMs, true);
    degrade_flag.join(df->readTimeoutMs, true);
    l2_quality_flag.join(df->readTimeoutMs, true);
    l4_quality_flag.join(df->readTimeoutMs, true);
    surface_flag.join(df->readTimeoutMs, true);

    /* Join and Add Ancillary Columns */
    anc_data.joinToGDF(df, df->readTimeoutMs, true);
}

/*----------------------------------------------------------------------------
 * subsettingThread
 *----------------------------------------------------------------------------*/
void* Gedi04aDataFrame::subsettingThread (void* parm)
{
    Gedi04aDataFrame* df = static_cast<Gedi04aDataFrame*>(parm);
    const GediFields& parms = *df->parms;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, df->traceId, "gedi04a_dataframe", "{\"context\":\"%s\", \"beam\":%s}", df->hdf04a->name, df->beamStr);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        /* Subset to Area of Interest */
        const AreaOfInterestGedi aoi(df->hdf04a, df->group, "lat_lowestmode", "lon_lowestmode", df->parms, df->readTimeoutMs);

        /* Read GEDI Datasets */
        const Gedi04aData gedi04a(df, aoi);

        /* Traverse All Footprints In Dataset */
        for(long footprint = 0; df->active.load() && footprint < aoi.count; footprint++)
        {
            /* Check Degrade Filter */
            if(parms.degrade_filter)
            {
                if(gedi04a.degrade_flag[footprint])
                {
                    continue;
                }
            }

            /* Check L2 Quality Filter */
            if(parms.l2_quality_filter)
            {
                if(!gedi04a.l2_quality_flag[footprint])
                {
                    continue;
                }
            }

            /* Check L4 Quality Filter */
            if(parms.l4_quality_filter)
            {
                if(!gedi04a.l4_quality_flag[footprint])
                {
                    continue;
                }
            }

            /* Check Surface Filter */
            if(parms.surface_filter)
            {
                if(!gedi04a.surface_flag[footprint])
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
            df->shot_number.append(gedi04a.shot_number[footprint]);
            df->time_ns.append(GediFields::deltatime2timestamp(gedi04a.delta_time[footprint]));
            df->latitude.append(aoi.latitude[footprint]);
            df->longitude.append(aoi.longitude[footprint]);
            df->agbd.append(gedi04a.agbd[footprint]);
            df->elevation.append(gedi04a.elev_lowestmode[footprint]);
            df->solar_elevation.append(gedi04a.solar_elevation[footprint]);
            df->sensitivity.append(gedi04a.sensitivity[footprint]);

            uint8_t row_flags = 0;
            if(gedi04a.degrade_flag[footprint]) row_flags |= GediFields::DEGRADE_FLAG_MASK;
            if(gedi04a.l2_quality_flag[footprint]) row_flags |= GediFields::L2_QUALITY_FLAG_MASK;
            if(gedi04a.l4_quality_flag[footprint]) row_flags |= GediFields::L4_QUALITY_FLAG_MASK;
            if(gedi04a.surface_flag[footprint]) row_flags |= GediFields::SURFACE_FLAG_MASK;
            df->flags.append(row_flags);

            /* Ancillary Data */
            if(gedi04a.anc_data.length() > 0)
            {
                gedi04a.anc_data.addToGDF(df, footprint);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), df->outQ, &df->active, "Failure on resource %s beam %s: %s", df->hdf04a->name, df->beamStr, e.what());
    }

    /* Dataframe Complete */
    df->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
