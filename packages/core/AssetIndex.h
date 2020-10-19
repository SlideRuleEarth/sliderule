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
#include "LuaEngine.h"

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
         * Types
         *--------------------------------------------------------------------*/

        typedef struct tsnode {
            List<int>*      ril;        // resource index list (data = index), NULL if branch
            T               span;       // for entire subtree rooted at this node
            struct tsnode*  left;       // left subtree
            struct tsnode*  right;      // right subtree
            int             depth;      // depth of tree at this node
        } node_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            AssetIndex      (lua_State* L, Asset& _asset, const char* meta_name, const struct luaL_Reg meta_table[], int _threshold=DEFAULT_THRESHOLD);
        virtual             ~AssetIndex     (void);

        virtual void        build           (void);
        virtual T           get             (int index);
        virtual bool        add             (const T& span); // NOT thread safe
        virtual List<int>*  query           (const T& span);
        virtual void        display         (void);

        virtual void        split           (node_t* node, T& lspan, T& rspan) = 0;
        virtual bool        isleft          (node_t* node, const T& span) = 0;
        virtual bool        isright         (node_t* node, const T& span) = 0;
        virtual bool        intersect       (const T& span1, const T& span2) = 0;
        virtual T           combine         (const T& span1, const T& span2) = 0;
        virtual T           attr2span       (Dictionary<double>* attr, bool* provided=NULL) = 0;
        virtual T           luatable2span   (lua_State* L, int parm) = 0;
        virtual void        displayspan     (const T& span) = 0;
        
    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void            buildtree       (node_t* root, int* maxdepth);
        void            updatenode      (int i, node_t** node, int* maxdepth);
        void            balancenode     (node_t** root);
        void            querynode       (const T& span, node_t* curr, List<int>* list);
        node_t*         newnode         (const T& span);
        void            deletenode      (node_t* node);
        bool            prunenode       (node_t* node);
        void            displaynode     (node_t* curr);

        static int      luaAdd          (lua_State* L);
        static int      luaQuery        (lua_State* L);
        static int      luaDisplay      (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Asset&          asset;
        List<T>         spans; // parallels asset resource list
        int32_t         threshold;
        node_t*         tree;
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

template <class T>
const char* AssetIndex<T>::OBJECT_TYPE = "AssetIndex";

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
AssetIndex<T>::AssetIndex (lua_State* L, Asset& _asset, const char* meta_name, const struct luaL_Reg meta_table[], int _threshold):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table),
    asset(_asset),
    threshold(_threshold)
{
    tree = NULL;

    LuaEngine::setAttrFunc(L, "add", luaAdd);
    LuaEngine::setAttrFunc(L, "query", luaQuery);
    LuaEngine::setAttrFunc(L, "display", luaDisplay);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
AssetIndex<T>::~AssetIndex (void)
{
    asset.releaseLuaObject();
    deletenode(tree);
}

/*----------------------------------------------------------------------------
 * build
 *----------------------------------------------------------------------------*/
template <class T>
void AssetIndex<T>::build (void)
{
    /* Build Tree Node */
    spans.clear();
    for(int i = 0; i < asset.size(); i++)
    {
        bool provided = false;
        T span = attr2span(&asset[i].attributes, &provided);            
        int index = spans.add(span);

        /* Create Tree Node */
        if(tree == NULL) tree = newnode(span);

        /* Update Tree Span */
        tree->span = combine(tree->span, span);

        /* Update Tree Resource List */
        tree->ril->add(index);
    }

    /* Build Tree Structure */
    int maxdepth = 0;
    buildtree(tree, &maxdepth);
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
template <class T>
T AssetIndex<T>::get (int index)
{
    return spans[index];
}

/*----------------------------------------------------------------------------
 * add - MUST BE COORDINATED w/ ASSET
 * 
 *  Note:   the call to add the span to the spans list relies upon the
 *          assumption that the index the span sits at within the list
 *          matches the index the originating resource sits at within the
 *          asset's resource list
 *----------------------------------------------------------------------------*/
template <class T>
bool AssetIndex<T>::add (const T& span)
{
    int maxdepth = 0;
    int index = spans.add(span);
    updatenode(index, &tree, &maxdepth);
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
 * buildtree
 *----------------------------------------------------------------------------*/
template <class T>
void AssetIndex<T>::buildtree (node_t* root, int* maxdepth)
{
    /* Check Root */
    if(!root || !root->ril) return;
    
    /* Split Current Leaf Node */
    int root_size = root->ril->length();
    if(root_size >= threshold)
    {
        /* Split Node */
        T lspan, rspan;
        split(root, lspan, rspan);
        node_t* lnode = newnode(lspan);
        node_t* rnode = newnode(rspan);
        
        /* Split Resources on Both Leaves */
        int lcnt = 0, rcnt = 0;
        for(int j = 0; j < root_size; j++)
        {
            int resource_index = root->ril->get(j);
            T& resource_span = spans[resource_index];
            
            if(intersect(lspan, resource_span))
            {
                lcnt++;
                lnode->span = combine(lnode->span, resource_span);
                lnode->ril->add(resource_index);
            }
            
            if(intersect(rspan, resource_span))
            {
                rcnt++;
                rnode->span = combine(rnode->span, resource_span);
                rnode->ril->add(resource_index);
            }
        }

        /* Check if Makes Sense to Split */
        if(lcnt > 0 && rcnt > 0 && lcnt != root_size && rcnt != root_size)
        {
            /* Split Root */            
            delete root->ril;
            root->ril = NULL;
            root->left = lnode;
            root->right = rnode;

            /* Recurse Into Each Leaf */
            buildtree(root->left, maxdepth);
            buildtree(root->right, maxdepth);

            /* Update Max Depth */
            (*maxdepth)++;
        }
        else
        {
            /* Clean Up (unused) Leaf Nodes */
            delete lnode->ril;
            delete lnode;
            delete rnode->ril;
            delete rnode;
        }
    }

    /* Update Current Depth */
    if(root->depth < *maxdepth)
    {
        root->depth = *maxdepth;
    }
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
    if(*node == NULL) *node = newnode(span);

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
            /* Split Node */
            T lspan, rspan;
            split(curr, lspan, rspan);
            
            /* Preview Split Resources on Both Leaves */
            int lcnt = 0, rcnt = 0;
            for(int j = 0; j < node_size; j++)
            {
                int resource_index = curr->ril->get(j);
                T& resource_span = spans[resource_index];
                if(intersect(lspan, resource_span)) lcnt++;
                if(intersect(rspan, resource_span)) rcnt++;
            }

            /* Check if Makes Sense to Split */
            if(lcnt > 0 && rcnt > 0 && lcnt != node_size && rcnt != node_size)
            {
                /* Split Node */
                for(int j = 0; j < node_size; j++)
                {
                    int resource_index = curr->ril->get(j);
                    T& resource_span = spans[resource_index];
                    if(intersect(lspan, resource_span)) updatenode(resource_index, &curr->left, maxdepth);
                    if(intersect(rspan, resource_span)) updatenode(resource_index, &curr->right, maxdepth);
                }

                /* Make Current Node a Branch */
                delete curr->ril;
                curr->ril = NULL;

                /* Update Max Depth */
                (*maxdepth)++;
            }
        }
    }
    else 
    {
        /* Update Branches */
        if(isleft(curr, span))  updatenode(i, &curr->left, maxdepth);
        if(isright(curr, span)) updatenode(i, &curr->right, maxdepth);

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

        /* Update Spans */
        D->span = B->span;
        B->span = combine(A->span, C->span);

        /* Check For Pruning */
        if(!prunenode(B))
        {
            /* Update Depth */
            B->depth = MAX(A->depth, C->depth) + 1;
        }
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

        /* Update Spans */
        B->span = D->span;
        D->span = combine(C->span, E->span);

        /* Check For Pruning */
        if(!prunenode(D))
        {
            /* Update Depth */
            D->depth = MAX(C->depth, E->depth) + 1;
        }
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
typename AssetIndex<T>::node_t* AssetIndex<T>::newnode (const T& span)
{
    node_t* node = new node_t;
    node->ril = new List<int>;
    node->span = span;
    node->left = NULL;
    node->right = NULL;
    node->depth = 0;
    return node;
}

/*----------------------------------------------------------------------------
 * displaynode
 *----------------------------------------------------------------------------*/
template <class T>
void AssetIndex<T>::deletenode (node_t* node)
{
    /* Stop */
    if(node == NULL) return;

    /* Save Off Branches */
    node_t* left = node->left;
    node_t* right = node->right;
    
    /* Delete Node */
    if(node->ril) delete node->ril;
    delete node;

    /* Recurse */
    deletenode(left);
    deletenode(right);
}


/*----------------------------------------------------------------------------
 * displaynode
 *----------------------------------------------------------------------------*/
template <class T>
bool AssetIndex<T>::prunenode (node_t* node)
{
    bool pruned = false;

    node_t* left = node->left;
    node_t* right = node->right;

    /* Check For Pruning */
    if(left->ril && right->ril)
    {
        /* Count Overlap */
        int matches = 0;
        for(int i = 0; i < left->ril->length(); i++)
        {
            int ri = left->ril->get(i);
            for(int j = 0; j < right->ril->length(); j++)
            {
                int rj = right->ril->get(j);
                if (ri == rj)
                {
                    matches++;
                    break;
                }
            }
        }

        /* Check if Overlap Warrants Pruning */
        if(matches == left->ril->length() || matches == right->ril->length())
        {
            /* Determine Which Leaf Contains All Indexes */
            node_t* prune;
            if(left->ril->length() >= right->ril->length())
            {
                prune = left;
            }
            else
            {
                prune = right;
            }

            /* Copy Indexes Into Branch Node */
            node->ril = new List<int>;
            for(int i = 0; i < prune->ril->length(); i++)
            {
                int ri = prune->ril->get(i);
                node->ril->add(ri);
            }

            /* Make Branch Node a Leaf Node */
            delete left->ril;
            delete right->ril;
            node->left = NULL;
            node->right = NULL;
            node->depth = 0;

            /* Successfully Pruned */
            pruned = true;
        }
    }

    /* Return Pruned Status */
    return pruned;
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
    displayspan(curr->span);
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
        if(curr->left) displayspan(curr->left->span);
        mlog(RAW, ", R");
        if(curr->right) displayspan(curr->right->span);
    }
    mlog(RAW, "\n\n");
    
    /* Recurse */
    displaynode(curr->left);
    displaynode(curr->right);
}

/*----------------------------------------------------------------------------
 * luaAdd - :add(<attributes>)
 *----------------------------------------------------------------------------*/
template <class T>
int AssetIndex<T>::luaAdd (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        AssetIndex<T>* lua_obj = (AssetIndex<T>*)getLuaSelf(L, 1);

        /* Create Resource Attributes */
        T span = lua_obj->luatable2span(L, 2);

        /* Add Resource */
        status = lua_obj->add(span);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error adding: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, 2);
}

/*----------------------------------------------------------------------------
 * luaQuery - :query(<resource table>)
 *----------------------------------------------------------------------------*/
template <class T>
int AssetIndex<T>::luaQuery (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        AssetIndex<T>* lua_obj = (AssetIndex<T>*)getLuaSelf(L, 1);

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
 * luaDisplay - :display()
 *----------------------------------------------------------------------------*/
template <class T>
int AssetIndex<T>::luaDisplay (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Parameters */
        AssetIndex<T>* lua_obj = (AssetIndex<T>*)getLuaSelf(L, 1);

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
