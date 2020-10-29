/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
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
