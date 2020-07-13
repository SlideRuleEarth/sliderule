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

#ifndef __ut_atl06dispatch__
#define __ut_atl06dispatch__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"

/******************************************************************************
 * MATH LIBRARY UNIT TEST CLASS
 ******************************************************************************/

class UT_Atl06Dispatch: public LuaObject
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

    private:


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        UT_Atl06Dispatch        (lua_State* L);
                        ~UT_Atl06Dispatch       (void);

        static int      luaLsfTest              (lua_State* L);
        static int      luaSortTest             (lua_State* L);
};

#endif  /* __ut_atl06dispatch__ */
