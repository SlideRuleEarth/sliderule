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
    {"query",       luaQuery},
    {"display",     luaDisplay},
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
    tree = NULL;
}

/*----------------------------------------------------------------------------
 * TimeSpan::Destructor
 *----------------------------------------------------------------------------*/
AssetIndex::TimeSpan::~TimeSpan (void)
{
    // TODO: delete tree
}

/*----------------------------------------------------------------------------
 * TimeSpan::update
 *----------------------------------------------------------------------------*/
bool AssetIndex::TimeSpan::update (int ri)
{
    int maxdepth = 0;
    updatenode(ri, &tree, &maxdepth);
    balancenode(&tree);
    return true;
}

/*----------------------------------------------------------------------------
 * TimeSpan::query
 *----------------------------------------------------------------------------*/
Ordering<int>* AssetIndex::TimeSpan::query (span_t span, Dictionary<double>* attr)
{
    Ordering<int>* list = new Ordering<int>();
    querynode(span, attr, tree, list);
    return list;
}

/*----------------------------------------------------------------------------
 * TimeSpan::display
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::display (void)
{
    displaynode(tree);
}

/*----------------------------------------------------------------------------
 * TimeSpan::updatenode
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::updatenode (int ri, node_t** node, int* maxdepth)
{
    resource_t& resource = asset->resources[ri];
    span_t& span = resource.span;
    int cri; // current resource index

    /* Create Node (if necessary */
    if(*node == NULL)
    {
        *node = new node_t;
        (*node)->ril = new Ordering<int>;
        (*node)->span = span;
        (*node)->before = NULL;
        (*node)->after = NULL;
        (*node)->depth = 0;
    }

    /* Pointer to Current Node */
    node_t* curr = *node;

    /* Update Tree Span */
    if(span.t0 < curr->span.t0) curr->span.t0 = span.t0;
    if(span.t1 > curr->span.t1) curr->span.t1 = span.t1;

    /* Update Current Leaf Node */
    if(curr->ril)
    {
        /* Add Index to Current Node */
        curr->ril->add(span.t1, ri);

        /* Split Current Leaf Node */
        if(curr->ril->length() == NODE_THRESHOLD)
        {
            curr->ril->first(&cri);

            /* Push left in tree - up to middle stop time */
            int middle_index = NODE_THRESHOLD / 2;
            for(int i = 0; i < middle_index; i++)
            {
                updatenode(cri, &curr->before, maxdepth);
                curr->ril->next(&cri);
            }

            /* Push right in tree - from middle stop time */
            for(int i = middle_index; i < NODE_THRESHOLD; i++)
            {
                updatenode(cri, &curr->after, maxdepth);
                curr->ril->next(&cri);
            }

            /* Make Current Node a Branch */
            delete curr->ril;
            curr->ril = NULL;
        }
    }
    else 
    {
        /* Traverse Branch Node */
        if(span.t1 < curr->before->span.t1)
        {   
            /* Update Left Tree */
            updatenode(ri, &curr->before, maxdepth);
        }
        else
        {   
            /* Update Right Tree */
            updatenode(ri, &curr->after, maxdepth);
        }

        /* Update Max Depth */
        (*maxdepth)++;
    }

    /* Update Current Depth */
    if(curr->depth < *maxdepth)
    {
        curr->depth = *maxdepth;
    }
}

