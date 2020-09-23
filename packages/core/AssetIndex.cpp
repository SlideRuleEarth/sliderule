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
AssetIndex::TimeSpan::TimeSpan (doube _t0, double _t1)
{
    t0 = _t0;
    t1 = _t1;
}

/*----------------------------------------------------------------------------
 * TimeSpan::Destructor
 *----------------------------------------------------------------------------*/
AssetIndex::TimeSpan::~TimeSpan (void) { }

/*----------------------------------------------------------------------------
 * TimeSpan::getkey
 *----------------------------------------------------------------------------*/
double AssetIndex::TimeSpan::getkey (void) 
{ 
    return t1;
}

/*----------------------------------------------------------------------------
 * TimeSpan::intersect
 *----------------------------------------------------------------------------*/
bool AssetIndex::TimeSpan::intersect (const Span& other) 
{ 
    return ((t0 >= other.t0 && t0 <= other.t1) ||
            (t1 >= other.t0 && t1 <= other.t1) || 
            (other.t0 >= t0 && other.t0 <= t1) ||
            (other.t1 >= t0 && other.t1 <= t1));
}

/*----------------------------------------------------------------------------
 * TimeSpan::isleft
 *----------------------------------------------------------------------------*/
bool AssetIndex::TimeSpan::isleft (const Span& other)
{
    return (t1 <= other.t1); // TODO: revist <= instead of <
}


/*----------------------------------------------------------------------------
 * TimeSpan::isright
 *----------------------------------------------------------------------------*/
bool AssetIndex::TimeSpan::isright (const Span& other)
{
    return (t1 >= other.t1);
}

/*----------------------------------------------------------------------------
 * TimeSpan::update
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::update (const Span& other)
{
    if(other.t0 < t0) t0 = other.t0;
    if(other.t1 > t1) t1 = other.t1;
}

/*----------------------------------------------------------------------------
 * TimeSpan::combine
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::combine (const Span& other1, const Span& other2)
{
    t0 = MIN(other1.t0, other2.t0);
    t1 = MAX(other1.t1, other2.t1);
}

/*----------------------------------------------------------------------------
 * TimeSpan::operator=
 *----------------------------------------------------------------------------*/
Span& AssetIndex::TimeSpan::operator= (const Span& other)
{
    t0 = other.t0;
    t1 = other.t1;
    return *this;
}

/*----------------------------------------------------------------------------
 * TimeSpan::display
 *----------------------------------------------------------------------------*/
void AssetIndex::TimeSpan::display (void)
{
    mlog(RAW, "[%.3lf, %.3lf]", t0, t1);
}


/*----------------------------------------------------------------------------
 * SpanTree::Constructor
 *----------------------------------------------------------------------------*/
AssetIndex::SpanTree::SpanTree (AssetIndex* _asset)
{
    asset = _asset;
    tree = NULL;
}

/*----------------------------------------------------------------------------
 * SpanTree::Destructor
 *----------------------------------------------------------------------------*/
AssetIndex::SpanTree::~SpanTree (void)
{
    // TODO: delete tree
}

/*----------------------------------------------------------------------------
 * SpanTree::update
 *----------------------------------------------------------------------------*/
bool AssetIndex::SpanTree::update (int ri)
{
    int maxdepth = 0;
    updatenode(ri, &tree, &maxdepth);
    balancenode(&tree);
    return true;
}

/*----------------------------------------------------------------------------
 * SpanTree::query
 *----------------------------------------------------------------------------*/
Ordering<int>* AssetIndex::SpanTree::query (span_t span, Dictionary<double>* attr)
{
    Ordering<int>* list = new Ordering<int>();
    querynode(span, attr, tree, list);
    return list;
}

/*----------------------------------------------------------------------------
 * SpanTree::display
 *----------------------------------------------------------------------------*/
void AssetIndex::SpanTree::display (void)
{
    displaynode(tree);
}

