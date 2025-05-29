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
#include "SystemConfig.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* RequestFields::OBJECT_TYPE = "RequestFields";
const char* RequestFields::LUA_META_NAME = "RequestFields";
const struct luaL_Reg RequestFields::LUA_META_TABLE[] = {
    {"export",      luaExport},
    {"encode",      luaEncode},
    {"polygon",     luaProjectedPolygonIncludes},
    {"mask",        luaRegionMaskIncludes},
    {"__index",     luaGetField},
    {"__newindex",  luaSetField},
    {"length",      luaGetLength},
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
        request_fields = new RequestFields(L, 0, NULL, NULL, {});
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
 * luaEncode - encode() --> json
 *----------------------------------------------------------------------------*/
int RequestFields::luaEncode (lua_State* L)
{
    try
    {
        const RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const char* sampler = getLuaString(L, 2, true, NULL);

        if(!sampler)
        {
            const string& json_str = lua_obj->toJson();
            lua_pushstring(L, json_str.c_str());
        }
        else
        {
            const string& json_str = lua_obj->samplers[sampler].toJson();
            lua_pushstring(L, json_str.c_str());
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error exporting %s: %s", OBJECT_TYPE, e.what());
        lua_pushnil(L);
    }

    return 1;
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
 * luaGetLength
 *----------------------------------------------------------------------------*/
int RequestFields::luaGetLength (lua_State* L)
{
    try
    {
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const char* field_name = getLuaString(L, 2);
        const long len = (*lua_obj)[field_name].length();
        lua_pushinteger(L, len);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "error retrieving length: %s", e.what());
        lua_pushinteger(L, 0);
    }

    return 1;
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
        FieldMap<GeoFields>::entry_t entry;
        const char* key = lua_obj->samplers.fields.first(&entry);
        while(key != NULL)
        {
            // create entry of GeoFields
            lua_pushstring(L, key);
            entry.field->toLua(L);
            lua_settable(L, -3);

            // goto next geo field
            key = lua_obj->samplers.fields.next(&entry);
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
    const bool status = false;
    try
    {
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const char* key = getLuaString(L, 2);
        const char* catalog = getLuaString(L, 3);

        // the long form of accessing this variable is because other methods are const
        lua_obj->samplers.fields[key].field->catalog.value = catalog;
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
    if(timeout == IO_INVALID_TIMEOUT)      timeout = SystemConfig::settings().requestTimeoutSec.value;
    if(rqstTimeout == IO_INVALID_TIMEOUT)  rqstTimeout = timeout;
    if(nodeTimeout == IO_INVALID_TIMEOUT)  nodeTimeout = timeout;
    if(readTimeout == IO_INVALID_TIMEOUT)  readTimeout = timeout;

    // project polygon (if necessary)
    pointsInPolygon = polygon.length();
    if(pointsInPolygon.value > 0)
    {
        // determine best projection to use
        if(projection == MathLib::AUTOMATIC_PROJECTION)
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
    environmentVersion = SystemConfig::settings().environmentVersion.value;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RequestFields::RequestFields(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const std::initializer_list<init_entry_t>& init_list):
    LuaObject (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    FieldDictionary ({
        {"asset",               &asset},
        {"resource",            &resource},
        {"resources",           &resources},
        {"poly",                &polygon},
        {"proj",                &projection},
        {"datum",               &datum},
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
        // deprecated
        {"raster",              &regionMask},
    }),
    asset(asset_name)
{
    // set key space
    keySpace = key_space;

    // set resource
    if(_resource)
    {
        resource = _resource;
    }

    // add additional fields to dictionary
    for(const init_entry_t elem: init_list)
    {
        const entry_t entry = {elem.field, false};
        fields.add(elem.name, entry);
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
    if(lua_istable(L, index))
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
    if(lua_istable(L, index))
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
}

/*----------------------------------------------------------------------------
 * convertToJson - MathLib::proj_t
 *----------------------------------------------------------------------------*/
string convertToJson(const MathLib::proj_t& v)
{
    switch(v)
    {
        case MathLib::AUTOMATIC_PROJECTION: return "\"auto\"";
        case MathLib::PLATE_CARREE:         return "\"plate_carree\"";
        case MathLib::NORTH_POLAR:          return "\"north_polar\"";
        case MathLib::SOUTH_POLAR:          return "\"south_polar\"";
        default:                            return "\"unknown\"";
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - MathLib::proj_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const MathLib::proj_t& v)
{
    switch(v)
    {
        case MathLib::AUTOMATIC_PROJECTION: lua_pushstring(L, "auto");          break;
        case MathLib::PLATE_CARREE:         lua_pushstring(L, "plate_carree");  break;
        case MathLib::NORTH_POLAR:          lua_pushstring(L, "north_polar");   break;
        case MathLib::SOUTH_POLAR:          lua_pushstring(L, "south_polar");   break;
        default:                            lua_pushstring(L, "unknown");       break;
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
        if(StringLib::match(proj_str, "auto"))              v = MathLib::AUTOMATIC_PROJECTION;
        else if(StringLib::match(proj_str, "plate_carree")) v = MathLib::PLATE_CARREE;
        else if(StringLib::match(proj_str, "north_polar"))  v = MathLib::NORTH_POLAR;
        else if(StringLib::match(proj_str, "south_polar"))  v = MathLib::SOUTH_POLAR;
   }
}

/*----------------------------------------------------------------------------
 * convertToJson - MathLib::datum_t
 *----------------------------------------------------------------------------*/
string convertToJson(const MathLib::datum_t& v)
{
    switch(v)
    {
        case MathLib::ITRF2014: return "\"ITRF2014\"";
        case MathLib::EGM08:    return "\"EGM08\"";
        case MathLib::NAVD88:   return "\"NAVD88\"";
        default:                return "\"unspecified\"";
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - MathLib::datum_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const MathLib::datum_t& v)
{
    switch(v)
    {
        case MathLib::ITRF2014: lua_pushstring(L, "ITRF2014");      break;
        case MathLib::EGM08:    lua_pushstring(L, "EGM08");         break;
        case MathLib::NAVD88:   lua_pushstring(L, "NAVD88");        break;
        default:                lua_pushstring(L, "unspecified");   break;
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - MathLib::datum_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, MathLib::datum_t& v)
{
    if(lua_isnumber(L, index))
    {
        v = static_cast<MathLib::datum_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* proj_str = LuaObject::getLuaString(L, index);
        if(StringLib::match(proj_str, "unspecified"))   v = MathLib::UNSPECIFIED_DATUM;
        else if(StringLib::match(proj_str, "ITRF2014")) v = MathLib::ITRF2014;
        else if(StringLib::match(proj_str, "EGM08"))    v = MathLib::EGM08;
        else if(StringLib::match(proj_str, "NAVD88"))   v = MathLib::NAVD88;
   }
}
