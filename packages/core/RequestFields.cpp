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

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "OsApi.h"
#include "RequestFields.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* RequestFields::OBJECT_TYPE = "RequestFields";
const char* RequestFields::LUA_META_NAME = "RequestFields";
const struct luaL_Reg RequestFields::LUA_META_TABLE[] = {
    {"export",      luaExport},
    {"poly",        luaProjectedPolygonIncludes},
    {"mask",        luaRegionMaskIncludes},
    {"__index",     luaGetField},
    {"__newindex",  luaSetField},
    {"hasoutput",   luaWithArrowOutput},
    {"samplers",    luaGetSamplers},
    {"withsamplers",luaWithSamplers},
    {"setcatalog",  luaSetCatalog},
    {NULL,          NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int RequestFields::luaCreate (lua_State* L)
{
    RequestFields* request_fields = NULL;
    try
    {
        request_fields = new RequestFields(L, 0, {});
        request_fields->fromLua(L, 1);
        return createLuaObject(L, request_fields);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        delete request_fields;
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaExport - export() --> lua table
 *----------------------------------------------------------------------------*/
int RequestFields::luaExport (lua_State* L)
{
    int num_rets = 1;

    try
    {
        const RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const char* sampler = getLuaString(L, 2, true, NULL);

        if(!sampler)
        {
            num_rets = lua_obj->toLua(L);
        }
        else
        {
            lua_obj->samplers[sampler].toLua(L);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error exporting %s: %s", OBJECT_TYPE, e.what());
        lua_pushnil(L);
    }

    return num_rets;
}

/*----------------------------------------------------------------------------
 * luaProjectedPolygonIncludes - poly(lon, lat)
 *----------------------------------------------------------------------------*/
int RequestFields::luaProjectedPolygonIncludes (lua_State* L)
{
    try
    {
        const RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const double lon = getLuaFloat(L, 2);
        const double lat = getLuaFloat(L, 3);
        const bool includes = lua_obj->polyIncludes(lon, lat);
        lua_pushboolean(L, includes);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error testing for inclusion in polygon: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaRegionMaskIncludes - mask(lon, lat)
 *----------------------------------------------------------------------------*/
int RequestFields::luaRegionMaskIncludes (lua_State* L)
{
    try
    {
        const RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const double lon = getLuaFloat(L, 2);
        const double lat = getLuaFloat(L, 3);
        const bool includes = lua_obj->maskIncludes(lon, lat);
        lua_pushboolean(L, includes);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error testing for inclusion in region mask: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaGetField - [<field_name>]
 *----------------------------------------------------------------------------*/
int RequestFields::luaGetField (lua_State* L)
{
    try
    {
        const RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const char* field_name = getLuaString(L, 2);

        // check the metatable for the key (to support functions)
        luaL_getmetatable(L, LUA_META_NAME);
        lua_pushstring(L, field_name);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1))  return 1;
        else lua_pop(L, 1);

        // handle field access
        return lua_obj->fields[field_name].field->toLua(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(DEBUG, "unable to retrieve field: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaSetField - [<field_name>]
 *----------------------------------------------------------------------------*/
int RequestFields::luaSetField (lua_State* L)
{
    try
    {
        const RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const char* field_name = getLuaString(L, 2);
        lua_obj->fields[field_name].field->fromLua(L, 3);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "error retrieving field: %s", e.what());
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * luaWithArrowOutput
 *----------------------------------------------------------------------------*/
int RequestFields::luaWithArrowOutput (lua_State* L)
{
    try
    {
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        lua_pushboolean(L, !lua_obj->output.path.value.empty());
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "error retrieving field: %s", e.what());
        lua_pushboolean(L, false);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaGetSamplers
 *----------------------------------------------------------------------------*/
int RequestFields::luaGetSamplers (lua_State* L)
{
    try
    {
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));

        // create table of GeoFields
        lua_newtable(L);

        // loop through each GeoFields
        GeoFields* geo_field;
        const char* key = lua_obj->samplers.values.first(&geo_field);
        while(key != NULL)
        {
            // create entry of GeoFields
            lua_pushstring(L, key);
            geo_field->toLua(L);
            lua_settable(L, -3);

            // goto next geo field
            key = lua_obj->samplers.values.next(&geo_field);
        }

        // return table
        return 1;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "error retrieving samplers: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaWithSamplers
 *----------------------------------------------------------------------------*/
int RequestFields::luaWithSamplers (lua_State* L)
{
    bool status = false;
    try
    {
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        status = lua_obj->samplers.length() > 0;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "error checking samplers: %s", e.what());
    }
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaSetCatalog
 *----------------------------------------------------------------------------*/
int RequestFields::luaSetCatalog (lua_State* L)
{
    bool status = false;
    try
    {
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const char* key = getLuaString(L, 2);
        const char* catalog = getLuaString(L, 3);

        // the long form of accessing this variable is because other methods are const
        lua_obj->samplers.values[key]->catalog.value = catalog;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "error checking samplers: %s", e.what());
    }
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * polyIncludes
 *----------------------------------------------------------------------------*/
bool RequestFields::polyIncludes (double lon, double lat) const
{
    // project coordinate
    const MathLib::coord_t coord = {lon, lat};
    const MathLib::point_t point = MathLib::coord2point(coord, projection.value);

    // test inside polygon
    if(MathLib::inpoly(projectedPolygon, pointsInPolygon.value, point))
    {
        return true;
    }

    // not inside polygon
    return false;
}

/*----------------------------------------------------------------------------
 * maskIncludes
 *----------------------------------------------------------------------------*/
bool RequestFields::maskIncludes (double lon, double lat) const
{
    return regionMask.includes(lon, lat);
}

/*----------------------------------------------------------------------------
 * geoFields
 *----------------------------------------------------------------------------*/
#ifdef __geo__
const GeoFields* RequestFields::geoFields(const char* key) const
{
    return &samplers[key];
}
#endif

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void RequestFields::fromLua (lua_State* L, int index)
{
    FieldDictionary::fromLua(L, index);

    // set timeouts (if necessary)
    if(timeout == INVALID_TIMEOUT)      timeout = DEFAULT_TIMEOUT;
    if(rqstTimeout == INVALID_TIMEOUT)  rqstTimeout = timeout;
    if(nodeTimeout == INVALID_TIMEOUT)  nodeTimeout = timeout;
    if(readTimeout == INVALID_TIMEOUT)  readTimeout = timeout;

    // project polygon (if necessary)
    pointsInPolygon = polygon.length();
    if(pointsInPolygon.value > 0)
    {
        // determine best projection to use
        if(projection == MathLib::AUTOMATIC)
        {
            if(polygon[0].lat > 70.0) projection = MathLib::NORTH_POLAR;
            else if(polygon[0].lat < -70.0) projection = MathLib::SOUTH_POLAR;
            else projection = MathLib::PLATE_CARREE;
        }

        // project polygon
        projectedPolygon = new MathLib::point_t [pointsInPolygon.value];
        for(int i = 0; i < pointsInPolygon.value; i++)
        {
            projectedPolygon[i] = MathLib::coord2point(polygon[i], projection.value);
        }
    }

    // version info
    slideruleVersion = LIBID;
    buildInformation = BUILDINFO;
    environmentVersion = OsApi::getEnvVersion();
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RequestFields::RequestFields(lua_State* L, uint64_t key_space, const std::initializer_list<entry_t>& init_list):
    LuaObject (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    FieldDictionary ({
        {"poly",                &polygon},
        {"projection",          &projection},
        {"points_in_polygon",   &pointsInPolygon},
        {"timeout",             &timeout},
        {"rqst_timeout",        &rqstTimeout},
        {"node_timeout",        &nodeTimeout},
        {"read_timeout",        &readTimeout},
        {"cluster_size_hint",   &clusterSizeHint},
        {"key_space",           &keySpace},
        {"region_mask",         &regionMask},
        {"sliderule_version",   &slideruleVersion},
        {"build_information",   &buildInformation},
        {"environment_version", &environmentVersion},
        #ifdef __arrow__
        {ArrowFields::PARMS,    &output},
        #endif
        #ifdef __geo__
        {GeoFields::PARMS,      &samplers},
        #endif
    })
{
    // set key space
    keySpace = key_space;

    // add additional fields to dictionary
    for(const entry_t elem: init_list)
    {
        fields.add(elem.name, elem);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
RequestFields::~RequestFields(void)
{
    delete [] projectedPolygon;
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * convertToJson - MathLib::coord_t
 *----------------------------------------------------------------------------*/
string convertToJson(const MathLib::coord_t& v)
{
    return FString("{\"lon\":%lf,\"lat\":%lf}", v.lon, v.lat).c_str();
}

/*----------------------------------------------------------------------------
 * convertToLua - MathLib::coord_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const MathLib::coord_t& v)
{
    lua_newtable(L);

    // longitude
    lua_pushstring(L, "lon");
    lua_pushnumber(L, v.lon);
    lua_settable(L, -3);

    // latitude
    lua_pushstring(L, "lat");
    lua_pushnumber(L, v.lat);
    lua_settable(L, -3);

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - MathLib::coord_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, MathLib::coord_t& v)
{
    // longitude
    lua_getfield(L, index, "lon");
    v.lon = LuaObject::getLuaFloat(L, -1);
    lua_pop(L, 1);

    // latitude
    lua_getfield(L, index, "lat");
    v.lat = LuaObject::getLuaFloat(L, -1);
    lua_pop(L, 1);
}

/*----------------------------------------------------------------------------
 * convertToJson - MathLib::point_t
 *----------------------------------------------------------------------------*/
string convertToJson(const MathLib::point_t& v)
{
    return FString("{\"x\":%lf,\"y\":%lf}", v.x, v.y).c_str();
}

/*----------------------------------------------------------------------------
 * convertToLua - MathLib::point_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const MathLib::point_t& v)
{
    lua_newtable(L);

    // x
    lua_pushnumber(L, v.x);
    lua_rawseti(L, -2, 1);

    // y
    lua_pushnumber(L, v.y);
    lua_rawseti(L, -2, 2);

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - MathLib::point_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, MathLib::point_t& v)
{
    // x
    lua_rawgeti(L, index, 1);
    v.x = LuaObject::getLuaInteger(L, -1);
    lua_pop(L, 1);

    // y
    lua_rawgeti(L, index, 1);
    v.y = LuaObject::getLuaInteger(L, -1);
    lua_pop(L, 1);
}

/*----------------------------------------------------------------------------
 * convertToJson - MathLib::proj_t
 *----------------------------------------------------------------------------*/
string convertToJson(const MathLib::proj_t& v)
{
    switch(v)
    {
        case MathLib::AUTOMATIC:    return "\"auto\"";
        case MathLib::PLATE_CARREE: return "\"plate_carree\"";
        case MathLib::NORTH_POLAR:  return "\"north_polar\"";
        case MathLib::SOUTH_POLAR:  return "\"south_polar\"";
        default:                    return "\"unknown\"";
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - MathLib::proj_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const MathLib::proj_t& v)
{
    switch(v)
    {
        case MathLib::AUTOMATIC:    lua_pushstring(L, "auto");          break;
        case MathLib::PLATE_CARREE: lua_pushstring(L, "plate_carree");  break;
        case MathLib::NORTH_POLAR:  lua_pushstring(L, "north_polar");   break;
        case MathLib::SOUTH_POLAR:  lua_pushstring(L, "south_polar");   break;
        default:                    lua_pushstring(L, "unknown");       break;
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - MathLib::proj_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, MathLib::proj_t& v)
{
    if(lua_isnumber(L, index))
    {
        v = static_cast<MathLib::proj_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* proj_str = LuaObject::getLuaString(L, index);
        if(StringLib::match(proj_str, "auto"))              v = MathLib::AUTOMATIC;
        else if(StringLib::match(proj_str, "plate_carree")) v = MathLib::PLATE_CARREE;
        else if(StringLib::match(proj_str, "north_polar"))  v = MathLib::NORTH_POLAR;
        else if(StringLib::match(proj_str, "south_polar"))  v = MathLib::SOUTH_POLAR;
   }
}
