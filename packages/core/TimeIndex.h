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

#ifndef __time_index__
#define __time_index__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "AssetIndex.h"
#include "LuaObject.h"

/******************************************************************************
 * TIME INDEX CLASS
 ******************************************************************************/

typedef struct {
    double t0;  // start
    double t1;  // stop            
} timespan_t;

class TimeIndex: public AssetIndex<timespan_t>
{
    public:

                        TimeIndex       (lua_State* L, Asset* _asset, int _threshold);
                        ~TimeIndex      (void);

        static int      luaCreate       (lua_State* L);

        double          getkey          (const timespan_t& span) override;
        void            display         (const timespan_t& span) override;
        bool            isleft          (const timespan_t& span1, const timespan_t& span2) override;
        bool            isright         (const timespan_t& span1, const timespan_t& span2) override;
        bool            intersect       (const timespan_t& span1, const timespan_t& span2) override;
        timespan_t      combine         (const timespan_t& span1, const timespan_t& span2) override;
        timespan_t      luatable2span   (lua_State* L, int parm) override;
};

#endif  /* __time_index__ */