/*----------------------------------------------------------------------------
 * TimeSpan::balancenode
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::balancenode (node_t** root)
{
    node_t* curr = *root;

    /* Return on Leaf */
    if(!curr->before || !curr->after) return;

    /* Check if Tree Out of Balance */
    if(curr->before->depth + 1 < curr->after->depth)
    {
        /* Rotate Left:
         *
         *        B                 D
         *      /   \             /   \
         *     A     D    ==>    B     E
         *          / \         / \
         *         C   E       A   C
         */

        /* Recurse on Subtree */
        balancenode(&curr->after);

        /* Build Pointers */
        node_t* A = curr->before;
        node_t* B = curr;
        node_t* C = curr->after->before;
        node_t* D = curr->after;

        /* Rotate */
        B->after = C;
        D->before = B;

        /* Link In */
        *root = D;

        /* Update Span */
        D->span = B->span;
        B->span.t0 = MIN(A->span.t0, C->span.t0);
        B->span.t1 = MAX(A->span.t1, C->span.t1);

        /* Update Depth */
        B->depth = MAX(A->depth, C->depth) + 1;
    }
    else if(curr->after->depth + 1 < curr->before->depth)
    {
        /* Rotate Right:
         *
         *        D                 B
         *      /   \             /   \
         *     B     E    ==>    A     D
         *    / \                     / \
         *   A   C                   C   E
         */

        /* Recurse on Subtree */
        balancenode(&curr->before);

        /* Build Pointers */
        node_t* B = curr->before;
        node_t* C = curr->before->after;
        node_t* D = curr;
        node_t* E = curr->after;

        /* Rotate */
        B->after = D;
        D->before = C;

        /* Link In */
        *root = B;

        /* Update Span */
        B->span = D->span;
        D->span.t0 = MIN(C->span.t0, E->span.t0);
        D->span.t1 = MAX(C->span.t1, E->span.t1);

        /* Update Depth */
        D->depth = MAX(C->depth, E->depth) + 1;
    }
}

/*----------------------------------------------------------------------------
 * TimeSpan::querynode
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::querynode (span_t span, Dictionary<double>* attr, node_t* curr, Ordering<int>* list)
{
    /* Return on Null Path */
    if(curr == NULL) return;

    /* Return if no Intersection with Tree */
    if(!intersect(span, curr->span)) return;

    /* If Leaf Node */
    if(curr->ril)
    {
        /* Populate with Current Node */
        for(int ri, t1 = curr->ril->first(&ri); t1 != (int)INVALID_KEY; t1 = curr->ril->next(&ri))
        {
            resource_t& resource = asset->resources[ri];
            if(intersect(span, resource.span))
            {
                /* Check Field/Value Filter */
                bool add_item = true;
                if(attr)
                {
                    double value;
                    for(const char* field = attr->first(&value); field; field = attr->next(&value))
                    {
                        try
                        {
                            if(resource.attr[field] != value)
                            {
                                add_item = false;
                                break;
                            }
                        }
                        catch(const std::exception& e)
                        {
                            ; // do nothing
                        }
                    }
                }

                /* Add Item */
                if(add_item)
                {
                    list->add(t1, ri, true);
                }
            }
        }
    }
    else /* Branch Node */
    {
        /* Goto Before Tree */
        querynode(span, attr, curr->before, list);

        /* Goto Before Tree */
        querynode(span, attr, curr->after, list);
    }
}

/*----------------------------------------------------------------------------
 * TimeSpan::displaynode
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::displaynode (node_t* curr)
{
    int ri;

    /* Stop */
    if(curr == NULL) return;

    /* Display */
    mlog(RAW, "\n<%d>[%.3lf, %.3lf]: ", curr->depth, curr->span.t0, curr->span.t1);
    if(curr->ril)
    {
        int t1 = curr->ril->first(&ri);
        while(t1 != (int)INVALID_KEY)
        {
            mlog(RAW, "%s ", asset->resources[ri].name);
            t1 = curr->ril->next(&ri);
        }
    }
    else
    {
        mlog(RAW, "B");
        if(curr->before) mlog(RAW, "(%.3lf, %.3lf)", curr->before->span.t0, curr->before->span.t1);
        mlog(RAW, ", A");
        if(curr->after) mlog(RAW, "(%.3lf, %.3lf)", curr->after->span.t0, curr->after->span.t1);
    }
    mlog(RAW, "\n");
    
    /* Recurse */
    displaynode(curr->before);
    displaynode(curr->after);
}

/*----------------------------------------------------------------------------
 * TimeSpan::intersect
 *----------------------------------------------------------------------------*/
