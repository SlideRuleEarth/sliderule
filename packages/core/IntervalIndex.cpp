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
#include "IntervalIndex.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* IntervalIndex::LuaMetaName = "IntervalIndex";
const struct luaL_Reg IntervalIndex::LuaMetaTable[] = {
    {"add",         luaAdd},
    {"query",       luaQuery},
    {"display",     luaDisplay},
    {NULL,          NULL}
};

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
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
IntervalIndex::IntervalIndex(lua_State* L, Asset*_asset, const char* _fieldname0, const char* _fieldname1, int _threshold):
    AssetIndex<intervalspan_t>(L, *_asset, LuaMetaName, LuaMetaTable, _threshold)
{
    assert(_fieldname0);
    assert(_fieldname1);

    fieldname0 = StringLib::duplicate(_fieldname0);
    fieldname1 = StringLib::duplicate(_fieldname1);

    build();
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
 * split - assume at least one resource in list
 *----------------------------------------------------------------------------*/
void IntervalIndex::split (node_t* node, intervalspan_t& lspan, intervalspan_t& rspan)
{
    /* Check Resource List */
    assert(node->ril->length() >= 2);

    /* Create List of Endpoints */
    List<double> endpoints;
    for(int i = 0; i < node->ril->length(); i++)
    {
        intervalspan_t span = get(node->ril->get(i));
        endpoints.add(span.t0);
        endpoints.add(span.t1);
    }

    /* Sort Endpoints */
    endpoints.sort();

    /* Calculate Split */
    int midpoint = endpoints.length() / 2;
    lspan.t0 = endpoints[0];
    lspan.t1 = endpoints[midpoint - 1];
    rspan.t0 = endpoints[midpoint];
    rspan.t1 = endpoints[endpoints.length() - 1];
}

/*----------------------------------------------------------------------------
 * isleft
 *----------------------------------------------------------------------------*/
bool IntervalIndex::isleft (node_t* node, const intervalspan_t& span)
{
    assert(node->left);
    assert(node->right);

    double left_val = node->left->span.t1;
    double right_val = node->right->span.t0;
    double split_val = (left_val + right_val) / 2.0;

    if(span.t0 <= split_val)    return true;
    else                        return false;
}

/*----------------------------------------------------------------------------
 * isright
 *----------------------------------------------------------------------------*/
bool IntervalIndex::isright (node_t* node, const intervalspan_t& span)
{
    assert(node->left);
    assert(node->right);

    double left_val = node->left->span.t1;
    double right_val = node->right->span.t0;
    double split_val = (left_val + right_val) / 2.0;

    if(span.t1 >= split_val)    return true;
    else                        return false;
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
 * attr2span
 *----------------------------------------------------------------------------*/
intervalspan_t IntervalIndex::attr2span (Dictionary<double>* attr, bool* provided)
{
    intervalspan_t span;
    bool status = false;
    
    try 
    {
        span.t0 = (*attr)[fieldname0];
        span.t1 = (*attr)[fieldname1];
        status = true;
    }
    catch(RunTimeException& e)
    {
        mlog(e.level(), "Failed to index asset: %s", e.what());
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

/*----------------------------------------------------------------------------
 * display
 *----------------------------------------------------------------------------*/
void IntervalIndex::displayspan (const intervalspan_t& span)
{
    print2term("[%.3lf, %.3lf]", span.t0, span.t1);
}
