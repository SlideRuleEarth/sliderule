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
#include "FieldElement.h"
#include "FieldColumn.h"
#include "FieldMap.h"
#include "AssetField.h"
#include "RegionMask.h"
#include "MathLib.h"
#include "OutputFields.h"
#include "SystemConfig.h"

#ifdef __geo__
#include "GeoFields.h"
#endif

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define REQUEST_INVALID_TIMEOUT -1

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreateParameters - parms(<parameter table>)
 *----------------------------------------------------------------------------*/
template <class T>
int luaCreateParameters (lua_State* L)
{
    T* parms = NULL;

    try
    {
        const uint64_t key_space = LuaObject::getLuaInteger(L, 2, true, INVALID_KEY);
        const char* asset_name = LuaObject::getLuaString(L, 3, true, NULL);
        const char* _resource = LuaObject::getLuaString(L, 4, true, NULL);

        parms = new T(L, key_space, asset_name, _resource);
        parms->fromLua(L, 1);

        return LuaObject::createLuaObject(L, parms);
    }
    catch(const RunTimeException& e)
    {
        delete parms;
        mlog(e.level(), "Error creating parameters: %s", e.what());
        return LuaObject::returnLuaStatus(L, false);
    }
}


 /******************************************************************************
 * CLASS
 ******************************************************************************/

class RequestParameters: public LuaObject, public FieldMap<Field>
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaExport (lua_State* L); // to Lua
        static int luaEncode (lua_State* L); // to JSON
        static int luaDescribe (lua_State* L); // to OpenAPI

        static int luaProjectedPolygonIncludes (lua_State* L) ;
        static int luaRegionMaskIncludes (lua_State* L);
        static int luaGetField (lua_State* L);
        static int luaSetField (lua_State* L);
        static int luaGetLength (lua_State* L);
        static int luaHasOutput (lua_State* L);
        static int luaHasArrowOutput (lua_State* L);
        static int luaHasLasOutput (lua_State* L);
        static int luaGetSamplers (lua_State* L);
        static int luaWithSamplers (lua_State* L);
        static int luaSetCatalog (lua_State* L);

        bool addParameter (const char* name, Field* field, const char* description);
        bool polyIncludes (double lon, double lat) const;
        bool maskIncludes (double lon, double lat) const;

        #ifdef __geo__
        const GeoFields* geoFields(const char* key) const;
        #endif

        virtual void fromLua (lua_State* L, int index) override;

        RequestParameters (lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* object_type = OBJECT_TYPE);
        virtual ~RequestParameters  (void) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        AssetField                      asset;              // name of Asset in asset dictionary to use for granules
        FieldElement<string>            resource;           // granule name (including file extension)
        FieldList<string>               resources;          // list of granule names
        FieldColumn<MathLib::coord_t>   polygon;
        FieldElement<int>               maxResources        {SystemConfig::settings().requestMaxResources.value};
        FieldElement<MathLib::proj_t>   projection          {MathLib::AUTOMATIC_PROJECTION};
        FieldElement<MathLib::datum_t>  datum               {MathLib::UNSPECIFIED_DATUM};
        FieldElement<int>               pointsInPolygon     {0};
        FieldElement<int>               timeout             {REQUEST_INVALID_TIMEOUT}; // global timeout
        FieldElement<int>               rqstTimeout         {REQUEST_INVALID_TIMEOUT};
        FieldElement<int>               nodeTimeout         {REQUEST_INVALID_TIMEOUT};
        FieldElement<int>               readTimeout         {REQUEST_INVALID_TIMEOUT};
        FieldElement<int>               clusterSizeHint     {0};
        FieldElement<uint64_t>          keySpace            {INVALID_KEY};
        RegionMask                      regionMask;
        FieldElement<string>            slideruleVersion;
        FieldElement<string>            buildInformation;
        FieldElement<string>            environmentVersion;
        OutputFields                    output;

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

string convertToJson(const MathLib::datum_t& v);
int convertToLua(lua_State* L, const MathLib::datum_t& v);
void convertFromLua(lua_State* L, int index, MathLib::datum_t& v);

inline uint32_t toEncoding(MathLib::coord_t& v) { (void)v; return Field::OBJECT; }
inline uint32_t toEncoding(MathLib::point_t& v) { (void)v; return Field::OBJECT; };
inline uint32_t toEncoding(MathLib::proj_t& v) { (void)v; return Field::OBJECT; };
inline uint32_t toEncoding(MathLib::datum_t& v) { (void)v; return Field::OBJECT; };

inline FieldUntypedColumn::column_t toDoubles(const FieldColumn<MathLib::coord_t>& v, long start_index, long num_elements) {
    (void)v;
    (void)start_index;
    (void)num_elements;
    throw RunTimeException(CRITICAL, RTE_FAILURE, "column format <coord_t> does not support conversion to doubles");
}

#endif  /* __request_fields__ */