/*----------------------------------------------------------------------------
 * SpanTree::updatenode
 *----------------------------------------------------------------------------*/
void AssetIndex::SpanTree::updatenode (int ri, node_t** node, int* maxdepth)
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
        (*node)->left = NULL;
        (*node)->right = NULL;
        (*node)->depth = 0;
    }

    /* Pointer to Current Node */
    node_t* curr = *node;

    /* Update Tree Span */
    curr->span.update(span);

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
                updatenode(cri, &curr->left, maxdepth);
                curr->ril->next(&cri);
            }

            /* Push right in tree - from middle stop time */
            for(int i = middle_index; i < NODE_THRESHOLD; i++)
            {
                updatenode(cri, &curr->right, maxdepth);
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
        if(span.isLeft(curr->left->span))
        {   
            /* Update Left Tree */
            updatenode(ri, &curr->left, maxdepth);
        }
        else
        {   
            /* Update Right Tree */
            updatenode(ri, &curr->right, maxdepth);
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
 * SpanTree::balancenode
 *----------------------------------------------------------------------------*/
void AssetIndex::SpanTree::balancenode (node_t** root)
{
    node_t* curr = *root;

    /* Return on Leaf */
    if(!curr->left || !curr->right) return;

    /* Check if Tree Out of Balance */
    if(curr->left->depth + 1 < curr->right->depth)
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
        balancenode(&curr->right);

        /* Build Pointers */
        node_t* A = curr->left;
        node_t* B = curr;
        node_t* C = curr->right->left;
        node_t* D = curr->right;

        /* Rotate */
        B->right = C;
        D->left = B;

        /* Link In */
        *root = D;

        /* Update Span */
        D->span = B->span;
        B->span.combine(A->span, C->span);

        /* Update Depth */
        B->depth = MAX(A->depth, C->depth) + 1;
    }
    else if(curr->right->depth + 1 < curr->left->depth)
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
        balancenode(&curr->left);

        /* Build Pointers */
        node_t* B = curr->left;
        node_t* C = curr->left->right;
        node_t* D = curr;
        node_t* E = curr->right;

        /* Rotate */
        B->right = D;
        D->left = C;

        /* Link In */
        *root = B;

        /* Update Span */
        B->span = D->span;
        D->span.combine(C->span, E->span);

        /* Update Depth */
        D->depth = MAX(C->depth, E->depth) + 1;
    }
}

/*----------------------------------------------------------------------------
 * SpanTree::querynode
 *----------------------------------------------------------------------------*/
void AssetIndex::SpanTree::querynode (span_t span, Dictionary<double>* attr, node_t* curr, Ordering<int>* list)
{
    /* Return on Null Path */
    if(curr == NULL) return;

    /* Return if no Intersection with Tree */
    if(!span.intersect(curr->span)) return;

    /* If Leaf Node */
    if(curr->ril)
    {
        /* Populate with Current Node */
        for(int ri, t1 = curr->ril->first(&ri); t1 != (int)INVALID_KEY; t1 = curr->ril->next(&ri))
        {
            resource_t& resource = asset->resources[ri];
            if(span.intersect(resource.span))
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
        querynode(span, attr, curr->left, list);

        /* Goto Before Tree */
        querynode(span, attr, curr->right, list);
    }
}

/*----------------------------------------------------------------------------
 * SpanTree::displaynode
 *----------------------------------------------------------------------------*/
void AssetIndex::SpanTree::displaynode (node_t* curr)
{
    int ri;

    /* Stop */
    if(curr == NULL) return;

    /* Display */
    curr->span.display();
    mlog(RAW, " <%d>\n", curr->depth);
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
        mlog(RAW, "L");
        if(curr->left) curr->left->span.display();
        mlog(RAW, ", R");
        if(curr->right) curr->right->span.display();
    }
    mlog(RAW, "\n\n");
    
    /* Recurse */
    displaynode(curr->left);
    displaynode(curr->right);
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
        SpanTree::span_t            span;   // start, stop
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
