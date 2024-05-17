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

#include "ContainerRunner.h"
#include "OsApi.h"
#include "EventLib.h"
#include "CurlLib.h" // netsvc package dependency
#include "EndpointObject.h"
#include "TimeLib.h"
#include <rapidjson/document.h>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <iostream>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ContainerRunner::OBJECT_TYPE = "ContainerRunner";
const char* ContainerRunner::LUA_META_NAME = "ContainerRunner";
const struct luaL_Reg ContainerRunner::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

const char* ContainerRunner::SHARED_DIRECTORY = "/data";

const char* ContainerRunner::REGISTRY = NULL;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :container(<parms>, host_shared_directory, [<outq_name>])
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaCreate (lua_State* L)
{
    CreParms* _parms = NULL;

    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<CreParms*>(getLuaObject(L, 1, CreParms::OBJECT_TYPE));
        const char* host_shared_directory = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3, true, NULL);

        /* Check Environment */
        if(REGISTRY == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "container registry must be set before a container can be run");
        }

        /* Create Container Runner */
        return createLuaObject(L, new ContainerRunner(L, _parms, host_shared_directory, outq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ContainerRunner::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ContainerRunner::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * getRegistry
 *----------------------------------------------------------------------------*/
const char* ContainerRunner::getRegistry (void)
{
    return REGISTRY;
}

/*----------------------------------------------------------------------------
 * luaList - list() -> json of running containers
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaList (lua_State* L)
{
    const char* unix_socket = "/var/run/docker.sock";
    const char* url= "http://localhost/v1.43/containers/json";

    /* Make Request for List of Containers */
    const char* response = NULL;
    int size = 0;
    const long http_code = CurlLib::request(EndpointObject::GET, url, NULL, &response, &size, false, false, CurlLib::DATA_TIMEOUT, NULL, unix_socket);

    /* Push Result */
    lua_pushinteger(L, http_code);
    lua_pushinteger(L, size);
    lua_pushstring(L, response);

    /* Clean Up */
    delete [] response;

    /* Return */
    return returnLuaStatus(L, true, 4);
}

/*----------------------------------------------------------------------------
 * luaSettings - settings() -> shared directory
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaSettings (lua_State* L)
{
    lua_pushstring(L, SHARED_DIRECTORY);
    return returnLuaStatus(L, true, 2);
}

/*----------------------------------------------------------------------------
 * luaCreateUnique - createunique(<unique shared directory>)
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaCreateUnique (lua_State* L)
{
    bool status = false;
    try
    {
        const char* host_shared_directory = getLuaString(L, 1);
        if(!std::filesystem::create_directory(host_shared_directory))
        {
            char err_buf[256];
            throw RunTimeException(CRITICAL, RTE_ERROR, "%s", strerror_r(errno, err_buf, sizeof(err_buf)));
        }
        status = true;
    }
    catch(const std::filesystem::filesystem_error& e1)
    {
        const string& explanation = e1.what();
        mlog(CRITICAL, "filesystem failure: %s", explanation.c_str());
    }
    catch(const RunTimeException& e2)
    {
        mlog(e2.level(), "Failed to create unique resources: %s", e2.what());
    }

    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaDeleteUnique - deleteunique(<unique shared directory>)
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaDeleteUnique (lua_State* L)
{
    bool status = false;
    try
    {
        const char* host_shared_directory = getLuaString(L, 1);
        if(!std::filesystem::remove_all(host_shared_directory))
        {
            char err_buf[256];
            throw RunTimeException(CRITICAL, RTE_ERROR, "%s", strerror_r(errno, err_buf, sizeof(err_buf)));
        }
        status = true;
    }
    catch(const std::filesystem::filesystem_error& e1)
    {
        const string& explanation = e1.what();
        mlog(CRITICAL, "filesystem failure - %s", explanation.c_str());
    }
    catch(const RunTimeException& e2)
    {
        mlog(e2.level(), "Failed to delete unique resources: %s", e2.what());
    }

    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaSetRegistry - setregistry(<registry>)
 *
 *  - MUST BE SET BEFORE FIRST USE
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaSetRegistry (lua_State* L)
{
    bool status = false;
    try
    {
        /* Get Parameters */
        const char* registry_name = getLuaString(L, 1);

        /* Set Registry */
        if(REGISTRY == NULL)
        {
            REGISTRY = StringLib::duplicate(registry_name);
            status = true;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failed to set registry: %s", e.what());
    }

    return returnLuaStatus(L, status);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ContainerRunner::ContainerRunner (lua_State* L, CreParms* _parms, const char* host_shared_directory, const char* outq_name):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    outQ(NULL),
    hostSharedDirectory(StringLib::duplicate(host_shared_directory))
{
    assert(_parms);
    assert(host_shared_directory);

    parms = _parms;
    if(outq_name) outQ = new Publisher(outq_name);
    active = true;
    controlPid = new Thread(controlThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ContainerRunner::~ContainerRunner (void)
{
    active = false;
    delete controlPid;
    delete outQ;
    delete [] hostSharedDirectory;
    parms->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * controlThread
 *----------------------------------------------------------------------------*/
void* ContainerRunner::controlThread (void* parm)
{
    ContainerRunner* cr = reinterpret_cast<ContainerRunner*>(parm);

    /* Set Docker Socket */
    const char* unix_socket = "/var/run/docker.sock";
    const char* api_version = "v1.43";

    /* Configure HTTP Headers */
    List<string*> headers(5);
    string* content_type = new string("Content-Type: application/json");
    headers.add(content_type);

    /* Build Container Command Parameter */
    string token;
    vector<string> tokens;
    std::istringstream cmd_str_iss(cr->parms->command);
    while(std::getline(cmd_str_iss, token, ' '))
    {
        if(!token.empty())
        {
            tokens.push_back(token);
        }
    }
    string cmd_str;
    for(unsigned i = 0; i < tokens.size(); i++)
    {
        FString elem_str("\"%s\"", tokens[i].c_str());
        cmd_str += elem_str.c_str();
        if(i < (tokens.size() - 1)) cmd_str += ", ";
    }
    FString cmd("\"Cmd\": [%s]}", cmd_str.c_str());

    /* Build Container Parameters */
    FString image("\"Image\": \"%s/%s\"", REGISTRY, cr->parms->image);
    FString host_config("\"HostConfig\": {\"Binds\": [\"%s:%s\"]}", cr->hostSharedDirectory, SHARED_DIRECTORY);
    FString data("{%s, %s, %s}", image.c_str(), host_config.c_str(), cmd.c_str());

    /* Create Container */
    FString create_url("http://localhost/%s/containers/create", api_version);
    const char* create_response = NULL;
    const long create_http_code = CurlLib::request(EndpointObject::POST, create_url.c_str(), data.c_str(), &create_response, NULL, false, false, cr->parms->timeout, &headers, unix_socket);
    if(create_http_code != EndpointObject::Created) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to create container <%s>: %ld - %s", cr->parms->image, create_http_code, create_response);
    else mlog(INFO, "Created container <%s> with parameters: %s", cr->parms->image, data.c_str());

    /* Run Container */
    if(create_http_code == EndpointObject::Created)
    {
        /* Get Container ID */
        rapidjson::Document json;
        json.Parse(create_response);
        const char* container_id = json["Id"].GetString();

        /* Build Container Name String */
        char subid[8];
        StringLib::copy(subid, container_id, 8);
        FString container_name_str("%s:%s", cr->parms->image, subid);

        /* Latch Start Time */
        int64_t logs_since = TimeLib::gps2systime(TimeLib::gpstime()) / 1000000;

        /* Start Container */
        FString start_url("http://localhost/%s/containers/%s/start", api_version, container_id);
        const char* start_response = NULL;
        const long start_http_code = CurlLib::request(EndpointObject::POST, start_url.c_str(), NULL, &start_response, NULL, false, false, CurlLib::DATA_TIMEOUT, NULL, unix_socket);
        if(start_http_code != EndpointObject::No_Content) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to start container <%s>: %ld - %s", container_name_str.c_str(), start_http_code, start_response);
        else mlog(INFO, "Started container <%s>", container_name_str.c_str());
        delete [] start_response;

        /* Wait Until Container Has Completed */
        bool done = false;
        bool in_error = false;
        int time_left = cr->parms->timeout;
        while(!done && !in_error)
        {
            /* Check Time Left */
            time_left -= WAIT_TIMEOUT;
            if(time_left <= 0)
            {
                mlog(ERROR, "Timeout reached for container <%s> after %d seconds", container_name_str.c_str(), cr->parms->timeout);
                done = true;
                in_error = true;
            }

            /* Poll Completion of Container */
            FString wait_url("http://localhost/%s/containers/%s/wait", api_version, container_id);
            const char* wait_response = NULL;
            const long wait_http_code = CurlLib::request(EndpointObject::POST, wait_url.c_str(), NULL, &wait_response, NULL, false, false, WAIT_TIMEOUT, NULL, unix_socket);
            if(wait_http_code == EndpointObject::OK)
            {
                mlog(INFO, "Container <%s> completed", cr->parms->image);
                done = true;
            }
            else if(wait_http_code != EndpointObject::Service_Unavailable) // curl timed out which is normal if container is still running
            {
                alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to wait for container <%s>: %ld - %s", container_name_str.c_str(), wait_http_code, wait_response);
                done = true;
                in_error = true;
            }
            delete [] wait_response;

            /* Get Logs for Container */
            const int64_t logs_now = TimeLib::gps2systime(TimeLib::gpstime()) / 1000000;
            FString log_url("http://localhost/%s/containers/%s/logs?stdout=1&stderr=1&since=%ld", api_version, container_id, logs_since);
            logs_since = logs_now;
            const char* log_response = NULL;
            int log_response_size = 0;
            const long log_http_code = CurlLib::request(EndpointObject::GET, log_url.c_str(), NULL, &log_response, &log_response_size, false, false, WAIT_TIMEOUT, NULL, unix_socket);
            if(log_http_code != EndpointObject::OK) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to get logs container <%s>: %ld - %s", container_name_str.c_str(), log_http_code, log_response);
            else processContainerLogs(log_response, log_response_size, container_id);
            delete [] log_response;

            /* Get Status of Container */
            if(!done)
            {
                FString status_url("http://localhost/%s/containers/%s/json", api_version, container_id);
                const char* status_response = NULL;
                const long status_http_code = CurlLib::request(EndpointObject::GET, status_url.c_str(), NULL, &status_response, NULL, false, false, WAIT_TIMEOUT, NULL, unix_socket);
                if(status_http_code != EndpointObject::OK)
                {
                    alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to get status of container <%s>: %ld - %s", container_name_str.c_str(), status_http_code, status_response);
                    done = true;
                    in_error = true;
                }
                else
                {
                    rapidjson::Document status_json;
                    status_json.Parse(status_response);
                    const char* container_status = status_json["State"]["Status"].GetString();
                    if(StringLib::match(container_status, "running"))
                    {
                        const bool rspq_status = alert(INFO, RTE_INFO, cr->outQ, NULL, "Container <%s> still running... %d seconds left", container_name_str.c_str(), time_left);
                        if(!rspq_status) in_error = true;
                    }
                    else if(StringLib::match(container_status, "stopped"))
                    {
                        alert(INFO, RTE_INFO, cr->outQ, NULL, "Container <%s> has stopped", container_name_str.c_str());
                        done = true;
                    }
                    else
                    {
                        alert(ERROR, RTE_ERROR, cr->outQ, NULL, "Container <%s> has is in an unexpected state: %s", container_name_str.c_str(), container_status);
                        done = true;
                        in_error = true;
                    }
                }
                delete [] status_response;
            }
        }

        /* (If Necessary) Force Stop Container */
        if(in_error)
        {
            FString stop_url("http://localhost/%s/containers/%s/stop", api_version, container_id);
            const char* stop_response = NULL;
            const long stop_http_code = CurlLib::request(EndpointObject::POST, stop_url.c_str(), NULL, &stop_response, NULL, false, false, CurlLib::DATA_TIMEOUT, NULL, unix_socket);
            if(stop_http_code != EndpointObject::No_Content) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to force stop container <%s>: %ld - %s", cr->parms->image, stop_http_code, stop_response);
            else mlog(INFO, "Force stopped container <%s> with Id %s", cr->parms->image, container_id);
            delete [] stop_response;
        }

        /* Remove Container */
        FString remove_url("http://localhost/%s/containers/%s", api_version, container_id);
        const char* remove_response = NULL;
        const long remove_http_code = CurlLib::request(EndpointObject::DELETE, remove_url.c_str(), NULL, &remove_response, NULL, false, false, CurlLib::DATA_TIMEOUT, NULL, unix_socket);
        if(remove_http_code != EndpointObject::No_Content) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to delete container <%s>: %ld - %s", cr->parms->image, remove_http_code, remove_response);
        else mlog(INFO, "Removed container <%s> with Id %s", cr->parms->image, container_id);
        delete [] remove_response;

        /* Get Result */
        if(cr->outQ)
        {
            // read files from output directory (provided to container)
            // stream files back to user (or to S3??? like ParquetBuilder; maybe need generic library for that)
        }
    }

    /* Clean Up */
    delete [] create_response;

    /* Signal Complete */
    cr->signalComplete();

    return NULL;
}

/*----------------------------------------------------------------------------
 * controlThread
 *----------------------------------------------------------------------------*/
void ContainerRunner::processContainerLogs (const char* buffer, int buffer_size, const char* id)
{
    char id_str[8];
    StringLib::copy(id_str, id, 8);

    int buffer_index = 0;
    while(buffer_index < buffer_size)
    {
        /* Check Truncation */
        if(buffer_index > (buffer_size - 8))
        {
            mlog(CRITICAL, "%s - truncated container log response at %d of %d", id_str, buffer_index, buffer_size);
            break;
        }

        /* Get Stdout vs. Stderr */
        const int pipe = buffer[buffer_index];
        buffer_index += 4;

        /* Get Message Length */
        uint32_t message_length = buffer[buffer_index++];
        message_length <<= 8;
        message_length |= buffer[buffer_index++];
        message_length <<= 8;
        message_length |= buffer[buffer_index++];
        message_length <<= 8;
        message_length |= buffer[buffer_index++];

        /* Get Message */
        const int log_message_length = MIN(EventLib::MAX_ATTR_SIZE, message_length);
        char* message = new char [log_message_length + 1];
        StringLib::copy(message, &buffer[buffer_index], log_message_length + 1);
        buffer_index += message_length;

        /* Determine Log Level */
        event_level_t lvl;
        if      (pipe == 1) lvl = INFO;
        else if (pipe == 2) lvl = ERROR;
        else                lvl = CRITICAL;

        /* Remove Trialing White Space */
        if(message[log_message_length-1] == '\n') message[log_message_length-1] = '\0';

        /* Log Message */
        mlog(lvl, "%s - %s", id_str, message);

        /* Clean Up */
        delete [] message;
    }
}
