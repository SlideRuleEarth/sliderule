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


#include "ProvisioningSystemLib.h"
#include "core.h"

#include <curl/curl.h>
#include <rapidjson/document.h>

/******************************************************************************
 * PROVISIONING SYSTEM LIBRARY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Local Functions
 *----------------------------------------------------------------------------*/

size_t write2nothing(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    (void)ptr;
    (void)userdata;
    return size * nmemb;
}

/*----------------------------------------------------------------------------
 * Static Data
 *----------------------------------------------------------------------------*/

const char* ProvisioningSystemLib::URL = NULL;
const char* ProvisioningSystemLib::Organization = NULL;

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ProvisioningSystemLib::init (void)
{
    URL = StringLib::duplicate(DEFAULT_PS_URL);
    Organization = StringLib::duplicate(DEFAULT_ORGANIZATION_NAME);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ProvisioningSystemLib::deinit (void)
{
    if(URL) delete [] URL;
    if(Organization) delete [] Organization;
}

/*----------------------------------------------------------------------------
 * validate
 *----------------------------------------------------------------------------*/
const char* ProvisioningSystemLib::login (const char* username, const char* password, const char* organization, bool verbose)
{
    char* rsps = NULL;

    try
    {
        /* Build API URL */
        SafeString url_str("%s/api/org_token/", URL);

        /* Build Bearer Token Header */
        SafeString hdr_str("Content-Type: application/json");

        /* Initialize Request */
        SafeString data_str("{\"username\":\"%s\",\"password\":\"%s\",\"org_name\":\"%s\"}", username, password, organization);

        /* Initialize Response */
        List<data_t> rsps_set;

        /* Initialize cURL */
        CURL* curl = curl_easy_init();
        if(curl)
        {
            /* Set cURL Options */
            curl_easy_setopt(curl, CURLOPT_URL, url_str.str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // seconds
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // seconds
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data_str.str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ProvisioningSystemLib::writeData);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rsps_set);

            /* Set Content-Type Header */
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, hdr_str.str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            /* Perform the request, res will get the return code */
            CURLcode res = curl_easy_perform(curl);

            /* Check for Success */
            if(res == CURLE_OK)
            {
                /* Get HTTP Code */
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                if(http_code == 200)
                {
                    /* Get Response Size */
                    int rsps_size = 0;
                    for(int i = 0; i < rsps_set.length(); i++)
                    {
                        rsps_size += rsps_set[i].size;
                    }

                    /* Allocate and Populate Response */
                    int rsps_index = 0;
                    rsps = new char [rsps_size + 1];
                    for(int i = 0; i < rsps_set.length(); i++)
                    {
                        memcpy(&rsps[rsps_index], rsps_set[i].data, rsps_set[i].size);
                        rsps_index += rsps_set[i].size;
                        delete [] rsps_set[i].data;
                    }
                    rsps[rsps_index] = '\0';
                }
                else if(verbose)
                {
                    mlog(CRITICAL, "Http error <%ld> returned by provisioning system", http_code);
                }
            }
            else if(verbose)
            {
                mlog(CRITICAL, "curl request error (%ld): %s", (long)res, curl_easy_strerror(res));
            }

            /* Always Cleanup */
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error on login: %s", e.what());
        if(rsps) delete [] rsps;
        rsps = NULL;
    }

    /* Return Response */
    return rsps;
}

/*----------------------------------------------------------------------------
 * validate
 *----------------------------------------------------------------------------*/
bool ProvisioningSystemLib::validate (const char* access_token, bool verbose)
{
    bool status = false;

    try
    {
        /* Build API URL */
        SafeString url_str("%s/api/membership_status/%s/", URL, Organization);

        /* Build Bearer Token Header */
        SafeString hdr_str("Authorization: Bearer %s", access_token);

        /* Initialize cURL */
        CURL* curl = curl_easy_init();
        if(curl)
        {
            /* Set cURL Options */
            curl_easy_setopt(curl, CURLOPT_URL, url_str.str());
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write2nothing);

            /* Set Bearer Token Header */
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, hdr_str.str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            /* Perform the request, res will get the return code */
            CURLcode res = curl_easy_perform(curl);

            /* Check for Success */
            if(res == CURLE_OK)
            {
                /* Get HTTP Code */
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                if(http_code == 200)
                {
                    /* Set Successfull */
                    status = true;
                }
                else if(verbose)
                {
                    mlog(CRITICAL, "Http error <%ld> returned by provisioning system", http_code);
                }
            }
            else if(verbose)
            {
                mlog(CRITICAL, "curl request failed: %ld", (long)res);
            }

            /* Always Cleanup */
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error validating membership: %s", e.what());
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * luaUrl - psurl(<URL>)
 *----------------------------------------------------------------------------*/
int ProvisioningSystemLib::luaUrl(lua_State* L)
{
    try
    {
        const char* _url = LuaObject::getLuaString(L, 1);

        if(URL) delete [] URL;
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

/*----------------------------------------------------------------------------
 * luaSetOrganization - psorg(<organization>)
 *----------------------------------------------------------------------------*/
int ProvisioningSystemLib::luaSetOrganization(lua_State* L)
{
    try
    {
        const char* _organization = LuaObject::getLuaString(L, 1);

        if(Organization) delete [] Organization;
        Organization = StringLib::duplicate(_organization);
    }
    catch(const RunTimeException& e)
    {
        // silently fail... allows calling lua script to set nil
        // as way of keeping and returning the current value
        (void)e;
    }

    lua_pushstring(L, Organization);

    return 1;
}

/*----------------------------------------------------------------------------
 * luaLogin - pslogin(<username>, <password>, <organization>, [<verbose>])
 *----------------------------------------------------------------------------*/
int ProvisioningSystemLib::luaLogin(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* username        = LuaObject::getLuaString(L, 1);
        const char* password        = LuaObject::getLuaString(L, 2);
        const char* organization    = LuaObject::getLuaString(L, 3);
        bool verbose                = LuaObject::getLuaBoolean(L, 4, true, false);

        const char* rsps = login(username, password, organization, verbose);
        lua_pushstring(L, rsps);
        delete [] rsps;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error authenticating: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaValidate - psvalidate(<token>, [<verbose>])
 *----------------------------------------------------------------------------*/
int ProvisioningSystemLib::luaValidate(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* token   = LuaObject::getLuaString(L, 1);
        bool verbose        = LuaObject::getLuaBoolean(L, 2, true, false);

        lua_pushboolean(L, validate(token, verbose));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error validating: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/******************************************************************************
 * AUTHENTICATOR SUBCLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - .psauth()
 *----------------------------------------------------------------------------*/
int ProvisioningSystemLib::Authenticator::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new Authenticator(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating Authenticator: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ProvisioningSystemLib::Authenticator::Authenticator (lua_State* L):
    LuaEndpoint::Authenticator(L)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ProvisioningSystemLib::Authenticator::~Authenticator (void)
{
}

/*----------------------------------------------------------------------------
 * isValid
 *----------------------------------------------------------------------------*/
bool ProvisioningSystemLib::Authenticator::isValid (const char* token)
{
    if(StringLib::match(ProvisioningSystemLib::Organization, DEFAULT_ORGANIZATION_NAME, MAX_STR_SIZE))
    {
        return true; // no authentication used for default organization name
    }
    else if(token != NULL)
    {
        return ProvisioningSystemLib::validate(token);
    }
    else
    {
        return false;
    }
}

/*----------------------------------------------------------------------------
 * writeData
 *----------------------------------------------------------------------------*/
size_t ProvisioningSystemLib::writeData(void *buffer, size_t size, size_t nmemb, void *userp)
{
    List<data_t>* rsps_set = (List<data_t>*)userp;

    data_t rsps;
    rsps.size = size * nmemb;
    rsps.data = new char [rsps.size + 1];

    memcpy(rsps.data, buffer, rsps.size);
    rsps.data[rsps.size] = '\0';

    rsps_set->add(rsps);

    return rsps.size;
}
