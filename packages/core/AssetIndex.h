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

#ifndef __asset_index__
#define __asset_index__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "LogLib.h"
#include "Dictionary.h"
#include "List.h"
#include "LuaObject.h"

/******************************************************************************
 * ASSET INDEX CLASS
 ******************************************************************************/

template <class T>
class AssetIndex: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DEFAULT_THRESHOLD = 8;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate (lua_State* L);

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*              LuaMetaName;
        static const struct luaL_Reg    LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct tsnode {
            List<int>*                  ril;        // resource index list (data = index), NULL if branch
            T                           span;       // for entire subtree rooted at this node
            struct tsnode*              left;       // left subtree
            struct tsnode*              right;      // right subtree
            int                         depth;      // depth of tree at this node
        } node_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        AssetIndex      (lua_State* L, Asset& _asset, int _threshold=DEFAULT_THRESHOLD);
        virtual         ~AssetIndex     (void);

        bool            add             (int i); // NOT thread safe
        List<int>*      query           (const T& span);
        void            display         (void);

        virtual void    display         (const T& span) = 0;
        virtual T       split           (const T& span) = 0;
        virtual bool    isleft          (const T& span1, const T& span2) = 0;
        virtual bool    isright         (const T& span1, const T& span2) = 0;
        virtual bool    intersect       (const T& span1, const T& span2) = 0;
        virtual T       combine         (const T& span1, const T& span2) = 0;
        virtual T       luatable2span   (lua_State* L, int parm) = 0;
        
        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Asset&          asset;
        List<T>         spans; // parallels asset resource list
        int32_t         threshold;
        node_t*         tree;

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void            updatenode      (int i, node_t** node, int* maxdepth);
        void            balancenode     (node_t** root);
        void            querynode       (const T& span, node_t* curr, List<int>* list);
        void            displaynode     (node_t* curr);

        static int      luaQuery        (lua_State* L);
        static int      luaDisplay      (lua_State* L);
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

template <class T>
const char* AssetIndex<T>::OBJECT_TYPE = "AssetIndex";

template <class T>
const char* AssetIndex<T>::LuaMetaName = "AssetIndex";

