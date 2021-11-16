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
 *INCLUDES
 ******************************************************************************/

#include "core.h"
#include "netsvc.h"
#include <curl/curl.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_SECURITY_LIBNAME  "netsvc"

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * netsvc_data_t
 *----------------------------------------------------------------------------*/
typedef struct {
    char* data;
    size_t size;
} netsvc_data_t;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * netsvc_write_data (used in GET)
 *----------------------------------------------------------------------------*/
size_t netsvc_write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    List<netsvc_data_t>* rsps_set = (List<netsvc_data_t>*)userp;

    netsvc_data_t rsps;
    rsps.size = size * nmemb;
    rsps.data = new char [rsps.size + 1];

    LocalLib::copy(rsps.data, buffer, rsps.size);
    rsps.data[rsps.size] = '\0';

    rsps_set->add(rsps);

    return rsps.size;
}

/*----------------------------------------------------------------------------
 * netsvc_read_data (used in POST)
 *----------------------------------------------------------------------------*/
size_t netsvc_read_data(void* buffer, size_t size, size_t nmemb, void *userp)
{
    netsvc_data_t* rqst = (netsvc_data_t*)userp;

    size_t buffer_size = size * nmemb;
    size_t bytes_to_copy = rqst->size;
    if(bytes_to_copy > buffer_size) bytes_to_copy = buffer_size;

    if(bytes_to_copy)
    {
        LocalLib::copy(buffer, rqst->data, bytes_to_copy);
        rqst->data += bytes_to_copy;
        rqst->size -= bytes_to_copy;
    }

    return bytes_to_copy;
}

/*----------------------------------------------------------------------------
 * netsvc_get
 *----------------------------------------------------------------------------*/
int netsvc_get (lua_State* L)
{
    bool status = false;
    List<netsvc_data_t> rsps_set;

    /* Get Parameters */
    const char* url             = LuaObject::getLuaString(L, 1);
    bool        verify_peer     = LuaObject::getLuaBoolean(L, 2, true, true);
    bool        verify_hostname = LuaObject::getLuaBoolean(L, 3, true, true);

    /* Initialize cURL */
    CURL* curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, netsvc_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rsps_set);
        curl_easy_setopt(curl, CURLOPT_NETRC, 1L);
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ".cookies");
        curl_easy_setopt(curl, CURLOPT_COOKIEJAR, ".cookies");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        /*
         * If you want to connect to a site who isn't using a certificate that is
         * signed by one of the certs in the CA bundle you have, you can skip the
         * verification of the server's certificate. This makes the connection
         * A LOT LESS SECURE.
         *
         * If you have a CA cert for the server stored someplace else than in the
         * default bundle, then the CURLOPT_CAPATH option might come handy for
         * you.
         */
        if(verify_peer)
        {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        }

        /*
         * If the site you're connecting to uses a different host name that what
         * they have mentioned in their server certificate's commonName (or
         * subjectAltName) fields, libcurl will refuse to connect. You can skip
         * this check, but this will make the connection less secure.
         */
        if(verify_hostname)
        {
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }

        /* Perform the request, res will get the return code */
        CURLcode res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
        {
            mlog(CRITICAL, "network services request failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            status = true;
        }

        /* Get Total Response Size */
        int total_rsps_size = 0;
        for(int i = 0; i < rsps_set.length(); i++)
        {
            total_rsps_size += rsps_set[i].size;
        }

        /* Allocate and Populate Total Response */
        int total_rsps_index = 0;
        char* total_rsps = new char [total_rsps_size + 1];
        for(int i = 0; i < rsps_set.length(); i++)
        {
            LocalLib::copy(&total_rsps[total_rsps_index], rsps_set[i].data, rsps_set[i].size);
            total_rsps_index += rsps_set[i].size;
            delete [] rsps_set[i].data;
        }
        total_rsps[total_rsps_index] = '\0';

        /* Return Response String */
        lua_pushlstring(L, total_rsps, total_rsps_index);
        delete [] total_rsps;

        /* Always Cleanup */
        curl_easy_cleanup(curl);
    }
    else
    {
        /* Return NIL in place of Response String */
        lua_pushnil(L);
    }

    /* Return Status */
    lua_pushboolean(L, status);
    return 2;
}

/*----------------------------------------------------------------------------
 * netsvc_post
 *----------------------------------------------------------------------------*/
int netsvc_post (lua_State* L)
{
    bool status = false;

    /* Get Parameters */
    const char* url             = LuaObject::getLuaString(L, 1);
    const char* data            = LuaObject::getLuaString(L, 2, true, "{}");

    /* Initialize Request */
    netsvc_data_t rqst;
    rqst.data = (char*)data;
    rqst.size = StringLib::size(data);

    /* Initialize Response */
    List<netsvc_data_t> rsps_set;

    /* Initialize cURL */
    CURL* curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, netsvc_read_data);
        curl_easy_setopt(curl, CURLOPT_READDATA, &rqst);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)rqst.size);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, netsvc_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rsps_set);

        /* Perform the request, res will get the return code */
        CURLcode res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
        {
            mlog(CRITICAL, "network services request failed: %s", curl_easy_strerror(res));
        }
        else
        {
            status = true;
        }

        /* Get Total Response Size */
        int total_rsps_size = 0;
        for(int i = 0; i < rsps_set.length(); i++)
        {
            total_rsps_size += rsps_set[i].size;
        }

        /* Allocate and Populate Total Response */
        int total_rsps_index = 0;
        char* total_rsps = new char [total_rsps_size + 1];
        for(int i = 0; i < rsps_set.length(); i++)
        {
            LocalLib::copy(&total_rsps[total_rsps_index], rsps_set[i].data, rsps_set[i].size);
            total_rsps_index += rsps_set[i].size;
            delete [] rsps_set[i].data;
        }
        total_rsps[total_rsps_index] = '\0';

        /* Return Response String */
        lua_pushlstring(L, total_rsps, total_rsps_index);
        delete [] total_rsps;

        /* Always Cleanup */
        curl_easy_cleanup(curl);
    }
    else
    {
        /* Return NIL in place of Response String */
        lua_pushnil(L);
    }

    /* Return Status */
    lua_pushboolean(L, status);
    return 2;
}

/*----------------------------------------------------------------------------
 * netsvc_open
 *----------------------------------------------------------------------------*/
int netsvc_open (lua_State* L)
{
    static const struct luaL_Reg netsvc_functions[] = {
        {"get",         netsvc_get},
        {"post",        netsvc_post},
        {NULL,          NULL}
    };

    /* Set Library */
    luaL_newlib(L, netsvc_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initnetsvc (void)
{
    /* Initialize cURL */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Extend Lua */
    LuaEngine::extend(LUA_SECURITY_LIBNAME, netsvc_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_SECURITY_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_SECURITY_LIBNAME, LIBID);
}

void deinitnetsvc (void)
{
    curl_global_cleanup();
}
}
