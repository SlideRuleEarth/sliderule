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
    {"export",  luaExport},
    {"poly",    luaProjectedPolygonIncludes},
    {"mask",    luaRegionMaskIncludes},
    {"__index", luaGetField},
    {NULL,      NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int RequestFields::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new RequestFields(L, 1, {}));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
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
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        num_rets = lua_obj->toLua(L);
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
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
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
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
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
        RequestFields* lua_obj = dynamic_cast<RequestFields*>(getLuaSelf(L, 1));
        const char* field_name = getLuaString(L, 2);
        return lua_obj->fields[field_name].field->toLua(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "error retrieving field: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
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
    return regionMask.value.includes(lon, lat);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
RequestFields::RequestFields(lua_State* L, int index, const std::initializer_list<entry_t>& init_list):
    LuaObject (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    FieldDictionary ({  
        {"polygon",             &polygon},
        {"projection",          &projection},
        {"points_in_polygon",   &pointsInPolygon},
        {"timeout",             &timeout},
        {"rqst_timeout",        &rqstTimeout},
        {"node_timeout",        &nodeTimeout},
        {"read_timeout",        &readTimeout},
        {"cluster_size_hint",   &clusterSizeHint},
        {"region_mask",         &regionMask},
        #ifdef __arrow__
        {"output",              &output},
        #endif
    })
{
    // add additional fields to dictionary
    for(const entry_t elem: init_list) 
    {
        fields.add(elem.name, elem);
    }

    // read in from Lua
    fromLua(L, index);

    // set timeouts (if necessary)
    if(timeout.value == TIMEOUT_UNSET)      timeout.value = DEFAULT_TIMEOUT;    
    if(rqstTimeout.value == TIMEOUT_UNSET)  rqstTimeout.value = timeout.value;
    if(nodeTimeout.value == TIMEOUT_UNSET)  nodeTimeout.value = timeout.value;
    if(readTimeout.value == TIMEOUT_UNSET)  readTimeout.value = timeout.value;

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
