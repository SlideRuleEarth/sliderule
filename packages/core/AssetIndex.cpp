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

#include "AssetIndex.h"
#include "Dictionary.h"
#include "List.h"
#include "Ordering.h"
#include "LogLib.h"
#include "OsApi.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Dictionary<AssetIndex*> AssetIndex::assets;
Mutex                   AssetIndex::assetsMut;

const char* AssetIndex::OBJECT_TYPE = "AssetIndex";
const char* AssetIndex::LuaMetaName = "AssetIndex";
const struct luaL_Reg AssetIndex::LuaMetaTable[] = {
    {"info",        luaInfo},
    {"load",        luaLoad},
    {NULL,          NULL}
};


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<name>, [<format>, <url>])
 *----------------------------------------------------------------------------*/
int AssetIndex::luaCreate (lua_State* L)
{
    try
    {
        bool alias = false;

        /* Get Required Parameters */
        const char* name = getLuaString(L, 1);

        /* Determine if AssetIndex Exists */
        AssetIndex* asset = NULL;
        assetsMut.lock();
        {
            if(assets.find(name))
            {
                asset = assets.get(name);
                associateMetaTable(L, LuaMetaName, LuaMetaTable);
                alias = true;
            }
        }
        assetsMut.unlock();

        /* Check if AssetIndex Needs to be Created */
        if(asset == NULL)
        {
            const char* format      = getLuaString(L, 2);
            const char* url         = getLuaString(L, 3);
            asset = new AssetIndex(L, name, format, url);
        }        

        /* Return AssetIndex Object */
        return createLuaObject(L, asset, alias);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * TimeSpan::Constructor
 *----------------------------------------------------------------------------*/
AssetIndex::TimeSpan::TimeSpan (AssetIndex* _asset)
{
    asset = _asset;
    root = NULL;
}

/*----------------------------------------------------------------------------
 * TimeSpan::Destructor
 *----------------------------------------------------------------------------*/
AssetIndex::TimeSpan::~TimeSpan (void)
{
    // TODO: delete tree
}

/*----------------------------------------------------------------------------
 * TimeSpan::add
 *----------------------------------------------------------------------------*/
bool AssetIndex::TimeSpan::add (int ri)
{
    /* Add to Root */
    updatenode(ri, &root);
    
    /* Rebalance */
    balancetree(root);

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * TimeSpan::query
 *----------------------------------------------------------------------------*/
Ordering<int>* AssetIndex::TimeSpan::query (span_t span)
{
    Ordering<int>* list = new Ordering<int>();
    traverse(span, root, list);
    return list;
}

/*----------------------------------------------------------------------------
 * TimeSpan::display - public
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::display (void)
{
    display(root);
}

/*----------------------------------------------------------------------------
 * TimeSpan::updatenode
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::updatenode (int ri, node_t** curr)
{
    resource_t& resource = asset->resources[ri];
    span_t& span = resource.span;

    /* Create Node (if necessary */
    if(*curr == NULL)
    {
        *curr = new node_t;
        (*curr)->treespan = span;
        (*curr)->nodespan = span;
        (*curr)->before = NULL;
        (*curr)->after = NULL;
    }

    /* Update Node */
    (*curr)->ril.add(span.t1, ri);
    if(span.t0 < (*curr)->treespan.t0) (*curr)->treespan.t0 = span.t0;
    if(span.t0 < (*curr)->nodespan.t0) (*curr)->nodespan.t0 = span.t0;
    if(span.t1 > (*curr)->treespan.t1) (*curr)->treespan.t1 = span.t1;
    if(span.t1 > (*curr)->nodespan.t1) (*curr)->nodespan.t1 = span.t1;
}

/*----------------------------------------------------------------------------
 * TimeSpan::balancetree
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::balancetree (node_t* curr)
{
    int ri;

    if(curr && curr->ril.length() > NODE_THRESHOLD)
    {
        /* Go to first resource index in list */
        curr->ril.first(&ri);

        /* Push left in tree - up to middle stop time */
        int middle_index = NODE_THRESHOLD / 2;
        for(int i = 0; i < middle_index; i++)
        {
            updatenode(ri, &curr->before);
            curr->ril.next(&ri);
        }

        /* Push right in tree - from middle stop time */
        for(int i = middle_index; i < NODE_THRESHOLD; i++)
        {
            updatenode(ri, &curr->after);
            curr->ril.next(&ri);
        }

        /* Clear current list */
        curr->ril.clear();
        LocalLib::set(&curr->nodespan, 0, sizeof(curr->nodespan));

        /* Rebalance children */
        balancetree(curr->before);
        balancetree(curr->after);
    }
}

