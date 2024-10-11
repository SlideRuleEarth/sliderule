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
#include "FieldMap.h"
#include "RegionMask.h"
#include "MathLib.h"

#ifdef __arrow__
#include "ArrowFields.h"
#endif

#ifdef __geo__
#include "GeoFields.h"
#endif

/******************************************************************************
 * CLASS
 ******************************************************************************/

class RequestFields: public LuaObject, public FieldDictionary
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            DEFAULT_TIMEOUT = 600, // seconds
            INVALID_TIMEOUT = -2
        } timeout_settings_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaCreate (lua_State* L);
        static int luaExport (lua_State* L);

        static int luaProjectedPolygonIncludes (lua_State* L) ;
        static int luaRegionMaskIncludes (lua_State* L);
        static int luaGetField (lua_State* L);
        static int luaSetField (lua_State* L);
        static int luaWithArrowOutput (lua_State* L);
        static int luaGetSamplers (lua_State* L);

        bool polyIncludes (double lon, double lat) const;
        bool maskIncludes (double lon, double lat) const;

        #ifdef __geo__
        const GeoFields* geoFields(const char* key) const;
        #endif

        virtual void fromLua (lua_State* L, int index) override;

        RequestFields (lua_State* L, uint64_t key_space, const std::initializer_list<entry_t>& init_list);
        virtual ~RequestFields  (void) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldColumn<MathLib::coord_t>   polygon;
        FieldElement<MathLib::proj_t>   projection          {MathLib::AUTOMATIC};
        FieldElement<int>               pointsInPolygon     {0};
        FieldElement<int>               timeout             {INVALID_TIMEOUT}; // global timeout
        FieldElement<int>               rqstTimeout         {INVALID_TIMEOUT};
        FieldElement<int>               nodeTimeout         {INVALID_TIMEOUT};
        FieldElement<int>               readTimeout         {INVALID_TIMEOUT};
        FieldElement<int>               clusterSizeHint     {0};
        FieldElement<uint64_t>          keySpace            {0};
        RegionMask                      regionMask;
        FieldElement<string>            slideruleVersion;
        FieldElement<string>            buildInformation;
        FieldElement<string>            environmentVersion;

        #ifdef __arrow__
        ArrowFields                     output;
        #endif

        #ifdef __geo__
        FieldMap<GeoFields>             samplers;
        #endif

        MathLib::point_t*               projectedPolygon    {NULL};
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

string convertToJson(const MathLib::coord_t& v);
int convertToLua(lua_State* L, const MathLib::coord_t& v);
void convertFromLua(lua_State* L, int index, MathLib::coord_t& v);

string convertToJson(const MathLib::point_t& v);
int convertToLua(lua_State* L, const MathLib::point_t& v);
void convertFromLua(lua_State* L, int index, MathLib::point_t& v);

string convertToJson(const MathLib::proj_t& v);
int convertToLua(lua_State* L, const MathLib::proj_t& v);
void convertFromLua(lua_State* L, int index, MathLib::proj_t& v);

inline uint32_t toEncoding(MathLib::coord_t& v) { (void)v; return Field::USER; }
inline uint32_t toEncoding(MathLib::point_t& v) { (void)v; return Field::USER; };
inline uint32_t toEncoding(MathLib::proj_t& v) { (void)v; return Field::USER; };

#endif  /* __request_fields__ */
