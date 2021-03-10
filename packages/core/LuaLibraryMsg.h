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

#ifndef __lua_library_msg__
#define __lua_library_msg__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "RecordObject.h"
#include "Dictionary.h"

#include <lua.h>

/******************************************************************************
 * LUA LIBRARY MSG CLASS
 ******************************************************************************/

class LuaLibraryMsg
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef RecordObject* (*createRecFunc) (const char* str);
        typedef RecordObject* (*associateRecFunc) (unsigned char* data, int size);

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_MSGLIBNAME;
        static const char* REC_TYPE_ATTR;
        static const char* REC_ID_ATTR;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     lmsg_init           (void);
        static bool     lmsg_addtype        (const char* recclass, char prefix, createRecFunc cfunc, associateRecFunc afunc);
        static int      luaopen_msglib      (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            const char*     msgq_name;
            Publisher*      pub;
        } msgPublisherData_t;

        typedef struct {
            const char*     msgq_name;
            Subscriber*     sub;
        } msgSubscriberData_t;

        typedef struct {
            const char*     record_str;
            RecordObject*   rec;
        } recUserData_t;

        typedef struct {
            char                prefix;
            createRecFunc       create;
            associateRecFunc    associate;
        } recClass_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const char* LUA_PUBMETANAME;
        static const char* LUA_SUBMETANAME;
        static const char* LUA_RECMETANAME;

        static const struct luaL_Reg msgLibsF [];
        static const struct luaL_Reg pubLibsM [];
        static const struct luaL_Reg subLibsM [];
        static const struct luaL_Reg recLibsM [];

        static recClass_t prefixLookUp[0xFF];
        static Dictionary<recClass_t> typeTable;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static RecordObject* populateRecord (const char* population_string);
        static RecordObject* associateRecord (const char* recclass, unsigned char* data, int size);

        /* message library functions */
        static int      lmsg_publish        (lua_State* L);
        static int      lmsg_subscribe      (lua_State* L);
        static int      lmsg_create         (lua_State* L);
        static int      lmsg_definition     (lua_State* L);

        /* publisher meta functions */
        static int      lmsg_sendstring     (lua_State* L);
        static int      lmsg_sendrecord     (lua_State* L);
        static int      lmsg_sendlog        (lua_State* L);
        static int      lmsg_deletepub      (lua_State* L);

        /* subscriber meta functions */
        static int      lmsg_recvstring     (lua_State* L);
        static int      lmsg_recvrecord     (lua_State* L);
        static int      lmsg_drain          (lua_State* L);
        static int      lmsg_deletesub      (lua_State* L);

        /* record meta functions */
        static int      lmsg_gettype        (lua_State* L);
        static int      lmsg_getfieldvalue  (lua_State* L);
        static int      lmsg_setfieldvalue  (lua_State* L);
        static int      lmsg_serialize      (lua_State* L);
        static int      lmsg_deserialize    (lua_State* L);
        static int      lmsg_tabulate       (lua_State* L);
        static int      lmsg_detabulate     (lua_State* L);
        static int      lmsg_deleterec      (lua_State* L);
};

#endif  /* __lua_library_msg__ */