/*----------------------------------------------------------------------------
 * TimeSpan::traverse
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::traverse (span_t span, node_t* curr, Ordering<int>* list)
{
    /* Return on Null Path */
    if(curr == NULL) return;

    /* Return if no Intersection with Tree */
    if(!intersect(span, curr->treespan)) return;

    /* Populate with Current Node */
    populate(span, curr, list);

    /* Goto Before Tree */
    traverse(span, curr->before, list);

    /* Goto Before Tree */
    traverse(span, curr->after, list);
}

/*----------------------------------------------------------------------------
 * TimeSpan::traverse
 *----------------------------------------------------------------------------*/
int AssetIndex::TimeSpan::populate (span_t span, node_t* curr, Ordering<int>* list)
{
    int num_added = 0;
    if(intersect(span, curr->nodespan))
    {
        int ri;
        int t1 = curr->ril.first(&ri);
        while(t1 != (int)INVALID_KEY)
        {
            resource_t& resource = asset->resources[ri];
            if(intersect(span, resource.span))
            {
                if(list->add(ri, t1, true))
                {
                    num_added++;
                }
            }
            t1 = curr->ril.next(&ri);
        }
    }
    return num_added;
}

/*----------------------------------------------------------------------------
 * TimeSpan::traverse
 *----------------------------------------------------------------------------*/
bool AssetIndex::TimeSpan::intersect (span_t span1, span_t span2)
{
    return ((span1.t0 >= span2.t0 && span1.t0 < span2.t1) ||
            (span1.t1 >= span2.t0 && span1.t1 < span2.t1));
}

/*----------------------------------------------------------------------------
 * TimeSpan::display - private
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::display (node_t* curr)
{
    int ri;

    /* Stop */
    if(curr == NULL) return;

    /* Display */
    mlog(RAW, "\n[%.3lf, %.3lf]: ", curr->treespan.t0, curr->treespan.t1);
    int t1 = curr->ril.first(&ri);
    while(t1 != (int)INVALID_KEY)
    {
        mlog(RAW, "%s ", asset->resources[ri].name);
        t1 = curr->ril.next(&ri);
    }
    mlog(RAW, "\n");
    mlog(RAW, "B");
    if(curr->before) mlog(RAW, "(%.3lf, %.3lf)", curr->before->treespan.t0, curr->before->treespan.t1);
    mlog(RAW, ", A");
    if(curr->after) mlog(RAW, "(%.3lf, %.3lf)", curr->after->treespan.t0, curr->after->treespan.t1);
    mlog(RAW, "\n");
    
    /* Recurse */
    display(curr->before);
    display(curr->after);
}
/*----------------------------------------------------------------------------
 * SpatialRegion::Constructor
 *----------------------------------------------------------------------------*/
AssetIndex::SpatialRegion::SpatialRegion (AssetIndex* _asset)
{
    asset = _asset;
    root = NULL;
}

/*----------------------------------------------------------------------------
 * SpatialRegion::Destructor
 *----------------------------------------------------------------------------*/
AssetIndex::SpatialRegion::~SpatialRegion (void)
{
}

/*----------------------------------------------------------------------------
 * SpatialRegion::add
 *----------------------------------------------------------------------------*/
bool AssetIndex::SpatialRegion::add (int ri)
{
    (void)ri;
    return true;
}

/*----------------------------------------------------------------------------
 * SpatialRegion::query
 *----------------------------------------------------------------------------*/
