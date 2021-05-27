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
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * netsvc_test
 *----------------------------------------------------------------------------*/
int netsvc_test (lua_State* L)
{
    (void)L;
    CURL *curl;
    CURLcode res;
 
    curl = curl_easy_init();
    if(curl) 
    {
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.google.com/");
        #ifdef SKIP_PEER_VERIFICATION
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
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        #endif
        
        #ifdef SKIP_HOSTNAME_VERIFICATION
            /*
            * If the site you're connecting to uses a different host name that what
            * they have mentioned in their server certificate's commonName (or
            * subjectAltName) fields, libcurl will refuse to connect. You can skip
            * this check, but this will make the connection less secure.
            */
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        #endif
        
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            fprintf(stdout, "cURL test successfull\n");
        }
   
        /* Always Cleanup */
        curl_easy_cleanup(curl);
    }
    
    return 0;
}

/*----------------------------------------------------------------------------
 * netsvc_open
 *----------------------------------------------------------------------------*/
int netsvc_open (lua_State* L)
{
    static const struct luaL_Reg netsvc_functions[] = {
        {"test",        netsvc_test},
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

    /* Initialize Modules */

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
