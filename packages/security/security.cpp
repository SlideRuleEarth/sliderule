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
#include "security.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_SECURITY_LIBNAME  "security"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/


#include <string.h>

int example1(const char* connect_str)
{
    BIO *sbio = NULL, *out = NULL;
   
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL_CONF_CTX* cctx = SSL_CONF_CTX_new();
    SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_CLIENT);
    SSL_CONF_CTX_set_ssl_ctx(cctx, ctx);

    do
    {
        if (!SSL_CONF_CTX_finish(cctx)) {
            fprintf(stderr, "Finish error\n");
            ERR_print_errors_fp(stderr);
            break;
        }

        sbio = BIO_new_ssl_connect(ctx);

        SSL *ssl = NULL;
        BIO_get_ssl(sbio, &ssl);

        if (!ssl) {
            fprintf(stderr, "Can't locate SSL pointer\n");
            break;
        }

        BIO_set_conn_hostname(sbio, connect_str);

        out = BIO_new_fp(stdout, BIO_NOCLOSE);
        if (BIO_do_connect(sbio) <= 0) {
            fprintf(stderr, "Error connecting to server\n");
            ERR_print_errors_fp(stderr);
            break;
        }

        BIO_puts(sbio, "GET / HTTP/1.0\n\n");
        char tmpbuf[1024];
        for (;;) {
            int len = BIO_read(sbio, tmpbuf, 1024);
            if (len <= 0)
                break;
            BIO_write(out, tmpbuf, len);
        }
    } while(false);

    SSL_CONF_CTX_free(cctx);
    BIO_free_all(sbio);
    BIO_free(out);
    return 0;
}

/*----------------------------------------------------------------------------
 * security_test
 *----------------------------------------------------------------------------*/
int security_test (lua_State* L)
{
    (void)L;

    example1("www.google.com:443");

  return 0;

}

/*----------------------------------------------------------------------------
 * security_open
 *----------------------------------------------------------------------------*/
int security_open (lua_State* L)
{
    static const struct luaL_Reg security_functions[] = {
        {"test",        security_test},
        {NULL,          NULL}
    };

    /* Set Library */
    luaL_newlib(L, security_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initsecurity (void)
{
    /* Initialize Modules */

    /* Extend Lua */
    LuaEngine::extend(LUA_SECURITY_LIBNAME, security_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_SECURITY_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_SECURITY_LIBNAME, LIBID);
}

void deinitsecurity (void)
{
}
}
