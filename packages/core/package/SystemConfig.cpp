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
 * INCLUDE
 ******************************************************************************/

#include "OsApi.h"
#include "SystemConfig.h"

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * getBuildInformation
 *----------------------------------------------------------------------------*/
const char* SystemConfig::getBuildInformation (void)
{
    return BUILDINFO;
}

/*----------------------------------------------------------------------------
 * getLibraryVersion
 *----------------------------------------------------------------------------*/
const char* SystemConfig::getLibraryVersion (void)
{
    return LIBID;
}

/*----------------------------------------------------------------------------
 * luaPopulate - (<parameter table>)
 *----------------------------------------------------------------------------*/
int SystemConfig::luaPopulate (lua_State* L)
{
    bool status = true;

    try
    {
        settings().fromLua(L, 1);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error populating system configuration: %s", e.what());
        status = false;
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * luaGetField - (<field_name>)
 *----------------------------------------------------------------------------*/
int SystemConfig::luaGetField (lua_State* L)
{
    try
    {
        const char* field_name = LuaObject::getLuaString(L, 1);
        return settings().fields[field_name].field->toLua(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(DEBUG, "unable to retrieve field: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaSetField - (<field_name>, <value>)
 *----------------------------------------------------------------------------*/
int SystemConfig::luaSetField (lua_State* L)
{
    bool status = true;

    try
    {
        const char* field_name = LuaObject::getLuaString(L, 1);
        settings().fields[field_name].field->fromLua(L, 2);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "error retrieving field: %s", e.what());
        status = false;
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SystemConfig::SystemConfig(void):
    FieldDictionary ({
        {"log_format",                  &logFormat},
        {"log_level",                   &logLevel},
        {"trace_level",                 &traceLevel},
        {"telemetry_level",             &telemetryLevel},
        {"alert_level",                 &alertLevel},
        {"app_port",                    &appPort},
        {"authenticate_to_nsidc",       &authenticateToNSIDC},
        {"authenticate_to_ornldaac",    &authenticateToORNLDAAC},
        {"authenticate_to_lpdaac",      &authenticateToLPDAAC},
        {"authenticate_to_podaac",      &authenticateToPODAAC},
        {"authenticate_to_asf",         &authenticateToASF},
        {"register_as_service",         &registerAsService},
        {"asset_directory",             &assetDirectory},
        {"normal_mem_thresh",           &normalMemoryThreshold},
        {"stream_mem_thresh",           &streamMemoryThreshold},
        {"msgq_depth",                  &msgQDepth},
        {"authenticate_to_prov_sys",    &authenticateToProvSys},
        {"is_public",                   &isPublic},
        {"in_cloud",                    &inCloud},
        {"sys_bucket",                  &systemBucket},
        {"post_startup_scripts",        &postStartupScripts},
        {"publish_timeout_ms",          &publishTimeoutMs},
        {"request_timeout_sec",         &requestTimeoutSec},
        {"ipv4",                        &ipv4},
        {"environment_version",         &environmentVersion},
        {"orchestrator_url;",           &orchestratorURL},
        {"organization",                &organization},
        {"cluster",                     &cluster},
        {"prov_sys_url",                &provSysURL},
        {"manager_url",                 &managerURL},
        {"ams_url",                     &amsURL},
        {"container_registry",          &containerRegistry}
    })
{
    // populate environment variables
    setIfProvided(ipv4, "IPV4");
    setIfProvided(environmentVersion, "ENVIRONMENT_VERSION");
    setIfProvided(orchestratorURL, "ORCHESTRATOR");
    setIfProvided(organization, "ORGANIZATION");
    setIfProvided(cluster, "CLUSTER");
    setIfProvided(provSysURL, "PROVISIONING_SYSTEM");
    setIfProvided(managerURL, "MANAGER");
    setIfProvided(amsURL, "AMS");
    setIfProvided(containerRegistry, "CONTAINER_REGISTRY");
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
SystemConfig::~SystemConfig(void) = default;

/*----------------------------------------------------------------------------
 * setIfProvided
 *----------------------------------------------------------------------------*/
void SystemConfig::setIfProvided(FieldElement<string>& field, const char* env)
{
    const char* str = getenv(env); // NOLINT(concurrency-mt-unsafe)
    if(str) field = str;
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * convertToJson - SystemConfig::event_format_t
 *----------------------------------------------------------------------------*/
string convertToJson(const SystemConfig::event_format_t& v)
{
    switch(v)
    {
        case SystemConfig::TEXT:    return "\"FMT_TEXT\"";
        case SystemConfig::CLOUD:   return "\"FMT_CLOUD\"";
        default:                    return "\"FMT_TEXT\"";
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - SystemConfig::event_format_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const SystemConfig::event_format_t& v)
{
    switch(v)
    {
        case SystemConfig::TEXT:    lua_pushstring(L, "FMT_TEXT");  break;
        case SystemConfig::CLOUD:   lua_pushstring(L, "FMT_CLOUD"); break;
        default:                    lua_pushstring(L, "FMT_TEXT");  break;
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - SystemConfig::event_format_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, SystemConfig::event_format_t& v)
{
    if(lua_isnumber(L, index))
    {
        v = static_cast<SystemConfig::event_format_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if(StringLib::match(str, "FMT_TEXT")) v = SystemConfig::TEXT;
        else if(StringLib::match(str, "FMT_CLOUD")) v = SystemConfig::CLOUD;
   }
}

/*----------------------------------------------------------------------------
 * convertToJson - event_level_t
 *----------------------------------------------------------------------------*/
string convertToJson(const event_level_t& v)
{
    switch(v)
    {
        case DEBUG:     return "\"DEBUG\"";
        case INFO:      return "\"INFO\"";
        case WARNING:   return "\"WARNING\"";
        case ERROR:     return "\"ERROR\"";
        case CRITICAL:  return "\"CRITICAL\"";
        default:        return "\"INVALID_EVENT_LEVEL\"";
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - event_level_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const event_level_t& v)
{
    switch(v)
    {
        case DEBUG:     lua_pushstring(L, "DEBUG");                 break;
        case INFO:      lua_pushstring(L, "INFO");                  break;
        case WARNING:   lua_pushstring(L, "WARNING");               break;
        case ERROR:     lua_pushstring(L, "ERROR");                 break;
        case CRITICAL:  lua_pushstring(L, "CRITICAL");              break;
        default:        lua_pushstring(L, "INVALID_EVENT_LEVEL");   break;
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - event_level_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, event_level_t& v)
{
    if(lua_isnumber(L, index))
    {
        v = static_cast<event_level_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if(StringLib::match(str, "DEBUG")) v = DEBUG;
        else if(StringLib::match(str, "INFO")) v = INFO;
        else if(StringLib::match(str, "WARNING")) v = WARNING;
        else if(StringLib::match(str, "ERROR")) v = ERROR;
        else if(StringLib::match(str, "CRITICAL")) v = CRITICAL;
        else v = INVALID_EVENT_LEVEL;
   }
}
