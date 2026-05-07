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

#include <algorithm>

#include "FileEndpoint.h"
#include "OsApi.h"
#include "SystemConfig.h"
#include "TimeLib.h"
#include "RequestFields.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* FileEndpoint::LUA_META_NAME = "FileEndpoint";
const struct luaL_Reg FileEndpoint::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - endpoint()
 *----------------------------------------------------------------------------*/
int FileEndpoint::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* root_directory = getLuaString(L, 1);
        const char* content_type = getLuaString(L, 2);

        /* Create Lua Endpoint */
        return createLuaObject(L, new FileEndpoint(L, root_directory, str2content(content_type)));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
FileEndpoint::FileEndpoint(lua_State* L, const char* root_directory, content_t content_type):
    EndpointObject(L, LUA_META_NAME, LUA_META_TABLE),
    rootDirectory(root_directory),
    contentType(content_type)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
FileEndpoint::~FileEndpoint(void) = default;

/*----------------------------------------------------------------------------
 * handleRequest
 *----------------------------------------------------------------------------*/
void FileEndpoint::handleRequest (Request* request)
{
    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, request->trace_id, "file_endpoint", "{\"verb\":\"%s\", \"resource\":\"%s\"}", verb2str(request->verb), request->resource);

    try
    {
        /* Get Sanitized File Path */
        string file_to_send = sanitize(request->resource);

        /* Open File */
        FILE* fp = fopen(file_to_send.c_str(), "rb");
        if(fp == NULL)
        {
            char err_buf[256];
            sendHeader(Not_Found, content2str(TEXT), &request->rspq, "Resource not found", 18);
            throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to open file %s: %s", file_to_send.c_str(), strerror_r(errno, err_buf, sizeof(err_buf)));
        }

        /* Perform First Read */
        uint8_t buf[MAX_BYTES_TO_SEND];
        size_t bytes_read = fread(buf, 1, MAX_BYTES_TO_SEND, fp);
        if(bytes_read < MAX_BYTES_TO_SEND)
        {
            /* Send Entire Response */
            sendHeader(OK, content2str(contentType), &request->rspq, reinterpret_cast<const char*>(buf), bytes_read);
        }
        else if(bytes_read > 0)
        {
            /* Initiate Chunked Response */
            sendHeader(OK, content2str(contentType), &request->rspq);

            /* Read and Send Rest of File */
            do
            {
                const int rc = request->rspq.postCopy(buf, bytes_read, SystemConfig::settings().publishTimeoutMs.value);
                if(rc <= 0)
                {
                    mlog(CRITICAL, "Failed to post file data on %s: %d", request->rspq.getName(), rc);
                    break;
                }
            } while((bytes_read = fread(buf, 1, MAX_BYTES_TO_SEND, fp)) > 0);
        }
        else
        {
            /* Failed to Read File */
            char err_buf[256];
            mlog(CRITICAL, "Failed (%lu) to read file %s: %s", bytes_read, file_to_send.c_str(), strerror_r(errno, err_buf, sizeof(err_buf)));
            sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, "Failed to read file", 19);
        }

        /* Close File */
        fclose(fp);
    }
    catch (const RunTimeException& e)
    {
        mlog(e.level(), "Error handling request: %s", e.what());
    }

    /* End Response */
    const int rc = request->rspq.postCopy("", 0, SystemConfig::settings().publishTimeoutMs.value);
    if(rc <= 0) mlog(CRITICAL, "Failed to post terminator on %s: %d", request->rspq.getName(), rc);

    /* Clean Up */
    delete request;

    /* Stop Trace */
    stop_trace(INFO, trace_id);
}

/*----------------------------------------------------------------------------
 * sanitize
 *
 *  Examples:
 *      file/rootDirectory/subfolder/resource
 *           |             |
 *         checks          |
 *          and            |--> returns subfolder/resource
 *         throws
 *----------------------------------------------------------------------------*/
string FileEndpoint::sanitize (const char* resource)
{
    assert(resource);

    /* Build path to resource */
    string path_to_resource(resource);

    /* Check for directory traversal */
    if(path_to_resource.find("..") != string::npos)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "directory traversal not allowed in resource: %s", resource);
    }

    /* Check for invalid characters */
    const string invalid_chars = "?*|<>\"'`;&$!#{}[]";
    if(path_to_resource.find_first_of(invalid_chars) != string::npos)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid characters in resource: %s", resource);
    }

    /* Return Fully Constructed Path */
    return string(CONFDIR) + string(PATH_DELIMETER_STR) + rootDirectory + string(PATH_DELIMETER_STR) + path_to_resource;
}
