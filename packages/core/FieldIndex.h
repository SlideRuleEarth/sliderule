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
 * TIME INDEX CLASS
 ******************************************************************************/

typedef struct {
    double minval;
    double maxval;
} fieldspan_t;

class FieldIndex: public AssetIndex<fieldspan_t>
{
    public:

                        FieldIndex      (lua_State* L, Asset* _asset,  const char* _fieldname, int _threshold);
                        ~FieldIndex     (void);

        static int      luaCreate       (lua_State* L);

        void            display         (const fieldspan_t& span) override;
        fieldspan_t     split           (const fieldspan_t& span) override;
        bool            isleft          (const fieldspan_t& span1, const fieldspan_t& span2) override;
        bool            isright         (const fieldspan_t& span1, const fieldspan_t& span2) override;
        bool            intersect       (const fieldspan_t& span1, const fieldspan_t& span2) override;
        fieldspan_t     combine         (const fieldspan_t& span1, const fieldspan_t& span2) override;
        fieldspan_t     luatable2span   (lua_State* L, int parm) override;

        const char*     fieldname;
};

#endif  /* __field_index__ */
