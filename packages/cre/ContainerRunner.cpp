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
#include <rapidjson/document.h>
#include <cstdlib>
#include <filesystem>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ContainerRunner::OBJECT_TYPE = "ContainerRunner";
const char* ContainerRunner::LUA_META_NAME = "ContainerRunner";
const struct luaL_Reg ContainerRunner::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

const char* ContainerRunner::INPUT_CONTROL_FILENAME = "in.json";
const char* ContainerRunner::OUTPUT_CONTROL_FILENAME = "out.json";
const char* ContainerRunner::HOST_SHARED_DIRECTORY = "/usr/local/share/applications";
const char* ContainerRunner::CONTAINER_SCRIPT_RUNTIME_DIRECTORY = "/usr/local/etc";

const char* ContainerRunner::REGISTRY = NULL;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :container(<parms>, unique_shared_directory, [<outq_name>])
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaCreate (lua_State* L)
{
    CreParms* _parms = NULL;

    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<CreParms*>(getLuaObject(L, 1, CreParms::OBJECT_TYPE));
        const char* unique_shared_directory = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3, true, NULL);

        /* Check Environment */
        if(REGISTRY == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "container registry must be set before a container can be run");
        }

        /* Create Container Runner */
        return createLuaObject(L, new ContainerRunner(L, _parms, unique_shared_directory, outq_name));
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
    long http_code = CurlLib::request(EndpointObject::GET, url, NULL, &response, &size, false, false, NULL, unix_socket);

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
 * luaSettings - settings() -> shared directory, input control filename, output control filename
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaSettings (lua_State* L)
{
    lua_pushstring(L, HOST_SHARED_DIRECTORY);
    lua_pushstring(L, INPUT_CONTROL_FILENAME);
    lua_pushstring(L, OUTPUT_CONTROL_FILENAME);
    return returnLuaStatus(L, true, 4);
}

/*----------------------------------------------------------------------------
 * luaCreateUnique - createunique(<unique shared directory>)
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaCreateUnique (lua_State* L)
{
    bool status = false;
    try
    {
        const char* unique_shared_directory = getLuaString(L, 1);
        if(!std::filesystem::create_directory(unique_shared_directory))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to create unique shared directory: %s", strerror(errno));
        }
        status = true;
    }
    catch(const std::filesystem::filesystem_error& e1)
    {
        const string& explanation = e1.what();
        mlog(CRITICAL, "Filesystem failure: %s", explanation.c_str());
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
        const char* unique_shared_directory = getLuaString(L, 1);
        if(!std::filesystem::remove_all(unique_shared_directory))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to delete unique shared directory: %s", strerror(errno));
        }
        status = true;
    }
    catch(const std::filesystem::filesystem_error& e1)
    {
        const string& explanation = e1.what();
        mlog(CRITICAL, "Filesystem failure: %s", explanation.c_str());
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
ContainerRunner::ContainerRunner (lua_State* L, CreParms* _parms, const char* unique_shared_directory, const char* outq_name):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    outQ(NULL),
    uniqueSharedDirectory(StringLib::duplicate(unique_shared_directory))
{
    assert(_parms);
    assert(unique_shared_directory);

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
    delete [] uniqueSharedDirectory;
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

    /* Build Container Parameters */
    FString image("\"Image\": \"%s/%s\"", REGISTRY, cr->parms->image);
    FString host_config("\"HostConfig\": { \"Binds\": [\"%s:%s\"], \"AutoRemove\": true }", cr->uniqueSharedDirectory, cr->uniqueSharedDirectory);
    FString cmd("\"Cmd\": [\"python\", \"%s/%s\"]}", CONTAINER_SCRIPT_RUNTIME_DIRECTORY, cr->parms->script);
    FString data("{%s, %s, %s}", image.c_str(), host_config.c_str(), cmd.c_str());

    /* Create Container */
    FString create_url("http://localhost/%s/containers/create", api_version);
    const char* create_response = NULL;
    long create_http_code = CurlLib::request(EndpointObject::POST, create_url.c_str(), data.c_str(), &create_response, NULL, false, false, &headers, unix_socket);
    if(create_http_code != EndpointObject::Created) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to create container <%s>: %ld - %s", cr->parms->image, create_http_code, create_response);
    else mlog(INFO, "Created container <%s>", cr->parms->image);

    /* Wait for Completion and Get Result */
    if(create_http_code == EndpointObject::Created)
    {
        /* Get Container ID */
        rapidjson::Document json;
        json.Parse(create_response);
        const char* container_id = json["Id"].GetString();

        /* Start Container */
        FString start_url("http://localhost/%s/containers/%s/start", api_version, container_id);
        const char* start_response = NULL;
        long start_http_code = CurlLib::request(EndpointObject::POST, start_url.c_str(), NULL, &start_response, NULL, false, false, NULL, unix_socket);
        if(start_http_code != EndpointObject::No_Content) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to start container <%s>: %ld - %s", cr->parms->image, start_http_code, start_response);
        else mlog(INFO, "Started container <%s> with Id %s", cr->parms->image, container_id);
        delete [] start_response;

        /* Poll Completion of Container */
        FString wait_url("http://localhost/%s/containers/%s/wait", api_version, container_id);
        const char* wait_response = NULL;
        long wait_http_code = CurlLib::request(EndpointObject::POST, wait_url.c_str(), NULL, &wait_response, NULL, false, false, NULL, unix_socket);
        if(wait_http_code != EndpointObject::OK) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to wait for container <%s>: %ld - %s", cr->parms->image, wait_http_code, wait_response);
        else mlog(INFO, "Waited for and auto-removed container <%s> with Id %s", cr->parms->image, container_id);
        delete [] wait_response;

        /* (If Necessary) Force Stop Container */
        if(wait_http_code != EndpointObject::OK)
        {
            FString stop_url("http://localhost/%s/containers/%s/stop", api_version, container_id);
            const char* stop_response = NULL;
            long stop_http_code = CurlLib::request(EndpointObject::POST, stop_url.c_str(), NULL, &stop_response, NULL, false, false, NULL, unix_socket);
            if(stop_http_code != EndpointObject::OK) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to force stop container <%s>: %ld - %s", cr->parms->image, stop_http_code, stop_response);
            else mlog(INFO, "Force stopped container <%s> with Id %s", cr->parms->image, container_id);
            delete [] stop_response;

            /* Remove Container */
            FString remove_url("http://localhost/%s/containers/%s", api_version, container_id);
            const char* remove_response = NULL;
            long remove_http_code = CurlLib::request(EndpointObject::DELETE, remove_url.c_str(), NULL, &remove_response, NULL, false, false, NULL, unix_socket);
            if(remove_http_code != EndpointObject::No_Content) alert(CRITICAL, RTE_ERROR, cr->outQ, NULL, "Failed to delete container <%s>: %ld - %s", cr->parms->image, remove_http_code, remove_response);
            else mlog(INFO, "Removed container <%s> with Id %s", cr->parms->image, container_id);
            delete [] remove_response;
        }

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
