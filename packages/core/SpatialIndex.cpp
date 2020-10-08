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
    AssetIndex<spatialspan_t>(L, *_asset, _threshold)
{
    projection = _projection;

    for(int i = 0; i < asset.size(); i++)
    {
        try 
        {
            spatialspan_t span;
            span.lat0 = asset[i].attributes["lat0"];
            span.lon0 = asset[i].attributes["lon0"];
            span.lat1 = asset[i].attributes["lat1"];
            span.lon1 = asset[i].attributes["lon1"];
            if( (projection == NORTH_POLAR && span.lat0 >= 0.0) ||
                (projection == SOUTH_POLAR && span.lat0 <  0.0) )
            {
                spans.add(span); // build local list of spans that mirror resource index list
                add(i); // build tree of indexes
            }
        }
        catch(std::out_of_range& e)
        {
            mlog(CRITICAL, "Failed to index asset %s: %s\n", asset.getName(), e.what());
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
    /* Project to Polar Coordinates */
    coord_t coord = project(node->span);

    /* Split Region */
    coord_t lcoord, rcoord;
    if(node->depth % 2 == 0) // even depth
    {
        /* Split Across X-Axis */
        double split_val = (coord.x0 + coord.x1) / 2.0;
        
        lcoord.x0 = coord.x0;
        lcoord.y0 = coord.y0;
        lcoord.x1 = split_val;
        lcoord.y1 = coord.y1;

        rcoord.x0 = split_val;
        rcoord.y0 = coord.y0;
        rcoord.x1 = coord.x1;
        rcoord.y1 = coord.y1;
    }
    else // odd depth
    {
        /* Split Across Y-Axis */
        double split_val = (coord.y0 + coord.y1) / 2.0;
        
        lcoord.x0 = coord.x0;
        lcoord.y0 = split_val;
        lcoord.x1 = coord.x1;
        lcoord.y1 = coord.y1;

        rcoord.x0 = coord.x0;
        rcoord.y0 = coord.y0;
        rcoord.x1 = coord.x1;
        rcoord.y1 = split_val;
    }

    /* Restore to Geographic Coordinates */
    lspan = restore(lcoord);
    rspan = restore(rcoord);
}

/*----------------------------------------------------------------------------
 * isleft
 *----------------------------------------------------------------------------*/
bool SpatialIndex::isleft (node_t* node, const spatialspan_t& span)
{
    assert(node->left);
    assert(node->right);

    /* Project to Polar Coordinates */
    coord_t lcoord = project(node->left->span);
    coord_t rcoord = project(node->right->span);
    coord_t scoord = project(span);

    /* Compare Against Split Value */
    if(node->depth % 2 == 0) // even depth = X-Axis
    {
        double left_val = lcoord.x1;
        double right_val = rcoord.x0;
        double split_val = (left_val + right_val) / 2.0;

        if(scoord.x0 <= split_val)  return true;
        else                        return false;        
    }
    else // odd depth = Y-Axis
    {
        double left_val = lcoord.y1;
        double right_val = rcoord.y0;
        double split_val = (left_val + right_val) / 2.0;

        if(scoord.y0 <= split_val)  return true;
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

    /* Project to Polar Coordinates */
    coord_t lcoord = project(node->left->span);
    coord_t rcoord = project(node->right->span);
    coord_t scoord = project(span);

    /* Compare Against Split Value */
    if(node->depth % 2 == 0) // even depth = X-Axis
    {
        double left_val = lcoord.x1;
        double right_val = rcoord.x0;
        double split_val = (left_val + right_val) / 2.0;

        if(scoord.x1 >= split_val)  return true;
        else                        return false;        
    }
    else // odd depth = Y-Axis
    {
        double left_val = lcoord.y1;
        double right_val = rcoord.y0;
        double split_val = (left_val + right_val) / 2.0;

        if(scoord.y1 >= split_val)  return true;
        else                        return false;        
    }
}

/*----------------------------------------------------------------------------
 * intersect
 *----------------------------------------------------------------------------*/
bool SpatialIndex::intersect (const spatialspan_t& span1, const spatialspan_t& span2) 
{ 
    /* Project to Polar Coordinates */
    coord_t coord1 = project(span1);
    coord_t coord2 = project(span2);    

    /* Check Intersection in X-Axis */
    bool xi = ((coord1.x0 >= coord2.x0 && coord1.x0 <= coord2.x1) ||
               (coord1.x1 >= coord2.x0 && coord1.x1 <= coord2.x1) || 
               (coord2.x0 >= coord1.x0 && coord2.x0 <= coord1.x1) ||
               (coord2.x1 >= coord1.x0 && coord2.x1 <= coord1.x1));

    /* Check Intersection in Y-Axis */
    bool yi = ((coord1.y0 >= coord2.y0 && coord1.y0 <= coord2.y1) ||
               (coord1.y1 >= coord2.y0 && coord1.y1 <= coord2.y1) || 
               (coord2.y0 >= coord1.y0 && coord2.y0 <= coord1.y1) ||
               (coord2.y1 >= coord1.y0 && coord2.y1 <= coord1.y1));

    /* Return Intersection */
    return (xi && yi);
}

/*----------------------------------------------------------------------------
 * combine
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::combine (const spatialspan_t& span1, const spatialspan_t& span2)
{
    /* Project to Polar Coordinates */
    coord_t coord1 = project(span1);
    coord_t coord2 = project(span2);    
    
    /* Combine Spans */
    coord_t coord = {
        .x0 = MIN(coord1.x0, coord2.x0),
        .y0 = MIN(coord1.y0, coord2.y0),
        .x1 = MAX(coord1.x1, coord2.x1),
        .y1 = MIN(coord1.y1, coord2.y1)
    };

    /* Return Geogreaphic Coordinates */
    return restore(coord);
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
void SpatialIndex::display (const spatialspan_t& span)
{
    mlog(RAW, "[%.3lf, %.3lf, %.3lf, %.3lf]", span.lat0, span.lon0, span.lat1, span.lon1);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * project
 *----------------------------------------------------------------------------*/
SpatialIndex::coord_t SpatialIndex::project (spatialspan_t span)
{
    coord_t coord;
    geo2cart(span.lat0, span.lon0, coord.x0, coord.y0);
    geo2cart(span.lat1, span.lon1, coord.x1, coord.y1);
    
    coord_t sorted_coord;
    sorted_coord.x0 = MIN(coord.x0, coord.x1);
    sorted_coord.y0 = MIN(coord.y0, coord.y1);
    sorted_coord.x1 = MAX(coord.x0, coord.x1);
    sorted_coord.y1 = MAX(coord.y0, coord.y1);

    return sorted_coord;
}

/*----------------------------------------------------------------------------
 * restore
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::restore (coord_t coord)
{
    spatialspan_t span;
    cart2geo(span.lat0, span.lon0, coord.x0, coord.y0);
    cart2geo(span.lat1, span.lon1, coord.x1, coord.y1);
    return span;
}

/*----------------------------------------------------------------------------
 * geo2cart
 *----------------------------------------------------------------------------*/
void SpatialIndex::geo2cart (const double lat, const double lon, double& x, double& y)
{
    /* Convert to Radians */
    double lonrad = lon * M_PI / 180.0;
    double latrad = lat * M_PI / 180.0;
    double latradp = (M_PI / 4.0) - (latrad / 2.0);

    /* Calculate r */
    double r = 0.0;
    if(projection == NORTH_POLAR)
    {
        r = 2 * tan(latradp);
    }
    else if(projection == SOUTH_POLAR)
    {
        r = 2 * tan(-latradp);
    }

    /* Calculate o */
    double o = lonrad;

    /* Calculate X */
    x = r * cos(o);

    /* Calculate Y */
    y = r * sin(o);
}

/*----------------------------------------------------------------------------
 * cart2geo
 *----------------------------------------------------------------------------*/
void SpatialIndex::cart2geo (double& lat, double& lon, const double x, const double y)
{
    /* Calculate r */
    double r = sqrt((x*x) + (y*y));

    /* Calculate o */
    double o = asin(y / r);

    /* Calculate Latitude */
    double latradp = atan(r / 2.0);
    double latrad = 90.0;
    if(projection == NORTH_POLAR)
    {
        latrad = (M_PI / 2.0) - (2.0 * latradp);
    }
    else if(projection == SOUTH_POLAR)
    {
        latrad = (M_PI / 2.0) - (2.0 * -latradp);
    }
    
    /* Calculate Longitude */
    double lonrad = o;

    /* Convert to Degress */
    lat = latrad * (180.0 / M_PI);
    lon = lonrad * (180.0 / M_PI);
}