template <class T>
const struct luaL_Reg AssetIndex<T>::LuaMetaTable[] = {
    {"query",       luaQuery},
    {"display",     luaDisplay},
    {NULL,          NULL}
};

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
AssetIndex<T>::AssetIndex (lua_State* L, Asset& _asset, int _threshold):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    asset(_asset),
    threshold(_threshold)
{
    tree = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
AssetIndex<T>::~AssetIndex (void)
{
    bool pending_delete = asset.releaseLuaObject();
    if(pending_delete) delete &asset; // TODO: far from ideal... freeing memory from a reference

    // TODO: delete tree
}

/*----------------------------------------------------------------------------
 * add
 *----------------------------------------------------------------------------*/
template <class T>
bool AssetIndex<T>::add (int i)
{
    int maxdepth = 0;
    updatenode(i, &tree, &maxdepth);
    balancenode(&tree);
    return true;
}

/*----------------------------------------------------------------------------
 * query
 *----------------------------------------------------------------------------*/
template <class T>
List<int>* AssetIndex<T>::query (const T& span)
{
    List<int>* list = new List<int>();
    querynode(span, tree, list);
    return list;
}

/*----------------------------------------------------------------------------
 * display
 *----------------------------------------------------------------------------*/
template <class T>
void AssetIndex<T>::display (void)
{
    displaynode(tree);
}

/*----------------------------------------------------------------------------
 * updatenode
 *----------------------------------------------------------------------------*/
template <class T>
void AssetIndex<T>::updatenode (int i, node_t** node, int* maxdepth)
{
    /* Get Span of Resource being Added */
    T& span = spans[i];

    /* Create Node (if necessary */
    if(*node == NULL)
    {
        *node = new node_t;
        (*node)->ril = new List<int>;
        (*node)->span = span;
        (*node)->left = NULL;
        (*node)->right = NULL;
        (*node)->depth = 0;
    }

    /* Pointer to Current Node */
    node_t* curr = *node;

    /* Update Tree Span */
    curr->span = combine(curr->span, span);

    /* Update Current Leaf Node */
    if(curr->ril)
    {
        /* Add Index to Current Node */
        curr->ril->add(i);

        /* Split Current Leaf Node */
        int node_size = curr->ril->length();
        if(node_size >= threshold)
        {        
            /* Get Split Span (ss) of Node */
            T ss = split(curr->span);

            /* Check Split Produces Resources on Both Leaves */
            int lcnt = 0, rcnt = 0;
            for(int j = 0; j < node_size; j++)
            {
                int resource_index = curr->ril->get(j);
                T& resource_span = spans[resource_index];
                if(isleft(resource_span, ss))   lcnt++;
                else                            rcnt++;
            }

            /* Split Node Around Split Span */
            if(lcnt > 0 && rcnt > 0)
            {
                for(int j = 0; j < node_size; j++)
                {
                    int resource_index = curr->ril->get(j);
                    T& resource_span = spans[resource_index];
                    if(isleft(resource_span, ss))   updatenode(resource_index, &curr->left, maxdepth);
                    else                            updatenode(resource_index, &curr->right, maxdepth);
                }

                /* Make Current Node a Branch */
                delete curr->ril;
                curr->ril = NULL;
            }
        }
    }
    else 
    {
        /* Traverse Branch Node */
        if(isleft(span, curr->left->span))
        {   
            /* Update Left Tree */
            updatenode(i, &curr->left, maxdepth);
        }
        else
        {   
            /* Update Right Tree */
            updatenode(i, &curr->right, maxdepth);
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
 * balancenode
 *----------------------------------------------------------------------------*/
template <class T>
void AssetIndex<T>::balancenode (node_t** root)
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

        /* Update T */
        D->span = B->span;
        B->span = combine(A->span, C->span);

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

        /* Update T */
        B->span = D->span;
        D->span = combine(C->span, E->span);

        /* Update Depth */
        D->depth = MAX(C->depth, E->depth) + 1;
    }
}

/*----------------------------------------------------------------------------
 * querynode
 *----------------------------------------------------------------------------*/
template <class T>
void AssetIndex<T>::querynode (const T& span, node_t* curr, List<int>* list)
{
    /* Return on Null Path */
    if(curr == NULL) return;

    /* Return if no Intersection with Tree */
    if(!intersect(span, curr->span)) return;
    
    /* If Leaf Node */
    if(curr->ril)
    {
        /* Populate with Current Node */
        for(int i = 0; i < curr->ril->length(); i++)
        {
            int resource_index = curr->ril->get(i);
            if(intersect(span, spans[resource_index]))
            {
                list->add(resource_index);
            }
        }
    }
    else /* Branch Node */
    {
        /* Goto Before Tree */
        querynode(span, curr->left, list);

        /* Goto Before Tree */
        querynode(span, curr->right, list);
    }
}

/*----------------------------------------------------------------------------
 * displaynode
 *----------------------------------------------------------------------------*/
template <class T>
void AssetIndex<T>::displaynode (node_t* curr)
{
    /* Stop */
    if(curr == NULL) return;

    /* Display */
    display(curr->span);
    mlog(RAW, " <%d>\n", curr->depth);
    if(curr->ril)
    {
        for(int i = 0; i < curr->ril->length(); i++)
        {
            int resource_index = curr->ril->get(i);
            mlog(RAW, "%s ", asset[resource_index].name);
        }
    }
    else
    {
        mlog(RAW, "L");
        if(curr->left) display(curr->left->span);
        mlog(RAW, ", R");
        if(curr->right) display(curr->right->span);
    }
    mlog(RAW, "\n\n");
    
    /* Recurse */
    displaynode(curr->left);
    displaynode(curr->right);
}

/*----------------------------------------------------------------------------
 * luaQuery - :query(<attribute table>)
 *----------------------------------------------------------------------------*/
template <class T>
int AssetIndex<T>::luaQuery (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        AssetIndex* lua_obj = (AssetIndex*)getLuaSelf(L, 1);

        /* Create Query Attributes */
        T span = lua_obj->luatable2span(L, 2);

        /* Query Resources */
        List<int>* ril = lua_obj->query(span);

        /* Return Resources */
        lua_newtable(L);
        for(int r = 1, i = 0; i < ril->length(); i++, r++)
        {
            int resource_index = ril->get(i);
            lua_pushstring(L, lua_obj->asset[resource_index].name);
            lua_rawseti(L, -2, r);
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
template <class T>
int AssetIndex<T>::luaDisplay (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Parameters */
        AssetIndex* lua_obj     = (AssetIndex*)getLuaSelf(L, 1);

        /* Display Tree */
        lua_obj->display();

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

#endif  /* __asset_index__ */