bool AssetIndex::TimeSpan::intersect (span_t span1, span_t span2)
{
    return ((span1.t0 >= span2.t0 && span1.t0 <= span2.t1) ||
            (span1.t1 >= span2.t0 && span1.t1 <= span2.t1) || 
            (span2.t0 >= span1.t0 && span2.t0 <= span1.t1) ||
            (span2.t1 >= span1.t0 && span2.t1 <= span1.t1));
}

/*----------------------------------------------------------------------------
 * SpatialRegion::Constructor
 *----------------------------------------------------------------------------*/
AssetIndex::SpatialRegion::SpatialRegion (AssetIndex* _asset)
{
    asset = _asset;
    tree = NULL;
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
        lua_obj->timeIndex.update(ri);

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

/*----------------------------------------------------------------------------
 * luaQuery - :query(<attribute table>)
 *----------------------------------------------------------------------------*/
int AssetIndex::luaQuery (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        AssetIndex* lua_obj = (AssetIndex*)getLuaSelf(L, 1);

        /* Create Query Attributes */
        TimeSpan::span_t            span;   // start, stop
        SpatialRegion::region_t     region; // southern, western, northern, eastern
        Dictionary<double>          attr;   // attributes

        /* Populate Attributes from Table */
        lua_pushnil(L);  // first key
        while (lua_next(L, 2) != 0)
        {
            double value = 0.0;
            bool provided = false;

            const char* key = getLuaString(L, -2);
            const char* str = getLuaString(L, -1, true, NULL, &provided);

            if(!provided) value = getLuaFloat(L, -1);
            else provided = StringLib::str2double(str, &value);

            if(provided)
            {
                     if(StringLib::match("t0",   key)) span.t0     = value;
                else if(StringLib::match("t1",   key)) span.t1     = value;
                else if(StringLib::match("lat0", key)) region.lat0 = value;
                else if(StringLib::match("lat1", key)) region.lat1 = value;
                else if(StringLib::match("lon0", key)) region.lon0 = value;
                else if(StringLib::match("lon1", key)) region.lon1 = value;
                else
                {
                    if(!attr.add(key, value, true))
                    {
                        mlog(CRITICAL, "Failed to populate duplicate attribute %s for query\n", key);
                    }
                }
            }
            else
            {
                mlog(DEBUG, "Unable to populate attribute %s for query\n", key);
            }

            lua_pop(L, 1); // removes 'value'; keeps 'key' for next iteration
        }

        /* Check For Attributes */
        Dictionary<double>* attr_ptr = NULL;        
        if(attr.length() > 0) attr_ptr = &attr;

        /* Query Resources */
        Ordering<int>* ril = lua_obj->timeIndex.query(span, attr_ptr);
        (void)region; // TODO: perform spatial query

        /* Return Resources */
        lua_newtable(L);
        int r = 1;
        int ri, t1 = ril->first(&ri);
        while(t1 != (int)INVALID_KEY)
        {
            lua_pushstring(L, lua_obj->resources[ri].name);
            lua_rawseti(L, -2, r++);
            t1 = ril->next(&ri);
        }

        /* Free Resource Index List */
        delete ril;

        /* Set Status */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error querying: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, 2);
}

/*----------------------------------------------------------------------------
 * luaDisplay - :display(<timetree>, <spacetree>)
 *----------------------------------------------------------------------------*/
int AssetIndex::luaDisplay (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Parameters */
        AssetIndex* lua_obj     = (AssetIndex*)getLuaSelf(L, 1);
        bool display_timetree   = getLuaBoolean(L, 2, true, true);
        bool display_spacetree  = getLuaBoolean(L, 3, true, false);

        /* Display Time Tree */
        if(display_timetree)
        {
            lua_obj->timeIndex.display();
        }

        /* Display Space Tree */
        if(display_spacetree)
        {
            // TODO
        }

        /* Set Status */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error displaying: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
