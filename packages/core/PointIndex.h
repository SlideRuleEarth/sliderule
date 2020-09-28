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

#ifndef __field_index__
#define __field_index__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "AssetIndex.h"
#include "LuaObject.h"

/******************************************************************************
 * POINT INDEX CLASS
 ******************************************************************************/

typedef struct {
    double minval;
    double maxval;
} pointspan_t;

class PointIndex: public AssetIndex<pointspan_t>
{
    public:

                        PointIndex      (lua_State* L, Asset* _asset,  const char* _fieldname, int _threshold);
                        ~PointIndex     (void);

        static int      luaCreate       (lua_State* L);

        void            display         (const pointspan_t& span) override;
        pointspan_t     split           (const pointspan_t& span) override;
        bool            isleft          (const pointspan_t& span1, const pointspan_t& span2) override;
        bool            isright         (const pointspan_t& span1, const pointspan_t& span2) override;
        bool            intersect       (const pointspan_t& span1, const pointspan_t& span2) override;
        pointspan_t     combine         (const pointspan_t& span1, const pointspan_t& span2) override;
        pointspan_t     luatable2span   (lua_State* L, int parm) override;

    private:

        const char*     fieldname;
};

#endif  /* __field_index__ */
