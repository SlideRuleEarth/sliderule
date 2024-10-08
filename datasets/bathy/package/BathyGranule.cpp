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
#include <limits>
#include <stdarg.h>
#include <algorithm>

#include "OsApi.h"
#include "MsgQ.h"
#include "H5Coro.h"
#include "H5Element.h"
#include "BathyFields.h"
#include "BathyGranule.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyGranule::OBJECT_TYPE = "BathyGranule";
const char* BathyGranule::LUA_META_NAME = "BathyGranule";
const struct luaL_Reg BathyGranule::LUA_META_TABLE[] = {
    {"export",      luaExport},
    {NULL,          NULL}
};

/******************************************************************************
 * ATL03 READER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(...)
 *----------------------------------------------------------------------------*/
int BathyGranule::luaCreate (lua_State* L)
{
    BathyFields* _parms = NULL;
    H5Object* _hdf03 = NULL;

    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 1, BathyFields::OBJECT_TYPE));
        _hdf03 = dynamic_cast<H5Object*>(getLuaObject(L, 2, H5Object::OBJECT_TYPE));
        const char* rqstq_name = getLuaString(L, 3);

        /* Return Reader Object */
        return createLuaObject(L, new BathyGranule(L, _parms, _hdf03, rqstq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_hdf03) _hdf03->releaseLuaObject();
        mlog(e.level(), "Error creating BathyGranule: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaExport - export() --> lua table
 *----------------------------------------------------------------------------*/
int BathyGranule::luaExport (lua_State* L)
{
    try
    {
        BathyGranule* lua_obj = dynamic_cast<BathyGranule*>(getLuaSelf(L, 1));
        return lua_obj->toLua(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error exporting %s: %s", OBJECT_TYPE, e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyGranule::BathyGranule (lua_State* L, BathyFields* _parms, H5Object* _hdf03, const char* rqstq_name):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    FieldDictionary({
        {"atlas_sdp_gps_epoch", &atlas_sdp_gps_epoch},
        {"data_end_utc",        &data_end_utc},
        {"data_start_utc",      &data_start_utc},
        {"end_delta_time",      &end_delta_time},
        {"end_geoseg",          &end_geoseg},
        {"end_gpssow",          &end_gpssow},
        {"end_gpsweek",         &end_gpsweek},
        {"end_orbit",           &end_orbit},
        {"release",             &release},
        {"granule_end_utc",     &granule_end_utc},
        {"granule_start_utc",   &granule_start_utc},
        {"start_delta_time",    &start_delta_time},
        {"start_geoseg",        &start_geoseg},
        {"start_gpssow",        &start_gpssow},
        {"start_gpsweek",       &start_gpsweek},
        {"start_orbit",         &start_orbit},
        {"version",             &version},
        {"crossing_time",       &crossing_time},
        {"lan",                 &lan},
        {"orbit_number",        &orbit_number},
        {"sc_orient",           &sc_orient},
        {"sc_orient_time",      &sc_orient_time}
    }),
    parmsPtr(_parms),
    parms(*_parms),
    rqstQ(rqstq_name),
    readTimeoutMs(parms.readTimeout.value * 1000),
    hdf03(_hdf03)
{
    try
    {
        /* Set Thread Specific Trace ID for H5Coro */
        EventLib::stashId (traceId);

        /* Start Reader Thread */
        active = true;
        pid = new Thread(readingThread, this);
    }
    catch(const RunTimeException& e)
    {
        /* Generate Exception Record */
        if(e.code() == RTE_TIMEOUT) alert(e.level(), RTE_TIMEOUT, &rqstQ, &active, "Failure on resource %s: %s", parms.resource.value.c_str(), e.what());
        else alert(e.level(), RTE_RESOURCE_DOES_NOT_EXIST, &rqstQ, &active, "Failure on resource %s: %s", parms.resource.value.c_str(), e.what());

        /* Indicate End of Data */
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyGranule::~BathyGranule (void)
{
    active = false;
    delete pid;
    hdf03->releaseLuaObject();
    parmsPtr->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * readingThread
 *----------------------------------------------------------------------------*/
void* BathyGranule::readingThread (void* parm)
{
    /* Get Thread Info */
    BathyGranule& granule = *static_cast<BathyGranule*>(parm);
    const BathyFields& parms = granule.parms;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, granule.traceId, "bathy_granule", "{\"asset\":\"%s\", \"resource\":\"%s\"}", parms.asset->getName(), parms.resource.value.c_str());
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
        H5Element<double>       atlas_sdp_gps_epoch (granule.hdf03, "/ancillary_data/atlas_sdp_gps_epoch");
        H5Element<const char*>  data_end_utc        (granule.hdf03, "/ancillary_data/data_end_utc");
        H5Element<const char*>  data_start_utc      (granule.hdf03, "/ancillary_data/data_start_utc");
        H5Element<double>       end_delta_time      (granule.hdf03, "/ancillary_data/end_delta_time");
        H5Element<int32_t>      end_geoseg          (granule.hdf03, "/ancillary_data/end_geoseg");
        H5Element<double>       end_gpssow          (granule.hdf03, "/ancillary_data/end_gpssow");
        H5Element<int32_t>      end_gpsweek         (granule.hdf03, "/ancillary_data/end_gpsweek");
        H5Element<int32_t>      end_orbit           (granule.hdf03, "/ancillary_data/end_orbit");
        H5Element<const char*>  release             (granule.hdf03, "/ancillary_data/release");
        H5Element<const char*>  granule_end_utc     (granule.hdf03, "/ancillary_data/granule_end_utc");
        H5Element<const char*>  granule_start_utc   (granule.hdf03, "/ancillary_data/granule_start_utc");
        H5Element<double>       start_delta_time    (granule.hdf03, "/ancillary_data/start_delta_time");
        H5Element<int32_t>      start_geoseg        (granule.hdf03, "/ancillary_data/start_geoseg");
        H5Element<double>       start_gpssow        (granule.hdf03, "/ancillary_data/start_gpssow");
        H5Element<int32_t>      start_gpsweek       (granule.hdf03, "/ancillary_data/start_gpsweek");
        H5Element<int32_t>      start_orbit         (granule.hdf03, "/ancillary_data/start_orbit");
        H5Element<const char*>  version             (granule.hdf03, "/ancillary_data/version");

        H5Element<double>       crossing_time       (granule.hdf03, "/orbit_info/crossing_time");
        H5Element<double>       lan                 (granule.hdf03, "/orbit_info/lan");
        H5Element<int16_t>      orbit_number        (granule.hdf03, "/orbit_info/orbit_number");
        H5Element<int8_t>       sc_orient           (granule.hdf03, "/orbit_info/sc_orient");
        H5Element<double>       sc_orient_time      (granule.hdf03, "/orbit_info/sc_orient_time");

        atlas_sdp_gps_epoch.join(granule.readTimeoutMs, true);
        data_end_utc.join(granule.readTimeoutMs, true);
        data_start_utc.join(granule.readTimeoutMs, true);
        end_delta_time.join(granule.readTimeoutMs, true);
        end_geoseg.join(granule.readTimeoutMs, true);
        end_gpssow.join(granule.readTimeoutMs, true);
        end_gpsweek.join(granule.readTimeoutMs, true);
        end_orbit.join(granule.readTimeoutMs, true);
        release.join(granule.readTimeoutMs, true);
        granule_end_utc.join(granule.readTimeoutMs, true);
        granule_start_utc.join(granule.readTimeoutMs, true);
        start_delta_time.join(granule.readTimeoutMs, true);
        start_geoseg.join(granule.readTimeoutMs, true);
        start_gpssow.join(granule.readTimeoutMs, true);
        start_gpsweek.join(granule.readTimeoutMs, true);
        start_orbit.join(granule.readTimeoutMs, true);
        version.join(granule.readTimeoutMs, true);

        crossing_time.join(granule.readTimeoutMs, true);
        lan.join(granule.readTimeoutMs, true);
        orbit_number.join(granule.readTimeoutMs, true);
        sc_orient.join(granule.readTimeoutMs, true);
        sc_orient_time.join(granule.readTimeoutMs, true);

        granule.atlas_sdp_gps_epoch   = atlas_sdp_gps_epoch.value;
        granule.data_end_utc          = data_end_utc.value;
        granule.data_start_utc        = data_start_utc.value;
        granule.end_delta_time        = end_delta_time.value;
        granule.end_geoseg            = end_geoseg.value;
        granule.end_gpssow            = end_gpssow.value;
        granule.end_gpsweek           = end_gpsweek.value;
        granule.end_orbit             = end_orbit.value;
        granule.release               = release.value;
        granule.granule_end_utc       = granule_end_utc.value;
        granule.granule_start_utc     = granule_start_utc.value;
        granule.start_delta_time      = start_delta_time.value;
        granule.start_geoseg          = start_geoseg.value;
        granule.start_gpssow          = start_gpssow.value;
        granule.start_gpsweek         = start_gpsweek.value;
        granule.start_orbit           = start_orbit.value;
        granule.version               = version.value;

        granule.crossing_time         = crossing_time.value;
        granule.lan                   = lan.value;
        granule.orbit_number          = orbit_number.value;
        granule.sc_orient             = sc_orient.value;
        granule.sc_orient_time        = sc_orient_time.value;
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), e.code(), &granule.rqstQ, &granule.active, "Failure on resource %s: %s", parms.resource.value.c_str(), e.what());
    }

    /* Mark Completion */
    granule.signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
