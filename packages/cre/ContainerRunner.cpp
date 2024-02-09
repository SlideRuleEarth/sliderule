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
#include "CurlLib.h" // netsvc package dependency
#include "EndpointObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ContainerRunner::OBJECT_TYPE = "ContainerRunner";
const char* ContainerRunner::LUA_META_NAME = "ContainerRunner";
const struct luaL_Reg ContainerRunner::LUA_META_TABLE[] = {
    {"result",      ContainerRunner::luaResult},
    {NULL,          NULL}
};


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :container(<parms>)
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaCreate (lua_State* L)
{
    CreParms* _parms = NULL;

    try
    {
        /* Get Parameters */
        _parms = dynamic_cast<CreParms*>(getLuaObject(L, 1, CreParms::OBJECT_TYPE));

        /* Create Dispatch */
        return createLuaObject(L, new ContainerRunner(L, _parms));
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

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ContainerRunner::ContainerRunner (lua_State* L, const CreParms* _parms):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    result(NULL)
{
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
    delete result;
}

/*----------------------------------------------------------------------------
 * controlThread
 *----------------------------------------------------------------------------*/
void* ContainerRunner::controlThread (void* parm)
{
    ContainerRunner* cr = static_cast<ContainerRunner*>(parm);
    const char* result = NULL;

    /* Run Container */
    const char* unix_socket = "/var/run/docker.sock";
    const char* url= "http://localhost/v1.44/containers/create";
    FString data("{\"Image\": \"%s\"}", cr->parms->image);
    List<string*> headers(5);
    string* content_type = new string("Content-Type: application/json");
    headers.add(content_type);
    const char* response = NULL;
    long http_code = CurlLib::request(EndpointObject::POST, url, data.c_str(), &response, NULL, false, false, &headers, unix_socket);
    if(http_code != EndpointObject::OK)
    {
        mlog(CRITICAL, "Failed to start container <%s>: %s", cr->parms->image, response);
    }

    /* Clean Up Response */
    delete [] response;
    response = NULL;

    /* Wait for Completion and Get Result */
    if(http_code == EndpointObject::OK)
    {
        /* Poll Completion of Container */
        // using timeout

        /* Get Result */
        // from well known file
    }

    /* Signal Complete */
    cr->resultLock.lock();
    {
        cr->result = result;
        cr->resultLock.signal(RESULT_SIGNAL);
    }
    cr->resultLock.unlock();
    cr->signalComplete();

    return NULL;
}


/*----------------------------------------------------------------------------
 * luaResult - result() -> result string
 *----------------------------------------------------------------------------*/
int ContainerRunner::luaResult (lua_State* L)
{
    ContainerRunner* lua_obj = NULL;
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Parameters */
        lua_obj = dynamic_cast<ContainerRunner*>(getLuaSelf(L, 1));

        /* Get Result */
        lua_obj->resultLock.lock();
        {
            /* Wait for Result */
            if(lua_obj->parms->timeout == IO_PEND)
            {
                while((lua_obj->result == NULL) && (lua_obj->active))
                {
                    lua_obj->resultLock.wait(RESULT_SIGNAL, SYS_TIMEOUT);
                }
            }
            else if(lua_obj->parms->timeout > 0)
            {
                long timeout_ms = lua_obj->parms->timeout * 1000;
                while((lua_obj->result == NULL) && (lua_obj->active) && (timeout_ms > 0))
                {
                    lua_obj->resultLock.wait(RESULT_SIGNAL, SYS_TIMEOUT);
                    timeout_ms -= SYS_TIMEOUT;
                }
            }

            /* Push Result */
            lua_pushstring(L, lua_obj->result);
            status = lua_obj->result != NULL;
            num_ret++;
        }
        lua_obj->resultLock.unlock();
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    return returnLuaStatus(L, status, num_ret);
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
