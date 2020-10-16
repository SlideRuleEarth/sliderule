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

#ifndef __interval_index__
#define __interval_index__

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
} intervalspan_t;

class IntervalIndex: public AssetIndex<intervalspan_t>
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        IntervalIndex   (lua_State* L, Asset* _asset, const char* _fieldname0, const char* _fieldname1, int _threshold);
                        ~IntervalIndex  (void);

        static int      luaCreate       (lua_State* L);

        void            split           (node_t* node, intervalspan_t& lspan, intervalspan_t& rspan) override;
        bool            isleft          (node_t* node, const intervalspan_t& span) override;
        bool            isright         (node_t* node, const intervalspan_t& span) override;
        bool            intersect       (const intervalspan_t& span1, const intervalspan_t& span2) override;
        intervalspan_t  combine         (const intervalspan_t& span1, const intervalspan_t& span2) override;
        intervalspan_t  attr2span       (Dictionary<double>* attr, bool* provided=NULL) override;
        intervalspan_t  luatable2span   (lua_State* L, int parm) override;
        void            displayspan     (const intervalspan_t& span) override;
    
    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*              LuaMetaName;
        static const struct luaL_Reg    LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char* fieldname0;
        const char* fieldname1;
};

#endif  /* __interval_index__ */
