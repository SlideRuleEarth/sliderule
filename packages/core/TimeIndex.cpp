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

#include "OsApi.h"
#include "AssetIndex.h"
#include "Asset.h"
#include "TimeIndex.h"
#include "StringLib.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset directory>)
 *----------------------------------------------------------------------------*/
int TimeIndex::luaCreate (lua_State* L)
{
    try
    {
        /* Get Asset Directory */
        Asset* _asset = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        int _threshold = getLuaInteger(L, 2, true, DEFAULT_THRESHOLD);

        /* Return AssetIndex Object */
        return createLuaObject(L, new TimeIndex(L, _asset, _threshold));
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
TimeIndex::TimeIndex(lua_State* L, Asset*_asset, int _threshold):
    AssetIndex<timespan_t>(L, *_asset, _threshold)
{
    for(int i = 0; i < asset.size(); i++)
    {
        try 
        {
            timespan_t span;
            span.t0 = asset[i].attributes["t0"];
            span.t1 = asset[i].attributes["t1"];
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
TimeIndex::~TimeIndex(void)
{
}

/*----------------------------------------------------------------------------
 * display
 *----------------------------------------------------------------------------*/
void TimeIndex::display (const timespan_t& span)
{
    mlog(RAW, "[%.3lf, %.3lf]", span.t0, span.t1);
}

/*----------------------------------------------------------------------------
 * split
 *----------------------------------------------------------------------------*/
timespan_t TimeIndex::split (const timespan_t& span)
{
    timespan_t t;
    t.t0 = span.t0;
    t.t1 = (span.t1 + span.t0) / 2.0;
    mlog(RAW, "PREV : "); display(span); mlog(RAW, "  |  ");    
    mlog(RAW, "SPLIT: "); display(t); mlog(RAW, "\n");
    return t;
}

/*----------------------------------------------------------------------------
 * isleft
 *----------------------------------------------------------------------------*/
bool TimeIndex::isleft (const timespan_t& span1, const timespan_t& span2)
{
    return (span1.t1 <= span2.t1);
}


/*----------------------------------------------------------------------------
 * isright
 *----------------------------------------------------------------------------*/
bool TimeIndex::isright (const timespan_t& span1, const timespan_t& span2)
{
    return (span1.t1 >= span2.t1);
}

/*----------------------------------------------------------------------------
 * intersect
 *----------------------------------------------------------------------------*/
bool TimeIndex::intersect (const timespan_t& span1, const timespan_t& span2) 
{ 
    return ((span1.t0 >= span2.t0 && span1.t0 <= span2.t1) ||
            (span1.t1 >= span2.t0 && span1.t1 <= span2.t1) || 
            (span2.t0 >= span1.t0 && span2.t0 <= span1.t1) ||
            (span2.t1 >= span1.t0 && span2.t1 <= span1.t1));
}

/*----------------------------------------------------------------------------
 * combine
 *----------------------------------------------------------------------------*/
timespan_t TimeIndex::combine (const timespan_t& span1, const timespan_t& span2)
{
    timespan_t span;
    span.t0 = MIN(span1.t0, span2.t0);
    span.t1 = MAX(span1.t1, span2.t1);
    return span;
}

/*----------------------------------------------------------------------------
 * luatable2span
 *----------------------------------------------------------------------------*/
timespan_t TimeIndex::luatable2span (lua_State* L, int parm)
{
    timespan_t span = {0.0, 0.0};

    /* Populate Attributes from Table */
    lua_pushnil(L);  // first key
    while (lua_next(L, parm) != 0)
    {
        double value = 0.0;
        bool provided = false;

        const char* key = getLuaString(L, -2);
        const char* str = getLuaString(L, -1, true, NULL, &provided);

        if(!provided) value = getLuaFloat(L, -1);
        else provided = StringLib::str2double(str, &value);

        if(provided)
        {
                 if(StringLib::match("t0",   key)) span.t0 = value;
            else if(StringLib::match("t1",   key)) span.t1 = value;
        }

        lua_pop(L, 1); // removes 'value'; keeps 'key' for next iteration
    }

    return span;
}
