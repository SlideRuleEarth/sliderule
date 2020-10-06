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
#include "IntervalIndex.h"
#include "StringLib.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <field1>, <field2>, [<threshold>])
 *----------------------------------------------------------------------------*/
int IntervalIndex::luaCreate (lua_State* L)
{
    try
    {
        /* Get Asset Directory */
        Asset*      _asset      = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* _fieldname0 = getLuaString(L, 2);
        const char* _fieldname1 = getLuaString(L, 3);
        int         _threshold  = getLuaInteger(L, 4, true, DEFAULT_THRESHOLD);

        /* Return AssetIndex Object */
        return createLuaObject(L, new IntervalIndex(L, _asset, _fieldname0, _fieldname1, _threshold));
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
IntervalIndex::IntervalIndex(lua_State* L, Asset*_asset, const char* _fieldname0, const char* _fieldname1, int _threshold):
    AssetIndex<intervalspan_t>(L, *_asset, _threshold)
{
    assert(_fieldname0);
    assert(_fieldname1);

    fieldname0 = StringLib::duplicate(_fieldname0);
    fieldname1 = StringLib::duplicate(_fieldname1);

    for(int i = 0; i < asset.size(); i++)
    {
        try 
        {
            intervalspan_t span;
            span.t0 = asset[i].attributes[fieldname0];
            span.t1 = asset[i].attributes[fieldname0];
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
IntervalIndex::~IntervalIndex(void)
{
    delete [] fieldname0;
    delete [] fieldname1;
}

/*----------------------------------------------------------------------------
 * display
 *----------------------------------------------------------------------------*/
void IntervalIndex::display (const intervalspan_t& span)
{
    mlog(RAW, "[%.3lf, %.3lf]", span.t0, span.t1);
}

/*----------------------------------------------------------------------------
 * split
 *----------------------------------------------------------------------------*/
intervalspan_t IntervalIndex::split (const intervalspan_t& span)
{
    intervalspan_t t;
    double sval = (span.t1 + span.t0) / 2.0;
    t.t0 = sval;
    t.t1 = sval;
    mlog(RAW, "PREV : "); display(span); mlog(RAW, "  |  ");    
    mlog(RAW, "SPLIT: "); display(t); mlog(RAW, "\n");
    return t;
}

/*----------------------------------------------------------------------------
 * isleft
 *----------------------------------------------------------------------------*/
bool IntervalIndex::isleft (const intervalspan_t& span1, const intervalspan_t& span2)
{
    return (span1.t1 <= span2.t1);
}


/*----------------------------------------------------------------------------
 * isright
 *----------------------------------------------------------------------------*/
bool IntervalIndex::isright (const intervalspan_t& span1, const intervalspan_t& span2)
{
    return (span1.t1 > span2.t1);
}

/*----------------------------------------------------------------------------
 * intersect
 *----------------------------------------------------------------------------*/
bool IntervalIndex::intersect (const intervalspan_t& span1, const intervalspan_t& span2) 
{ 
    return ((span1.t0 >= span2.t0 && span1.t0 <= span2.t1) ||
            (span1.t1 >= span2.t0 && span1.t1 <= span2.t1) || 
            (span2.t0 >= span1.t0 && span2.t0 <= span1.t1) ||
            (span2.t1 >= span1.t0 && span2.t1 <= span1.t1));
}

/*----------------------------------------------------------------------------
 * combine
 *----------------------------------------------------------------------------*/
intervalspan_t IntervalIndex::combine (const intervalspan_t& span1, const intervalspan_t& span2)
{
    intervalspan_t span;
    span.t0 = MIN(span1.t0, span2.t0);
    span.t1 = MAX(span1.t1, span2.t1);
    return span;
}

/*----------------------------------------------------------------------------
 * luatable2span
 *----------------------------------------------------------------------------*/
intervalspan_t IntervalIndex::luatable2span (lua_State* L, int parm)
{
    intervalspan_t span = {0.0, 0.0};

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
                 if(StringLib::match(fieldname0,   key)) span.t0 = value;
            else if(StringLib::match(fieldname1,   key)) span.t1 = value;
        }

        lua_pop(L, 1); // removes 'value'; keeps 'key' for next iteration
    }

    return span;
}
