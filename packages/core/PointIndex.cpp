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
 * STATIC DATA
 ******************************************************************************/

const char* PointIndex::LuaMetaName = "PointIndex";
const struct luaL_Reg PointIndex::LuaMetaTable[] = {
    {"add",         luaAdd},
    {"query",       luaQuery},
    {"display",     luaDisplay},
    {NULL,          NULL}
};

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
    AssetIndex<pointspan_t>(L, *_asset, LuaMetaName, LuaMetaTable, _threshold)
{
    assert(_fieldname);

    fieldname = StringLib::duplicate(_fieldname);

    build();
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
PointIndex::~PointIndex(void)
{
    delete [] fieldname;
}

/*----------------------------------------------------------------------------
 * split
 *----------------------------------------------------------------------------*/
void PointIndex::split (node_t* node, pointspan_t& lspan, pointspan_t& rspan)
{
    double split_val = (node->span.maxval + node->span.minval) / 2.0;
    lspan.minval = node->span.minval;
    lspan.maxval = split_val;
    rspan.minval = split_val;
    rspan.maxval = node->span.maxval;
}

/*----------------------------------------------------------------------------
 * isleft
 *----------------------------------------------------------------------------*/
bool PointIndex::isleft (node_t* node, const pointspan_t& span)
{
    assert(node->left);
    double split_val = node->left->span.maxval;
    return (span.minval <= split_val);
}


/*----------------------------------------------------------------------------
 * isright
 *----------------------------------------------------------------------------*/
bool PointIndex::isright (node_t* node, const pointspan_t& span)
{
    assert(node->right);
    double split_val = node->right->span.minval;
    return (span.maxval >= split_val);
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
 * attr2span
 *----------------------------------------------------------------------------*/
pointspan_t PointIndex::attr2span (Dictionary<double>* attr, bool* provided)
{
    pointspan_t span;
    bool status = false;
    
    try 
    {
        span.maxval = (*attr)[fieldname];
        span.minval = span.maxval;
        status = true;
    }
    catch(std::out_of_range& e)
    {
        mlog(CRITICAL, "Failed to index asset: %s\n", e.what());
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

/*----------------------------------------------------------------------------
 * display
 *----------------------------------------------------------------------------*/
void PointIndex::displayspan (const pointspan_t& span)
{
    mlog(RAW, "[%.3lf, %.3lf]", span.minval, span.maxval);
}
