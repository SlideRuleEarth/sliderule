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

#ifndef __request_fields__
#define __request_fields__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "FieldDictionary.h"
#include "FieldElement.h"
#include "FieldColumn.h"
#include "RegionMask.h"
#include "List.h"
#include "MathLib.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class RequestFields: public LuaObject, FieldDictionary
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DEFAULT_TIMEOUT            = 600; // seconds
        static const int DEFAULT_CLUSTER_SIZE_HINT  = 0; // dynamic
        static const int TIMEOUT_UNSET              = -1;

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaCreate (lua_State* L);
        static int luaExport (lua_State* L);

        static int luaProjectedPolygonIncludes (lua_State* L);
        static int luaRegionMaskIncludes (lua_State* L);

        bool polyIncludes (double lon, double lat);
        bool maskIncludes (double lon, double lat);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldColumn<MathLib::coord_t>   polygon;
        FieldElement<MathLib::proj_t>   projection          {MathLib::AUTOMATIC};
        FieldElement<int>               pointsInPolygon     {0};
        FieldElement<int>               timeout             {TIMEOUT_UNSET}; // global timeout
        FieldElement<int>               rqstTimeout         {TIMEOUT_UNSET};
        FieldElement<int>               nodeTimeout         {TIMEOUT_UNSET};
        FieldElement<int>               readTimeout         {TIMEOUT_UNSET};
        FieldElement<int>               clusterSizeHint     {DEFAULT_CLUSTER_SIZE_HINT};
        FieldElement<RegionMask>        regionMask;
        
        MathLib::point_t*               projectedPolygon    {NULL};

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        RequestFields   (lua_State* L, int index);
        ~RequestFields  (void) override;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

int convertToLua(lua_State* L, const MathLib::coord_t& v);
void convertFromLua(lua_State* L, int index, MathLib::coord_t& v);

int convertToLua(lua_State* L, const MathLib::point_t& v);
void convertFromLua(lua_State* L, int index, MathLib::point_t& v);

int convertToLua(lua_State* L, const MathLib::proj_t& v);
void convertFromLua(lua_State* L, int index, MathLib::proj_t& v);

#endif  /* __request_fields__ */
