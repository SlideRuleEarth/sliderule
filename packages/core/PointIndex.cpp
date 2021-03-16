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
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s", LuaMetaName, e.what());
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
        mlog(CRITICAL, "Failed to index asset: %s", e.what());
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
    print2term("[%.3lf, %.3lf]", span.minval, span.maxval);
}
