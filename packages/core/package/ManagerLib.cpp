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

#include "ManagerLib.h"
#include "OsApi.h"
#include "SystemConfig.h"
#include "TimeLib.h"
#include "LuaEngine.h"
#include "EndpointObject.h"
#include "CurlLib.h"
#include "StringLib.h"

/******************************************************************************
 * ORCHESTRATOR LIBRARY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ManagerLib::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ManagerLib::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
ManagerLib::rsps_t ManagerLib::request (EndpointObject::verb_t verb, const char* resource, const char* data)
{
    // build headers
    CurlLib::hdrs_t headers;
    const FString* content_type_header = new const FString("Content-Type: application/json");
    headers.add(content_type_header);

    // make request
    rsps_t rsps;
    const FString path("%s%s", SystemConfig::settings().managerURL.value.c_str(), resource);
    rsps.code = CurlLib::request(verb, path.c_str(), data, &rsps.response, &rsps.size,
                                 false, false, CurlLib::DATA_TIMEOUT, &headers);

    // return response
    return rsps;
}

/*----------------------------------------------------------------------------
 * luaRequest - post(<verb>, <resource>, <data>)
 *----------------------------------------------------------------------------*/
int ManagerLib::luaRequest(lua_State* L)
{
    // initialize response
    rsps_t rsps = {
        .code = EndpointObject::Bad_Request,
        .response = NULL,
        .size = 0
    };

    try
    {
        // get parameters
        const char* action = LuaObject::getLuaString(L, 1);
        const char* resource = LuaObject::getLuaString(L, 2);
        const char* data = LuaObject::getLuaString(L, 3, true, NULL);

        // translate verb
        const EndpointObject::verb_t verb = EndpointObject::str2verb(action);
        if(verb == EndpointObject::UNRECOGNIZED)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid action: %s", action);
        }

        // make request
        rsps = request(verb, resource, data);
        if(rsps.code != EndpointObject::OK)
        {
            if(rsps.response) mlog(CRITICAL, "%s", rsps.response);
            throw RunTimeException(CRITICAL, RTE_FAILURE, "<%ld> returned from %s", rsps.code, resource);
        }

        // return response
        lua_pushlstring(L, rsps.response, rsps.size);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error in request to manager: %s", e.what());
        lua_pushnil(L);
    }

    // cleanup
    delete [] rsps.response;

    return 1;
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
        "version": "%s"
    })json",
        date.year, date.month, date.day,
        gmt.hour, gmt.minute, gmt.second,
        event->source_ip,
        event->longitude, event->latitude,
        event->client,
        event->endpoint,
        event->duration,
        event->code,
        event->account,
        event->version);

    const rsps_t rsps = request(EndpointObject::POST, "/manager/telemetry/record", rqst.c_str());
    if(rsps.code != EndpointObject::OK)
    {
        mlog(WARNING, "Failed to record request to %s: %s", event->endpoint, rsps.response);
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
    const char* encoded_str = StringLib::jsonize(event->text);

    const FString rqst(R"json({
        "record_time": "%04d-%02d-%02d %02d:%02d:%02d",
        "status_code": %d,
        "version": "%s",
        "message": "%s"
    })json",
        date.year, date.month, date.day,
        gmt.hour, gmt.minute, gmt.second,
        event->code,
        LIBID,
        encoded_str);

    const rsps_t rsps = request(EndpointObject::POST, "/manager/alerts/issue", rqst.c_str());

    if(rsps.code != EndpointObject::OK)
    {
        mlog(WARNING, "Failed to issue alarm %d: %s", event->code, rsps.response);
        status = false;
    }

    delete [] rsps.response;
    delete [] encoded_str;

    return status;
}