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

#include "AmsLib.h"
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
void AmsLib::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void AmsLib::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
AmsLib::rsps_t AmsLib::request (EndpointObject::verb_t verb, const char* resource, const char* data)
{
    // build headers
    CurlLib::hdrs_t headers;
    const FString* content_type_header = new const FString("Content-Type: application/json");
    headers.add(content_type_header);

    // make request
    rsps_t rsps;
    const FString path("%s/ams/%s", SystemConfig::settings().amsURL.value.c_str(), resource);
    rsps.code = CurlLib::request(verb, path.c_str(), data, &rsps.response, &rsps.size,
                                 false, false, CurlLib::DATA_TIMEOUT, &headers);

    // return response
    return rsps;
}

/*----------------------------------------------------------------------------
 * luaRequest - post(<verb>, <resource>, <data>)
 *----------------------------------------------------------------------------*/
int AmsLib::luaRequest(lua_State* L)
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
        mlog(e.level(), "Error in request to asset metadata service: %s", e.what());
        lua_pushnil(L);
    }

    // cleanup
    delete [] rsps.response;

    return 1;
}