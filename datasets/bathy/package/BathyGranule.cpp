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

    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<BathyFields*>(getLuaObject(L, 1, BathyFields::OBJECT_TYPE));
        const char* rqstq_name = getLuaString(L, 2);

        /* Return Reader Object */
        return createLuaObject(L, new BathyGranule(L, _parms, rqstq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
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
BathyGranule::BathyGranule (lua_State* L, BathyFields* _parms, const char* rqstq_name):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    FieldDictionary( 
    {   
        {"year",                    &year},
        {"month",                   &month},
        {"day",                     &day},
        {"rgt",                     &rgt},
        {"cycle",                   &cycle},
        {"region",                  &region}
    }),
    parmsPtr(_parms),
    parms(*_parms),
    rqstQ(rqstq_name),
    readTimeoutMs(parms.readTimeout.value * 1000),
    context(NULL)
{
    try
    {
        /* Set Thread Specific Trace ID for H5Coro */
        EventLib::stashId (traceId);

        /* Create H5Coro Contexts */
        context = new H5Coro::Context(parms.asset, parms.resource.value.c_str());

        /* Parse Globals (throws) */
        TimeLib::date_t granule_date;
        uint16_t granule_rgt;
        uint8_t granule_cycle;
        uint8_t granule_region;
        parseResource(parms.resource.value.c_str(), granule_date, granule_rgt, granule_cycle, granule_region);
        year = granule_date.year;
        month = granule_date.month;
        day = granule_date.day;
        rgt = granule_rgt;
        cycle = granule_cycle;
        region = granule_region;
        
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
    delete context;
    if(parmsPtr) parmsPtr->releaseLuaObject();
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
        H5Element<double>       atlas_sdp_gps_epoch (granule.context, "/ancillary_data/atlas_sdp_gps_epoch");
        H5Element<const char*>  data_end_utc        (granule.context, "/ancillary_data/data_end_utc");
        H5Element<const char*>  data_start_utc      (granule.context, "/ancillary_data/data_start_utc");
        H5Element<int32_t>      end_cycle           (granule.context, "/ancillary_data/end_cycle");
        H5Element<double>       end_delta_time      (granule.context, "/ancillary_data/end_delta_time");
        H5Element<int32_t>      end_geoseg          (granule.context, "/ancillary_data/end_geoseg");
        H5Element<double>       end_gpssow          (granule.context, "/ancillary_data/end_gpssow");
        H5Element<int32_t>      end_gpsweek         (granule.context, "/ancillary_data/end_gpsweek");
        H5Element<int32_t>      end_orbit           (granule.context, "/ancillary_data/end_orbit");
        H5Element<int32_t>      end_region          (granule.context, "/ancillary_data/end_region");
        H5Element<int32_t>      end_rgt             (granule.context, "/ancillary_data/end_rgt");
        H5Element<const char*>  release             (granule.context, "/ancillary_data/release");
        H5Element<const char*>  granule_end_utc     (granule.context, "/ancillary_data/granule_end_utc");
        H5Element<const char*>  granule_start_utc   (granule.context, "/ancillary_data/granule_start_utc");
        H5Element<int32_t>      start_cycle         (granule.context, "/ancillary_data/start_cycle");
        H5Element<double>       start_delta_time    (granule.context, "/ancillary_data/start_delta_time");
        H5Element<int32_t>      start_geoseg        (granule.context, "/ancillary_data/start_geoseg");
        H5Element<double>       start_gpssow        (granule.context, "/ancillary_data/start_gpssow");
        H5Element<int32_t>      start_gpsweek       (granule.context, "/ancillary_data/start_gpsweek");
        H5Element<int32_t>      start_orbit         (granule.context, "/ancillary_data/start_orbit");
        H5Element<int32_t>      start_region        (granule.context, "/ancillary_data/start_region");
        H5Element<int32_t>      start_rgt           (granule.context, "/ancillary_data/start_rgt");
        H5Element<const char*>  version             (granule.context, "/ancillary_data/version");

        H5Element<double>       crossing_time       (granule.context, "/orbit_info/crossing_time");
        H5Element<int8_t>       cycle_number        (granule.context, "/orbit_info/cycle_number");
        H5Element<double>       lan                 (granule.context, "/orbit_info/lan"); 
        H5Element<int16_t>      orbit_number        (granule.context, "/orbit_info/orbit_number");
        H5Element<int16_t>      rgt                 (granule.context, "/orbit_info/rgt"); 
        H5Element<int8_t>       sc_orient           (granule.context, "/orbit_info/sc_orient");
        H5Element<double>       sc_orient_time      (granule.context, "/orbit_info/sc_orient_time");

        atlas_sdp_gps_epoch.join(granule.readTimeoutMs, true);
        data_end_utc.join(granule.readTimeoutMs, true);
        data_start_utc.join(granule.readTimeoutMs, true);
        end_cycle.join(granule.readTimeoutMs, true);
        end_delta_time.join(granule.readTimeoutMs, true);
        end_geoseg.join(granule.readTimeoutMs, true);
        end_gpssow.join(granule.readTimeoutMs, true);
        end_gpsweek.join(granule.readTimeoutMs, true);
        end_orbit.join(granule.readTimeoutMs, true);
        end_region.join(granule.readTimeoutMs, true);
        end_rgt.join(granule.readTimeoutMs, true);
        release.join(granule.readTimeoutMs, true);
        granule_end_utc.join(granule.readTimeoutMs, true);
        granule_start_utc.join(granule.readTimeoutMs, true);
        start_cycle.join(granule.readTimeoutMs, true);
        start_delta_time.join(granule.readTimeoutMs, true);
        start_geoseg.join(granule.readTimeoutMs, true);
        start_gpssow.join(granule.readTimeoutMs, true);
        start_gpsweek.join(granule.readTimeoutMs, true);
        start_orbit.join(granule.readTimeoutMs, true);
        start_region.join(granule.readTimeoutMs, true);
        start_rgt.join(granule.readTimeoutMs, true);
        version.join(granule.readTimeoutMs, true);

        crossing_time.join(granule.readTimeoutMs, true);
        cycle_number.join(granule.readTimeoutMs, true);
        lan.join(granule.readTimeoutMs, true);
        orbit_number.join(granule.readTimeoutMs, true);
        rgt.join(granule.readTimeoutMs, true);
        sc_orient.join(granule.readTimeoutMs, true);
        sc_orient_time.join(granule.readTimeoutMs, true);

        granule.atlas_sdp_gps_epoch   = atlas_sdp_gps_epoch.value;
        granule.data_end_utc          = data_end_utc.value;
        granule.data_start_utc        = data_start_utc.value;
        granule.end_cycle             = end_cycle.value;
        granule.end_delta_time        = end_delta_time.value;
        granule.end_geoseg            = end_geoseg.value;
        granule.end_gpssow            = end_gpssow.value;
        granule.end_gpsweek           = end_gpsweek.value;
        granule.end_orbit             = end_orbit.value;
        granule.end_region            = end_region.value;
        granule.end_rgt               = end_rgt.value;
        granule.release               = release.value;
        granule.granule_end_utc       = granule_end_utc.value;
        granule.granule_start_utc     = granule_start_utc.value;
        granule.start_cycle           = start_cycle.value;
        granule.start_delta_time      = start_delta_time.value;
        granule.start_geoseg          = start_geoseg.value;
        granule.start_gpssow          = start_gpssow.value;
        granule.start_gpsweek         = start_gpsweek.value;
        granule.start_orbit           = start_orbit.value;
        granule.start_region          = start_region.value;
        granule.start_rgt             = start_rgt.value;
        granule.version               = version.value;

        granule.crossing_time         = crossing_time.value;
        granule.cycle_number          = cycle_number.value;
        granule.lan                   = lan.value;
        granule.orbit_number          = orbit_number.value;
        granule.rgt                   = rgt.value;
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

/*----------------------------------------------------------------------------
 * parseResource
 *
 *  ATL0x_YYYYMMDDHHMMSS_ttttccrr_vvv_ee
 *      YYYY    - year
 *      MM      - month
 *      DD      - day
 *      HH      - hour
 *      MM      - minute
 *      SS      - second
 *      tttt    - reference ground track
 *      cc      - cycle
 *      rr      - region
 *      vvv     - version
 *      ee      - revision
 *----------------------------------------------------------------------------*/
void BathyGranule::parseResource (const char* _resource, TimeLib::date_t& date, uint16_t& rgt, uint8_t& cycle, uint8_t& region)
{
    long val;

    if(StringLib::size(_resource) < 29)
    {
        rgt = 0;
        cycle = 0;
        region = 0;
        date.year = 0;
        date.month = 0;
        date.day = 0;
        return; // early exit on error
    }

    /* get year */
    char year_str[5];
    year_str[0] = _resource[6];
    year_str[1] = _resource[7];
    year_str[2] = _resource[8];
    year_str[3] = _resource[9];
    year_str[4] = '\0';
    if(StringLib::str2long(year_str, &val, 10))
    {
        date.year = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse year from resource %s: %s", _resource, year_str);
    }

    /* get month */
    char month_str[3];
    month_str[0] = _resource[10];
    month_str[1] = _resource[11];
    month_str[2] = '\0';
    if(StringLib::str2long(month_str, &val, 10))
    {
        date.month = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse month from resource %s: %s", _resource, month_str);
    }

    /* get month */
    char day_str[3];
    day_str[0] = _resource[12];
    day_str[1] = _resource[13];
    day_str[2] = '\0';
    if(StringLib::str2long(day_str, &val, 10))
    {
        date.day = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse day from resource %s: %s", _resource, day_str);
    }

    /* get RGT */
    char rgt_str[5];
    rgt_str[0] = _resource[21];
    rgt_str[1] = _resource[22];
    rgt_str[2] = _resource[23];
    rgt_str[3] = _resource[24];
    rgt_str[4] = '\0';
    if(StringLib::str2long(rgt_str, &val, 10))
    {
        rgt = static_cast<uint16_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse RGT from resource %s: %s", _resource, rgt_str);
    }

    /* get cycle */
    char cycle_str[3];
    cycle_str[0] = _resource[25];
    cycle_str[1] = _resource[26];
    cycle_str[2] = '\0';
    if(StringLib::str2long(cycle_str, &val, 10))
    {
        cycle = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse cycle from resource %s: %s", _resource, cycle_str);
    }

    /* get region */
    char region_str[3];
    region_str[0] = _resource[27];
    region_str[1] = _resource[28];
    region_str[2] = '\0';
    if(StringLib::str2long(region_str, &val, 10))
    {
        region = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to parse region from resource %s: %s", _resource, region_str);
    }
}
