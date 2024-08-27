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
#include "geo.h"
#include "NetsvcParms.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* NetsvcParms::SELF               = "netsvc";
const char* NetsvcParms::POLYGON            = "poly";
const char* NetsvcParms::RASTER             = "raster";
const char* NetsvcParms::LATITUDE           = "lat";
const char* NetsvcParms::LONGITUDE          = "lon";
const char* NetsvcParms::PROJECTION         = "proj";
const char* NetsvcParms::RQST_TIMEOUT       = "rqst-timeout";
const char* NetsvcParms::NODE_TIMEOUT       = "node-timeout";
const char* NetsvcParms::READ_TIMEOUT       = "read-timeout";
const char* NetsvcParms::GLOBAL_TIMEOUT     = "timeout";
const char* NetsvcParms::CLUSTER_SIZE_HINT  = "cluster_size_hint";

const char* NetsvcParms::OBJECT_TYPE = "NetsvcParms";
const char* NetsvcParms::LUA_META_NAME = "NetsvcParms";
const struct luaL_Reg NetsvcParms::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int NetsvcParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Network service parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new NetsvcParms(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * tojson
 *----------------------------------------------------------------------------*/
const char* NetsvcParms::tojson(void) const
{
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    rapidjson::Value nullval(rapidjson::kNullType);

#ifdef __geo__
    if(raster._raster) doc.AddMember("raster", rapidjson::Value(raster._raster->getJsonString(), allocator), allocator);
    else       doc.AddMember("raster", nullval, allocator);
#endif

    doc.AddMember("rqst_timeout", rapidjson::Value(rqst_timeout), allocator);
    doc.AddMember("node_timeout", rapidjson::Value(node_timeout), allocator);
    doc.AddMember("read_timeout", rapidjson::Value(read_timeout), allocator);
    doc.AddMember("cluster_size_hint", rapidjson::Value(cluster_size_hint), allocator);

    doc.AddMember("projection", rapidjson::Value(MathLib::proj2str(projection), allocator), allocator);

    /* Polygon is a list of coordinates */
    doc.AddMember("points_in_poly", rapidjson::Value(points_in_poly), allocator);
    if(points_in_poly > 0)
    {
        rapidjson::Value _poly(rapidjson::kArrayType);
        const List<MathLib::coord_t>::Iterator poly_iterator(polygon);
        for(int i = 0; i < points_in_poly; i++)
        {
            rapidjson::Value _coord(rapidjson::kObjectType);
            _coord.AddMember("lon", poly_iterator[i].lon, allocator);
            _coord.AddMember("lat", poly_iterator[i].lat, allocator);
            _poly.PushBack(_coord, allocator);
        }
        doc.AddMember("polygon", _poly, allocator);
    }
    else doc.AddMember("polygon", rapidjson::Value("[]"), allocator);

    /* User should not care about 'projected_poly, it is private data */

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return StringLib::duplicate(buffer.GetString());
}

/*----------------------------------------------------------------------------
 * includes
 *----------------------------------------------------------------------------*/
bool NetsvcParms::RasterImpl::includes(double lon, double lat) const  // NOLINT(readability-convert-member-functions-to-static)
{
    /*
     * This function must be defined in the source file to ensure that
     * the correct behavior is preserved in plugins. Plugins do not have
     * the __geo__ define, and inlining this function in the header file would
     * result in false being returned.
     */
#ifdef __geo__
    return _raster->includes(lon, lat);
#else
    (void)lon;
    (void)lat;
    return false;
#endif
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
NetsvcParms::NetsvcParms(lua_State* L, int index):
    LuaObject                   (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    rqst_timeout                (DEFAULT_RQST_TIMEOUT),
    node_timeout                (DEFAULT_NODE_TIMEOUT),
    read_timeout                (DEFAULT_READ_TIMEOUT),
    cluster_size_hint           (DEFAULT_CLUSTER_SIZE_HINT),
    projection                  (MathLib::AUTOMATIC),
    projected_poly              (NULL),
    points_in_poly              (0)
{
    bool provided = false;

    try
    {
        /* Polygon */
        lua_getfield(L, index, NetsvcParms::POLYGON);
        get_lua_polygon(L, -1, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d points", NetsvcParms::POLYGON, polygon.length());
        lua_pop(L, 1);

        /* Raster */
        lua_getfield(L, index, NetsvcParms::RASTER);
        get_lua_raster(L, -1, &provided);
        if(provided) mlog(INFO, "Setting %s file for use", NetsvcParms::RASTER);
        lua_pop(L, 1);

        /* Projection */
        lua_getfield(L, index, NetsvcParms::PROJECTION);
        get_lua_projection(L, -1, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", NetsvcParms::PROJECTION, projection);
        lua_pop(L, 1);

        /* Global Timeout */
        lua_getfield(L, index, NetsvcParms::GLOBAL_TIMEOUT);
        const int global_timeout = LuaObject::getLuaInteger(L, -1, true, 0, &provided);
        if(provided)
        {
            rqst_timeout = global_timeout;
            node_timeout = global_timeout;
            read_timeout = global_timeout;
            mlog(DEBUG, "Setting %s to %d", NetsvcParms::RQST_TIMEOUT, global_timeout);
            mlog(DEBUG, "Setting %s to %d", NetsvcParms::NODE_TIMEOUT, global_timeout);
            mlog(DEBUG, "Setting %s to %d", NetsvcParms::READ_TIMEOUT, global_timeout);
        }
        lua_pop(L, 1);

        /* Request Timeout */
        lua_getfield(L, index, NetsvcParms::RQST_TIMEOUT);
        rqst_timeout = LuaObject::getLuaInteger(L, -1, true, rqst_timeout, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", NetsvcParms::RQST_TIMEOUT, rqst_timeout);
        lua_pop(L, 1);

        /* Node Timeout */
        lua_getfield(L, index, NetsvcParms::NODE_TIMEOUT);
        node_timeout = LuaObject::getLuaInteger(L, -1, true, node_timeout, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", NetsvcParms::NODE_TIMEOUT, node_timeout);
        lua_pop(L, 1);

        /* Read Timeout */
        lua_getfield(L, index, NetsvcParms::READ_TIMEOUT);
        read_timeout = LuaObject::getLuaInteger(L, -1, true, read_timeout, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", NetsvcParms::READ_TIMEOUT, read_timeout);
        lua_pop(L, 1);

        /* Cluster Size Hint */
        lua_getfield(L, index, NetsvcParms::CLUSTER_SIZE_HINT);
        cluster_size_hint = LuaObject::getLuaInteger(L, -1, true, cluster_size_hint, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", NetsvcParms::CLUSTER_SIZE_HINT, cluster_size_hint);
        lua_pop(L, 1);

        /* Process Area of Interest */
        points_in_poly = polygon.length();
        if(points_in_poly > 0)
        {
            /* Determine Best Projection To Use */
            if(projection == MathLib::AUTOMATIC)
            {
                if(polygon[0].lat > 70.0) projection = MathLib::NORTH_POLAR;
                else if(polygon[0].lat < -70.0) projection = MathLib::SOUTH_POLAR;
                else projection = MathLib::PLATE_CARREE;
            }

            /* Project Polygon */
            const List<MathLib::coord_t>::Iterator poly_iterator(polygon);
            projected_poly = new MathLib::point_t [points_in_poly];
            for(int i = 0; i < points_in_poly; i++)
            {
                projected_poly[i] = MathLib::coord2point(poly_iterator[i], projection);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw; // rethrow exception
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
NetsvcParms::~NetsvcParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void NetsvcParms::cleanup (void) const
{
    delete [] projected_poly;
}

/*----------------------------------------------------------------------------
 * get_lua_polygon
 *----------------------------------------------------------------------------*/
void NetsvcParms::get_lua_polygon (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    if(provided) *provided = false;

    /* Must be table of coordinates */
    if(lua_istable(L, index))
    {
        /* Get Number of Points in Polygon */
        const int num_points = lua_rawlen(L, index);

        /* Iterate through each coordinate */
        for(int i = 0; i < num_points; i++)
        {
            /* Get coordinate table */
            lua_rawgeti(L, index, i+1);
            if(lua_istable(L, index))
            {
                MathLib::coord_t coord;

                /* Get longitude entry */
                lua_getfield(L, index, NetsvcParms::LONGITUDE);
                coord.lon = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Get latitude entry */
                lua_getfield(L, index, NetsvcParms::LATITUDE);
                coord.lat = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Add Coordinate */
                polygon.add(coord);
                if(provided) *provided = true;
            }
            lua_pop(L, 1);
        }
    }
}

/*----------------------------------------------------------------------------
 * get_lua_raster
 *----------------------------------------------------------------------------*/
void NetsvcParms::get_lua_raster (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of
     * {
     *      data = <geojson file>,
     *      cellsize = <cellsize>
     * }
     */
#ifdef __geo__
    if(lua_istable(L, index))
    {
        try
        {
            raster._raster = GeoJsonRaster::create(L, index);
            *provided = true;
        }
        catch(const RunTimeException& e)
        {
            mlog(e.level(), "Error creating GeoJsonRaster file: %s", e.what());
        }
    }
#endif
}

/*----------------------------------------------------------------------------
 * get_lua_projection
 *----------------------------------------------------------------------------*/
void NetsvcParms::get_lua_projection (lua_State* L, int index, bool* provided)
{
    *provided = false;
    if(lua_isnumber(L, index))
    {
        projection = static_cast<MathLib::proj_t>(LuaObject::getLuaInteger(L, index, true, projection, provided));
    }
    else if(lua_isstring(L, index))
    {
        const char* proj_str = LuaObject::getLuaString(L, index, true, "auto", provided);
        if(*provided)
        {
            if(StringLib::match(proj_str, "auto")) projection = MathLib::AUTOMATIC;
            else if(StringLib::match(proj_str, "plate_carree")) projection = MathLib::PLATE_CARREE;
            else if(StringLib::match(proj_str, "north_polar")) projection = MathLib::NORTH_POLAR;
            else if(StringLib::match(proj_str, "south_polar")) projection = MathLib::SOUTH_POLAR;
        }
    }
}
