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
#include "PointIndex.h"
#include "StringLib.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <fieldname>, [<threshold>])
 *----------------------------------------------------------------------------*/
int PointIndex::luaCreate (lua_State* L)
{
    try
    {
        /* Get Asset Directory */
        Asset*      _asset      = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* _fieldname  = getLuaString(L, 2);
        int         _threshold  = getLuaInteger(L, 3, true, DEFAULT_THRESHOLD);

        /* Return AssetIndex Object */
        return createLuaObject(L, new PointIndex(L, _asset, _fieldname, _threshold));
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
PointIndex::PointIndex(lua_State* L, Asset* _asset, const char* _fieldname, int _threshold):
    AssetIndex<pointspan_t>(L, *_asset, _threshold)
{
    assert(_fieldname);

    fieldname = StringLib::duplicate(_fieldname);
    for(int i = 0; i < asset.size(); i++)
    {
        try 
        {
            pointspan_t span;
            span.maxval = asset[i].attributes[fieldname];
            span.minval = span.maxval;
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
PointIndex::~PointIndex(void)
{
    delete [] fieldname;
}

/*----------------------------------------------------------------------------
 * display
 *----------------------------------------------------------------------------*/
void PointIndex::display (const pointspan_t& span)
{
    mlog(RAW, "[%.3lf, %.3lf]", span.minval, span.maxval);
}

/*----------------------------------------------------------------------------
 * split
 *----------------------------------------------------------------------------*/
pointspan_t PointIndex::split (const pointspan_t& span)
{
    pointspan_t f;
    f.minval = span.minval;
    f.maxval = (span.maxval + span.minval) / 2.0;
    return f;
}

/*----------------------------------------------------------------------------
 * isleft
 *----------------------------------------------------------------------------*/
bool PointIndex::isleft (const pointspan_t& span1, const pointspan_t& span2)
{
    return (span1.maxval <= span2.maxval);
}


/*----------------------------------------------------------------------------
 * isright
 *----------------------------------------------------------------------------*/
bool PointIndex::isright (const pointspan_t& span1, const pointspan_t& span2)
{
    return (span1.maxval >= span2.maxval);
}

/*----------------------------------------------------------------------------
 * intersect
 *----------------------------------------------------------------------------*/
bool PointIndex::intersect (const pointspan_t& span1, const pointspan_t& span2) 
{ 
    return ((span1.minval >= span2.minval && span1.minval <= span2.maxval) ||
            (span1.maxval >= span2.minval && span1.maxval <= span2.maxval) || 
            (span2.minval >= span1.minval && span2.minval <= span1.maxval) ||
            (span2.maxval >= span1.minval && span2.maxval <= span1.maxval));
}

/*----------------------------------------------------------------------------
 * combine
 *----------------------------------------------------------------------------*/
pointspan_t PointIndex::combine (const pointspan_t& span1, const pointspan_t& span2)
{
    pointspan_t span;
    span.minval = MIN(span1.minval, span2.minval);
    span.maxval = MAX(span1.maxval, span2.maxval);
    return span;
}

/*----------------------------------------------------------------------------
 * luatable2span
 *----------------------------------------------------------------------------*/
pointspan_t PointIndex::luatable2span (lua_State* L, int parm)
{
    pointspan_t span = {0.0, 0.0};

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
            if(StringLib::match(fieldname, key))
            {
                span.maxval = value;
                span.minval = value;
            }
        }

        lua_pop(L, 1); // removes 'value'; keeps 'key' for next iteration
    }

    return span;
}