List<int>* AssetIndex::SpatialRegion::query (region_t region)
{
    (void)region;
    return NULL;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
AssetIndex::AssetIndex (lua_State* L, const char* _name, const char* _format, const char* _url):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    timeIndex(this),
    spatialIndex(this)
{
    /* Configure LuaObject Name */
    ObjectName  = StringLib::duplicate(_name);

    /* Initialize Members */
    name        = StringLib::duplicate(_name);
    format      = StringLib::duplicate(_format);
    url         = StringLib::duplicate(_url);

    /* Register AssetIndex */
    assetsMut.lock();
    {
        AssetIndex* asset = this;
        if(assets.add(name, asset, true))
        {
            registered = true;
        }
        else
        {
            registered = false;
            mlog(CRITICAL, "Failed to register asset %s\n", name);
        }
    }
    assetsMut.unlock();
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
AssetIndex::~AssetIndex (void)
{
    /* Remove Asset from Dictionary */
    if(registered)
    {
        registered = false;
        assetsMut.lock();
        {
            assets.remove(name);
        }
        assetsMut.unlock();
    }

    /* Delete Members */
    delete [] name;
    delete [] format;
    delete [] url;
}


/*----------------------------------------------------------------------------
 * luaInfo - :info() --> name, format, url
 *----------------------------------------------------------------------------*/
int AssetIndex::luaInfo (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        AssetIndex* lua_obj = (AssetIndex*)getLuaSelf(L, 1);

        /* Push Info */
        lua_pushlstring(L, lua_obj->name,   StringLib::size(lua_obj->name));
        lua_pushlstring(L, lua_obj->format, StringLib::size(lua_obj->format));
        lua_pushlstring(L, lua_obj->url,    StringLib::size(lua_obj->url));

        /* Set Status */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error retrieving asset: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, 4);
}

/*----------------------------------------------------------------------------
 * luaLoad - :load(resource, attributes) --> boolean status
 *----------------------------------------------------------------------------*/
int AssetIndex::luaLoad (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        AssetIndex* lua_obj = (AssetIndex*)getLuaSelf(L, 1);

        /* Get Resource */
        const char* resource_name = getLuaString(L, 2);

        /* Create Resource */
        resource_t resource;
        StringLib::copy(&resource.name[0], resource_name, RESOURCE_NAME_MAX_LENGTH);
        LocalLib::set(&resource.span, 0, sizeof(resource.span));
        LocalLib::set(&resource.region, 0, sizeof(resource.region));

        /* Populate Attributes from Table */
        lua_pushnil(L);  // first key
        while (lua_next(L, 3) != 0)
        {
            double value = 0.0;
            bool provided = false;

            const char* key = getLuaString(L, -2);
            const char* str = getLuaString(L, -1, true, NULL, &provided);

            if(!provided) value = getLuaFloat(L, -1);
            else provided = StringLib::str2double(str, &value);

            if(provided)
            {
                     if(StringLib::match("t0",   key)) resource.span.t0     = value;
                else if(StringLib::match("t1",   key)) resource.span.t1     = value;
                else if(StringLib::match("lat0", key)) resource.region.lat0 = value;
                else if(StringLib::match("lat1", key)) resource.region.lat1 = value;
                else if(StringLib::match("lon0", key)) resource.region.lon0 = value;
                else if(StringLib::match("lon1", key)) resource.region.lon1 = value;
                else
                {
                    if(!resource.attr.add(key, value, true))
                    {
                        mlog(CRITICAL, "Failed to populate duplicate attribute %s for resource %s\n", key, resource_name);
                    }
                }
            }
            else
            {
                mlog(DEBUG, "Unable to populate attribute %s for resource %s\n", key, resource_name);
            }

            lua_pop(L, 1); // removes 'value'; keeps 'key' for next iteration
        }

        /* Register Resource */
        int ri = lua_obj->resources.add(resource);
        lua_obj->timeIndex.add(ri);
        lua_obj->timeIndex.display();

        /* Set Status */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error loading resource: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
