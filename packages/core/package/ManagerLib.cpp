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

#include <rapidjson/document.h>

#include "ManagerLib.h"
#include "OsApi.h"
#include "TimeLib.h"
#include "LuaEngine.h"
#include "EndpointObject.h"
#include "CurlLib.h"

/******************************************************************************
 * ORCHESTRATOR LIBRARY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Static Data
 *----------------------------------------------------------------------------*/

const char* ManagerLib::URL = NULL;

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ManagerLib::init (void)
{
    URL = StringLib::duplicate("http://127.0.0.1:8000");
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ManagerLib::deinit (void)
{
    delete [] URL;
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
ManagerLib::rsps_t ManagerLib::request (EndpointObject::verb_t verb, const char* resource, const char* data)
{
    rsps_t rsps;
    const FString path("%s%s", URL, resource);
    rsps.code = CurlLib::request(verb, path.c_str(), data, &rsps.response, &rsps.size);
    return rsps;
}

/*----------------------------------------------------------------------------
 * recordTelemetry
 *----------------------------------------------------------------------------*/
bool ManagerLib::recordTelemetry (const EventLib::telemetry_t* event)
{
    bool status = true;

    const TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(event->time);
    const TimeLib::date_t date = TimeLib::gmt2date(gmt);

    const FString rqst(R"json({
        "record_time": "%04d-%02d-%02d %02d:%02d:%02d",
        "source_ip": "%s",
        "aoi": {"x": %lf, "y": %lf},
        "client": "%s",
        "endpoint": "%s",
        "duration": %f,
        "status_code": %d,
        "account": "%s",
        "version": "%s",
        "message": "%s"
    })json",
        date.year, date.month, date.day,
        gmt.hour, gmt.minute, gmt.second,
        event->ipv4,
        event->aoi.lon, event->aoi.lat,
        event->client,
        event->endpoint,
        event->duration,
        event->code,
        event->account,
        event->version,
        event->message);

    const rsps_t rsps = request(EndpointObject::POST, "/manager/telemetry/record", rqst.c_str());
    if(rsps.code != EndpointObject::OK)
    {
        mlog(CRITICAL, "Failed to record request to %s: %s", event->endpoint, rsps.response);
        status = false;
    }

    delete [] rsps.response;

    return status;
}

/*----------------------------------------------------------------------------
 * issueAlert
 *----------------------------------------------------------------------------*/
bool ManagerLib::issueAlert (const EventLib::alert_t* event)
{
    bool status = true;

    const TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(TimeLib::gpstime());
    const TimeLib::date_t date = TimeLib::gmt2date(gmt);

    const FString rqst(R"json({
        "record_time": "%04d-%02d-%02d %02d:%02d:%02d",
        "status_code": %d,
        "account": "%s",
        "version": "%s",
        "message": "%s"
    })json",
        date.year, date.month, date.day,
        gmt.hour, gmt.minute, gmt.second,
        event->code,
        "anonymous",
        LIBID,
        event->text);

    const rsps_t rsps = request(EndpointObject::POST, "/manager/alert/issue", rqst.c_str());
    if(rsps.code != EndpointObject::OK)
    {
        mlog(CRITICAL, "Failed to issue alarm %d: %s", event->code, rsps.response);
        status = false;
    }

    delete [] rsps.response;

    return status;
}

/*----------------------------------------------------------------------------
 * luaUrl - orchurl(<URL>)
 *----------------------------------------------------------------------------*/
int ManagerLib::luaUrl(lua_State* L)
{
    try
    {
        const char* _url = LuaObject::getLuaString(L, 1);

        delete [] URL;
        URL = StringLib::duplicate(_url);
    }
    catch(const RunTimeException& e)
    {
        // silently fail... allows calling lua script to set nil
        // as way of keeping and returning the current value
        (void)e;
    }

    lua_pushstring(L, URL);

    return 1;
}
