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
#include "MathLib.h"
#include "SpatialIndex.h"
#include "StringLib.h"
#include "LuaEngine.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* SpatialIndex::LuaMetaName = "SpatialIndex";
const struct luaL_Reg SpatialIndex::LuaMetaTable[] = {
    {"add",         luaAdd},
    {"query",       luaQuery},
    {"display",     luaDisplay},
    {"project",     luaProject},
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
        Asset*          _asset      = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        MathLib::proj_t _projection = (MathLib::proj_t)getLuaInteger(L, 2);
        int             _threshold  = getLuaInteger(L, 3, true, DEFAULT_THRESHOLD);

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
SpatialIndex::SpatialIndex(lua_State* L, Asset* _asset, MathLib::proj_t _projection, int _threshold):
    AssetIndex<spatialspan_t>(L, *_asset, LuaMetaName, LuaMetaTable, _threshold)
{
    projection = _projection;

    build();
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
    projspan_t proj = project(node->span);

    /* Split Region */
    projspan_t lproj, rproj;
    if(node->depth % 2 == 0) // even depth
    {
        /* Split Across Radius */
        double split_val = (proj.p0.x + proj.p1.x) / 2.0;
        
        lproj.p0.x = proj.p0.x;
        lproj.p0.y = proj.p0.y;
        lproj.p1.x = split_val;
        lproj.p1.y = proj.p1.y;

        rproj.p0.x = split_val;
        rproj.p0.y = proj.p0.y;
        rproj.p1.x = proj.p1.x;
        rproj.p1.y = proj.p1.y;
    }
    else // odd depth
    {
        /* Split Across Angle */
        double split_val = (proj.p0.y + proj.p1.y) / 2.0;
        
        lproj.p0.x = proj.p0.x;
        lproj.p0.y = split_val;
        lproj.p1.x = proj.p1.x;
        lproj.p1.y = proj.p1.y;

        rproj.p0.x = proj.p0.x;
        rproj.p0.y = proj.p0.y;
        rproj.p1.x = proj.p1.x;
        rproj.p1.y = split_val;
    }

    /* Restore to Geographic polarinates */
    lspan = restore(lproj);
    rspan = restore(rproj);
}

/*----------------------------------------------------------------------------
 * isleft
 *----------------------------------------------------------------------------*/
bool SpatialIndex::isleft (node_t* node, const spatialspan_t& span)
{
    assert(node->left);
    assert(node->right);

    /* Project to Polar polarinates */
    projspan_t lproj = project(node->left->span);
    projspan_t rproj = project(node->right->span);
    projspan_t sproj = project(span);

    /* Compare Against Split Value */
    if(node->depth % 2 == 0) // even depth = Radius
    {
        double split_val = (lproj.p1.x + rproj.p0.x) / 2.0;

        if(sproj.p0.x <= split_val)  return true;
        else                        return false;        
    }
    else // odd depth = Angle
    {
        double split_val = (lproj.p1.y + rproj.p0.y) / 2.0;

        if(sproj.p0.y <= split_val)  return true;
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
    projspan_t lproj = project(node->left->span);
    projspan_t rproj = project(node->right->span);
    projspan_t sproj = project(span);

    /* Compare Against Split Value */
    if(node->depth % 2 == 0) // even depth = Radius
    {
        double split_val = (lproj.p1.x + rproj.p0.x) / 2.0;

        if(sproj.p1.x >= split_val)  return true;
        else                        return false;        
    }
    else // odd depth = Angle
    {
        double split_val = (lproj.p1.y + rproj.p0.y) / 2.0;

        if(sproj.p1.y >= split_val)  return true;
        else                        return false;        
    }
}

/*----------------------------------------------------------------------------
 * intersect
 *----------------------------------------------------------------------------*/
bool SpatialIndex::intersect (const spatialspan_t& span1, const spatialspan_t& span2) 
{ 
    /* Project to Polar polarinates */
    projspan_t polar1 = project(span1);
    projspan_t polar2 = project(span2);

    /* Check Intersection in Radius */
    bool xi = ((polar1.p0.x >= polar2.p0.x && polar1.p0.x <= polar2.p1.x) ||
               (polar1.p1.x >= polar2.p0.x && polar1.p1.x <= polar2.p1.x) || 
               (polar2.p0.x >= polar1.p0.x && polar2.p0.x <= polar1.p1.x) ||
               (polar2.p1.x >= polar1.p0.x && polar2.p1.x <= polar1.p1.x));

    /* Check Intersection in Angle */
    bool yi = ((polar1.p0.y >= polar2.p0.y && polar1.p0.y <= polar2.p1.y) ||
               (polar1.p1.y >= polar2.p0.y && polar1.p1.y <= polar2.p1.y) || 
               (polar2.p0.y >= polar1.p0.y && polar2.p0.y <= polar1.p1.y) ||
               (polar2.p1.y >= polar1.p0.y && polar2.p1.y <= polar1.p1.y));

    /* Return Intersection */
    return (xi && yi);
}

/*----------------------------------------------------------------------------
 * combine
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::combine (const spatialspan_t& span1, const spatialspan_t& span2)
{
    projspan_t proj;

    /* Project to Polar Coordinates */
    projspan_t polar1 = project(span1);
    projspan_t polar2 = project(span2);    
    
    /* Combine Spans */
    proj.p0.x = MIN(MIN(MIN(polar1.p0.x, polar2.p0.x), polar1.p1.x), polar2.p1.x);
    proj.p0.y = MIN(MIN(MIN(polar1.p0.y, polar2.p0.y), polar1.p1.y), polar2.p1.y);
    proj.p1.x = MAX(MAX(MAX(polar1.p0.x, polar2.p0.x), polar1.p1.x), polar2.p1.x);
    proj.p1.y = MAX(MAX(MAX(polar1.p0.y, polar2.p0.y), polar1.p1.y), polar2.p1.y);

    /* Restore to Geographic Coordinates */
    spatialspan_t span = restore(proj);

    /* Return Coordinates */
    return span;
}

/*----------------------------------------------------------------------------
 * attr2span
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::attr2span (Dictionary<double>* attr, bool* provided)
{
    spatialspan_t span;
    bool status = false;
    
    try 
    {
        span.c0.lat = (*attr)["lat0"];
        span.c0.lon = (*attr)["lon0"];
        span.c1.lat = (*attr)["lat1"];
        span.c1.lon = (*attr)["lon1"];
        if( (projection == MathLib::NORTH_POLAR && span.c0.lat >= 0.0) ||
            (projection == MathLib::SOUTH_POLAR && span.c0.lat <  0.0) )
        {
            status = true;
        }
    }
    catch(std::out_of_range& e)
    {
        mlog(CRITICAL, "Failed to index asset %s\n", e.what());
    }

    if(provided)
    {
        *provided = status;
    }

    return span;
}

/*----------------------------------------------------------------------------
 * luatable2span
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::luatable2span (lua_State* L, int parm)
{
    spatialspan_t span = {{0.0, 0.0}, {0.0, 0.0}};

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
                 if(StringLib::match("lat0", key))   span.c0.lat = value;
            else if(StringLib::match("lon0", key))   span.c0.lon = value;
            else if(StringLib::match("lat1", key))   span.c1.lat = value;
            else if(StringLib::match("lon1", key))   span.c1.lon = value;
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
    projspan_t proj = project(span);
    mlog(RAW, "[%d,%d x %d,%d]", (int)(proj.p0.x*100), (int)(proj.p0.y*100), (int)(proj.p1.x*100), (int)(proj.p1.y*100));
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * project
 *----------------------------------------------------------------------------*/
SpatialIndex::projspan_t SpatialIndex::project (spatialspan_t span)
{
    projspan_t proj;
    
    MathLib::coord2point(span.c0, proj.p0, projection);
    MathLib::coord2point(span.c1, proj.p1, projection);

    double xmin = MIN(proj.p0.x, proj.p1.x);
    double ymin = MIN(proj.p0.y, proj.p1.y);
    double xmax = MAX(proj.p0.x, proj.p1.x);
    double ymax = MAX(proj.p0.y, proj.p1.y);

    proj.p0.x = xmin;
    proj.p0.y = ymin;
    proj.p1.x = xmax;
    proj.p1.y = ymax;

    return proj;
}

/*----------------------------------------------------------------------------
 * restore
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::restore (projspan_t proj)
{
    spatialspan_t span;
    MathLib::point2coord(span.c0, proj.p0, projection);
    MathLib::point2coord(span.c1, proj.p1, projection);
    return span;
}

/*----------------------------------------------------------------------------
 * luaProject: project(<lat>, <lon>)
 *----------------------------------------------------------------------------*/
int SpatialIndex::luaProject (lua_State* L)
{
    try
    {
        /* Get Self */
        SpatialIndex* lua_obj = (SpatialIndex*)getLuaSelf(L, 1);

        /* Get Spherical Coordinates */
        MathLib::coord_t c;
        c.lat = getLuaFloat(L, 2);
        c.lon = getLuaFloat(L, 3);

        /* Convert Coordinates */
        MathLib::point_t p;       
        MathLib::coord2point(c, p, lua_obj->projection);
        lua_pushnumber(L, p.x);
        lua_pushnumber(L, p.y);

        /* Return Coordinates */
        return 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error projecting: %s\n", e.errmsg);
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
        MathLib::point_t p; 
        p.x = getLuaFloat(L, 2);
        p.y = getLuaFloat(L, 3);

        /* Convert Coordinates */
        MathLib::coord_t c;
        MathLib::point2coord(c, p, lua_obj->projection);
        lua_pushnumber(L, c.lat);
        lua_pushnumber(L, c.lon);

        /* Return Coordinates */
        return 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error restoring: %s\n", e.errmsg);
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
        LuaEngine::setAttrNum(L, "lat0", lspan.c0.lat);
        LuaEngine::setAttrNum(L, "lon0", lspan.c0.lon);
        LuaEngine::setAttrNum(L, "lat1", lspan.c1.lat);
        LuaEngine::setAttrNum(L, "lon1", lspan.c1.lon);
        lua_newtable(L);
        LuaEngine::setAttrNum(L, "lat0", rspan.c0.lat);
        LuaEngine::setAttrNum(L, "lon0", rspan.c0.lon);
        LuaEngine::setAttrNum(L, "lat1", rspan.c1.lat);
        LuaEngine::setAttrNum(L, "lon1", rspan.c1.lon);

        /* Return Spans */
        return 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error splitting: %s\n", e.errmsg);
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
        mlog(CRITICAL, "Error intersecting: %s\n", e.errmsg);
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
        LuaEngine::setAttrNum(L, "lat0", span.c0.lat);
        LuaEngine::setAttrNum(L, "lon0", span.c0.lon);
        LuaEngine::setAttrNum(L, "lat1", span.c1.lat);
        LuaEngine::setAttrNum(L, "lon1", span.c1.lon);
        
        /* Return Span */
        return 1;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error combining: %s\n", e.errmsg);
    }

    /* Return Failure */
    return returnLuaStatus(L, false);
}
