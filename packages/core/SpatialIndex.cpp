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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <math.h>

#include "OsApi.h"
#include "AssetIndex.h"
#include "Asset.h"
#include "SpatialIndex.h"
#include "StringLib.h"
#include "LuaEngine.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* SpatialIndex::LuaMetaName = "SpatialIndex";
const struct luaL_Reg SpatialIndex::LuaMetaTable[] = {
    {"polar",       luaPolar},
    {"sphere",      luaSphere},  
    {"split",       luaSplit},
    {"intersect",   luaIntersect},
    {"combine",     luaCombine},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, [<threshold>])
 *----------------------------------------------------------------------------*/
int SpatialIndex::luaCreate (lua_State* L)
{
    try
    {
        /* Get Asset Directory */
        Asset*      _asset      = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        proj_t      _projection = (proj_t)getLuaInteger(L, 2);
        int         _threshold  = getLuaInteger(L, 3, true, DEFAULT_THRESHOLD);

        /* Return AssetIndex Object */
        return createLuaObject(L, new SpatialIndex(L, _asset, _projection, _threshold));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
SpatialIndex::SpatialIndex(lua_State* L, Asset* _asset, proj_t _projection, int _threshold):
    AssetIndex<spatialspan_t>(L, *_asset, LuaMetaName, LuaMetaTable, _threshold)
{
    projection = _projection;

    Asset& spatial_asset = *_asset;
    for(int i = 0; i < spatial_asset.size(); i++)
    {
        try 
        {
            spatialspan_t span;            
            span.lat0 = spatial_asset[i].attributes["lat0"];
            span.lon0 = spatial_asset[i].attributes["lon0"];
            span.lat1 = spatial_asset[i].attributes["lat1"];
            span.lon1 = spatial_asset[i].attributes["lon1"];
            if( (projection == NORTH_POLAR && span.lat0 >= 0.0) ||
                (projection == SOUTH_POLAR && span.lat0 <  0.0) )
            {
                add(span); // build tree of indexes
            }
        }
        catch(std::out_of_range& e)
        {
            mlog(CRITICAL, "Failed to index asset %s: %s\n", spatial_asset.getName(), e.what());
            break;
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
SpatialIndex::~SpatialIndex(void)
{
}

/*----------------------------------------------------------------------------
 * split
 *----------------------------------------------------------------------------*/
void SpatialIndex::split (node_t* node, spatialspan_t& lspan, spatialspan_t& rspan)
{
    /* Project to Polar polarinates */
    polarspan_t polar = project(node->span);

    /* Split Region */
    polarspan_t lpolar, rpolar;
    if(node->depth % 2 == 0) // even depth
    {
        /* Split Across Radius */
        double split_val = (polar.x0 + polar.x1) / 2.0;
        
        lpolar.x0 = polar.x0;
        lpolar.y0 = polar.y0;
        lpolar.x1 = split_val;
        lpolar.y1 = polar.y1;

        rpolar.x0 = split_val;
        rpolar.y0 = polar.y0;
        rpolar.x1 = polar.x1;
        rpolar.y1 = polar.y1;
    }
    else // odd depth
    {
        /* Split Across Angle */
        double split_val = (polar.y0 + polar.y1) / 2.0;
        
        lpolar.x0 = polar.x0;
        lpolar.y0 = split_val;
        lpolar.x1 = polar.x1;
        lpolar.y1 = polar.y1;

        rpolar.x0 = polar.x0;
        rpolar.y0 = polar.y0;
        rpolar.x1 = polar.x1;
        rpolar.y1 = split_val;
    }

    /* Restore to Geographic polarinates */
    lspan = restore(lpolar);
    rspan = restore(rpolar);
}

/*----------------------------------------------------------------------------
 * isleft
 *----------------------------------------------------------------------------*/
bool SpatialIndex::isleft (node_t* node, const spatialspan_t& span)
{
    assert(node->left);
    assert(node->right);

    /* Project to Polar polarinates */
    polarspan_t lpolar = project(node->left->span);
    polarspan_t rpolar = project(node->right->span);
    polarspan_t spolar = project(span);

    /* Compare Against Split Value */
    if(node->depth % 2 == 0) // even depth = Radius
    {
        double split_val = (lpolar.x1 + rpolar.x0) / 2.0;

        if(spolar.x0 <= split_val)  return true;
        else                        return false;        
    }
    else // odd depth = Angle
    {
        double split_val = (lpolar.y1 + rpolar.y0) / 2.0;

        if(spolar.y0 <= split_val)  return true;
        else                        return false;        
    }
}


/*----------------------------------------------------------------------------
 * isright
 *----------------------------------------------------------------------------*/
bool SpatialIndex::isright (node_t* node, const spatialspan_t& span)
{
    assert(node->left);
    assert(node->right);

    /* Project to Polar polarinates */
    polarspan_t lpolar = project(node->left->span);
    polarspan_t rpolar = project(node->right->span);
    polarspan_t spolar = project(span);

    /* Compare Against Split Value */
    if(node->depth % 2 == 0) // even depth = Radius
    {
        double split_val = (lpolar.x1 + rpolar.x0) / 2.0;

        if(spolar.x1 >= split_val)  return true;
        else                        return false;        
    }
    else // odd depth = Angle
    {
        double split_val = (lpolar.y1 + rpolar.y0) / 2.0;

        if(spolar.y1 >= split_val)  return true;
        else                        return false;        
    }
}

/*----------------------------------------------------------------------------
 * intersect
 *----------------------------------------------------------------------------*/
bool SpatialIndex::intersect (const spatialspan_t& span1, const spatialspan_t& span2) 
{ 
    /* Project to Polar polarinates */
    polarspan_t polar1 = project(span1);
    polarspan_t polar2 = project(span2);

    /* Check Intersection in Radius */
    bool xi = ((polar1.x0 >= polar2.x0 && polar1.x0 <= polar2.x1) ||
               (polar1.x1 >= polar2.x0 && polar1.x1 <= polar2.x1) || 
               (polar2.x0 >= polar1.x0 && polar2.x0 <= polar1.x1) ||
               (polar2.x1 >= polar1.x0 && polar2.x1 <= polar1.x1));

    /* Check Intersection in Angle */
    bool yi = ((polar1.y0 >= polar2.y0 && polar1.y0 <= polar2.y1) ||
               (polar1.y1 >= polar2.y0 && polar1.y1 <= polar2.y1) || 
               (polar2.y0 >= polar1.y0 && polar2.y0 <= polar1.y1) ||
               (polar2.y1 >= polar1.y0 && polar2.y1 <= polar1.y1));

    /* Return Intersection */
    return (xi && yi);
}

/*----------------------------------------------------------------------------
 * combine
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::combine (const spatialspan_t& span1, const spatialspan_t& span2)
{
    polarspan_t polar;

    /* Project to Polar Coordinates */
    polarspan_t polar1 = project(span1);
    polarspan_t polar2 = project(span2);    
    
    /* Combine Spans */
    polar.x0 = MIN(MIN(MIN(polar1.x0, polar2.x0), polar1.x1), polar2.x1);
    polar.y0 = MIN(MIN(MIN(polar1.y0, polar2.y0), polar1.y1), polar2.y1);
    polar.x1 = MAX(MAX(MAX(polar1.x0, polar2.x0), polar1.x1), polar2.x1);
    polar.y1 = MAX(MAX(MAX(polar1.y0, polar2.y0), polar1.y1), polar2.y1);

    /* Restore to Geographic Coordinates */
    spatialspan_t span = restore(polar);

    /* Return Coordinates */
    return span;
}

/*----------------------------------------------------------------------------
 * luatable2span
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::luatable2span (lua_State* L, int parm)
{
    spatialspan_t span = {0.0, 0.0, 0.0, 0.0};

    /* Populate Attributes from Table */
    lua_pushnil(L);  // first key
    while(lua_next(L, parm) != 0)
    {
        double value = 0.0;
        bool provided = false;

        const char* key = getLuaString(L, -2);
        const char* str = getLuaString(L, -1, true, NULL, &provided);

        if(!provided) value = getLuaFloat(L, -1);
        else provided = StringLib::str2double(str, &value);

        if(provided)
        {
                 if(StringLib::match("lat0", key))   span.lat0 = value;
            else if(StringLib::match("lon0", key))   span.lon0 = value;
            else if(StringLib::match("lat1", key))   span.lat1 = value;
            else if(StringLib::match("lon1", key))   span.lon1 = value;
        }

        lua_pop(L, 1); // removes 'value'; keeps 'key' for next iteration
    }

    return span;
}

/*----------------------------------------------------------------------------
 * display
 *----------------------------------------------------------------------------*/
void SpatialIndex::displayspan (const spatialspan_t& span)
{
    polarspan_t polar = project(span);
    mlog(RAW, "[%d,%d x %d,%d]", (int)(polar.x0*100), (int)(polar.y0*100), (int)(polar.x1*100), (int)(polar.y1*100));
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * project
 *----------------------------------------------------------------------------*/
SpatialIndex::polarspan_t SpatialIndex::project (spatialspan_t span)
{
    polarspan_t polar;
    
    geo2polar(span.lat0, span.lon0, polar.x0, polar.y0);
    geo2polar(span.lat1, span.lon1, polar.x1, polar.y1);

    double xmin = MIN(polar.x0, polar.x1);
    double ymin = MIN(polar.y0, polar.y1);
    double xmax = MAX(polar.x0, polar.x1);
    double ymax = MAX(polar.y0, polar.y1);

    polar.x0 = xmin;
    polar.y0 = ymin;
    polar.x1 = xmax;
    polar.y1 = ymax;

    return polar;
}

/*----------------------------------------------------------------------------
 * restore
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::restore (polarspan_t polar)
{
    spatialspan_t span;
    polar2geo(span.lat0, span.lon0, polar.x0, polar.y0);
    polar2geo(span.lat1, span.lon1, polar.x1, polar.y1);
    return span;
}

/*----------------------------------------------------------------------------
 * geo2polar
 *----------------------------------------------------------------------------*/
void SpatialIndex::geo2polar (const double lat, const double lon, double& x, double& y)
{
    double r = 0.0, o = 0.0;

    /* Convert to Radians */
    double lonrad = lon * M_PI / 180.0;
    double latrad = lat * M_PI / 180.0;

    /* Calculate r */
    if(projection == NORTH_POLAR)
    {
        double latradp = (M_PI / 4.0) - (latrad / 2.0);
        r = 2 * tan(latradp);
    }
    else if(projection == SOUTH_POLAR)
    {
        double latradp = -(M_PI / 4.0) - (latrad / 2.0);
        r = -2 * tan(latradp);
    }

    /* Calculate o */
    if(projection == NORTH_POLAR)
    {
        o = lonrad;        
    }
    else if(projection == SOUTH_POLAR)
    {
        o = -lonrad;
    }

    /* Calculate X */
    x = r * cos(o);

    /* Calculate Y */
    y = r * sin(o);
}

/*----------------------------------------------------------------------------
 * polar2geo
 *----------------------------------------------------------------------------*/
void SpatialIndex::polar2geo (double& lat, double& lon, const double x, const double y)
{
    double latrad = 90.0, lonrad = 0.0;

    /* Calculate r */
    double r = sqrt((x*x) + (y*y));

    /* Calculate o */
    double o = 0.0;
    if(x != 0.0)
    {
        o = atan(y / x);

        /* Adjust for Quadrants */
        if(x < 0.0 && y >= 0.0) o += M_PI;
        else if(x < 0.0 && y < 0.0) o -= M_PI;
    }
    else
    {
        /* PI/2 or -PI/2 */
        o = asin(y / r);
    }

    /* Calculate Latitude */
    if(projection == NORTH_POLAR)
    {
        double latradp = atan(r / 2.0);
        latrad = (M_PI / 2.0) - (2.0 * latradp);
    }
    else if(projection == SOUTH_POLAR)
    {
        double latradp = atan(r / -2.0);
        latrad = (-2.0 * latradp) - (M_PI / 2.0);
    }
    
    /* Calculate Longitude */
    if(projection == NORTH_POLAR)
    {
        lonrad = o;
    }
    else if(projection == SOUTH_POLAR)
    {
        lonrad = -o;
    }

    /* Convert to Degress */
    lat = latrad * (180.0 / M_PI);
    lon = lonrad * (180.0 / M_PI);
}

/*----------------------------------------------------------------------------
 * luaPolar: polar(<lat>, <lon>)
 *----------------------------------------------------------------------------*/
int SpatialIndex::luaPolar (lua_State* L)
{
    try
    {
        /* Get Self */
        SpatialIndex* lua_obj = (SpatialIndex*)getLuaSelf(L, 1);

        /* Get Spherical Coordinates */
        double lat = getLuaFloat(L, 2);
        double lon = getLuaFloat(L, 3);

        /* Convert Coordinates */
        double x, y;        
        lua_obj->geo2polar(lat, lon, x, y);
        lua_pushnumber(L, x);
        lua_pushnumber(L, y);

        /* Return Coordinates */
        return 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error converting to polar: %s\n", e.errmsg);
    }

    /* Return Failure */
    return returnLuaStatus(L, false);
}

/*----------------------------------------------------------------------------
 * luaSphere - sphere(<x>, <y>)
 *----------------------------------------------------------------------------*/
int SpatialIndex::luaSphere (lua_State* L)
{
    try
    {
        /* Get Self */
        SpatialIndex* lua_obj = (SpatialIndex*)getLuaSelf(L, 1);

        /* Get Polar Coordinates */
        double x = getLuaFloat(L, 2);
        double y = getLuaFloat(L, 3);

        /* Convert Coordinates */
        double lat, lon;
        lua_obj->polar2geo(lat, lon, x, y);
        lua_pushnumber(L, lat);
        lua_pushnumber(L, lon);

        /* Return Coordinates */
        return 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error converting to polar: %s\n", e.errmsg);
    }

    /* Return Failure */
    return returnLuaStatus(L, false);
}

/*----------------------------------------------------------------------------
 * luaSplit
 *----------------------------------------------------------------------------*/
int SpatialIndex::luaSplit (lua_State* L)
{
    try
    {
        /* Get Self */
        SpatialIndex* lua_obj = (SpatialIndex*)getLuaSelf(L, 1);

        /* Get Spatial Span */
        spatialspan_t span = lua_obj->luatable2span(L, 2);
        int depth = getLuaInteger(L, 3, true, 0);

        /* Build Temporary Node (to split) */
        node_t node;
        node.span = span;
        node.depth = depth;
        node.left = NULL;
        node.right = NULL;
        node.ril = NULL;

        /* Split Span */
        spatialspan_t lspan, rspan;
        lua_obj->split(&node, lspan, rspan);

        /* Return Spans */
        lua_newtable(L);
        LuaEngine::setAttrNum(L, "lat0", lspan.lat0);
        LuaEngine::setAttrNum(L, "lon0", lspan.lon0);
        LuaEngine::setAttrNum(L, "lat1", lspan.lat1);
        LuaEngine::setAttrNum(L, "lon1", lspan.lon1);
        lua_newtable(L);
        LuaEngine::setAttrNum(L, "lat0", rspan.lat0);
        LuaEngine::setAttrNum(L, "lon0", rspan.lon0);
        LuaEngine::setAttrNum(L, "lat1", rspan.lat1);
        LuaEngine::setAttrNum(L, "lon1", rspan.lon1);

        /* Return Spans */
        return 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error converting to polar: %s\n", e.errmsg);
    }

    /* Return Failure */
    return returnLuaStatus(L, false);
}

/*----------------------------------------------------------------------------
 * luaIntersect
 *----------------------------------------------------------------------------*/
int SpatialIndex::luaIntersect (lua_State* L)
{
    try
    {
        /* Get Self */
        SpatialIndex* lua_obj = (SpatialIndex*)getLuaSelf(L, 1);

        /* Get Spatial Spans */
        spatialspan_t span1 = lua_obj->luatable2span(L, 2);
        spatialspan_t span2 = lua_obj->luatable2span(L, 3);

        /* Get Intersection */
        bool intersect = lua_obj->intersect(span1, span2);
        lua_pushboolean(L, intersect);

        /* Return Intersection */
        return 1;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error converting to polar: %s\n", e.errmsg);
    }

    /* Return Failure */
    return returnLuaStatus(L, false);
}

/*----------------------------------------------------------------------------
 * luaCombine
 *----------------------------------------------------------------------------*/
int SpatialIndex::luaCombine (lua_State* L)
{
    try
    {
        /* Get Self */
        SpatialIndex* lua_obj = (SpatialIndex*)getLuaSelf(L, 1);

        /* Get Spatial Spans */
        spatialspan_t span1 = lua_obj->luatable2span(L, 2);
        spatialspan_t span2 = lua_obj->luatable2span(L, 3);

        /* Combine Spans */
        spatialspan_t span = lua_obj->combine(span1, span2);

        /* Return Span */
        lua_newtable(L);
        LuaEngine::setAttrNum(L, "lat0", span.lat0);
        LuaEngine::setAttrNum(L, "lon0", span.lon0);
        LuaEngine::setAttrNum(L, "lat1", span.lat1);
        LuaEngine::setAttrNum(L, "lon1", span.lon1);
        
        /* Return Span */
        return 1;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error converting to polar: %s\n", e.errmsg);
    }

    /* Return Failure */
    return returnLuaStatus(L, false);
}
