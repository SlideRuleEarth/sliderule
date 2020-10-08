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
        int         _threshold  = getLuaInteger(L, 2, true, DEFAULT_THRESHOLD);

        /* Return AssetIndex Object */
        return createLuaObject(L, new SpatialIndex(L, _asset, _threshold));
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
SpatialIndex::SpatialIndex(lua_State* L, Asset* _asset, int _threshold):
    AssetIndex<spatialspan_t>(L, *_asset, _threshold)
{
    for(int i = 0; i < asset.size(); i++)
    {
        try 
        {
            spatialspan_t span;
            span.lat0 = asset[i].attributes["lat0"];
            span.lon0 = asset[i].attributes["lon0"];
            span.lat1 = asset[i].attributes["lat1"];
            span.lon1 = asset[i].attributes["lon1"];
            spans.add(span); // build local list of spans that mirror resource index list
            add(i); // build tree of indexes
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
}

/*----------------------------------------------------------------------------
 * isleft
 *----------------------------------------------------------------------------*/
bool SpatialIndex::isleft (node_t* node, const spatialspan_t& span)
{
    return false;
}


/*----------------------------------------------------------------------------
 * isright
 *----------------------------------------------------------------------------*/
bool SpatialIndex::isright (node_t* node, const spatialspan_t& span)
{
    return false;
}

/*----------------------------------------------------------------------------
 * intersect
 *----------------------------------------------------------------------------*/
bool SpatialIndex::intersect (const spatialspan_t& span1, const spatialspan_t& span2) 
{ 
    // TODO: need intersection code for spatial region
    (void)span1;
    (void)span2;
    return false;
}

/*----------------------------------------------------------------------------
 * combine
 *----------------------------------------------------------------------------*/
spatialspan_t SpatialIndex::combine (const spatialspan_t& span1, const spatialspan_t& span2)
{
    spatialspan_t span;
    span.lat0 = MIN(span1.lat0, span2.lat0);
    span.lon0 = MIN(span1.lon0, span2.lon0);
    span.lat1 = MAX(span1.lat1, span2.lat1);
    span.lon1 = MIN(span1.lon1, span2.lon1);
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
void SpatialIndex::display (const spatialspan_t& span)
{
    mlog(RAW, "[%.3lf, %.3lf, %.3lf, %.3lf]", span.lat0, span.lon0, span.lat1, span.lon1);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * classify
 *----------------------------------------------------------------------------*/
SpatialIndex::proj_t SpatialIndex::classify (spatialspan_t span)
{
    if(span.lat0 >= 0.0)    return NORTH_POLAR;
    else                    return SOUTH_POLAR;
}

/*----------------------------------------------------------------------------
 * project
 * 
 *           --------------------------------------------------------------
 *           |                                                            |
 *           |                                                            |
 *           |                          NORTHERN                          |
 *           |                                                            |
 *           |                                                            |
 * NORTH  60 ---------------------------------------------------------------
 *           |                             |                              |
 *           |                             |                              |
 *           |      WESTERN HEMISPHERE     |      EASTERN HEMISPHERE      |
 *           |                             |                              |
 *           |                             |                              |
 * SOUTH -60 ---------------------------------------------------------------
 *           |                                                            |
 *           |                                                            |
 *           |                          SOUTHERN                          |
 *           |                                                            |
 *           |                                                            |
 *           --------------------------------------------------------------
 *          -180                           0     1    2    3            180
 *----------------------------------------------------------------------------*/
SpatialIndex::coord_t SpatialIndex::project (proj_t p, double lat, double lon)
{
    coord_t coord = { 0.0, 0.0 };

    /* Convert to Radians */
    double xrad = lon * M_PI / 180.0;
    double yrad = lat * M_PI / 180.0;
    double yradp = (M_PI / 4.0) * (yrad / 2.0);

    /* Calculate r */
    double r;
    if(p == NORTH_POLAR)
    {
        r = 2 * tan(yradp);
    }
    else /* if(p == SOUTH_POLAR) */
    {
        r = 2 * tan(-yradp);
    }

    /* Calculate o */
    double o = xrad;

    /* Calculate X */
    coord.x = r * cos(o);

    /* Calculate Y */
    coord.y = r * sin(o);

    /* Return Coordinates */
    return coord;
}

