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

#ifndef __logger__
#define __logger__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "LuaObject.h"
#include "OsApi.h"
#include "LogLib.h"

/******************************************************************************
 * LOGGER CLASS
 ******************************************************************************/

class Logger: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static int  logHandler  (const char* msg, int size, void* parm);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        okey_t      logid;
        Publisher*  outq;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    Logger      (lua_State* L, log_lvl_t _level, const char* outq_name, int qdepth=MsgQ::CFG_DEPTH_STANDARD);
                    ~Logger     (void);

        static int  luaConfig   (lua_State* L);
};

#endif  /* __logger__ */
