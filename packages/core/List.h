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

#ifndef __list__
#define __list__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <stdlib.h>
#include <assert.h>
#include <stdexcept>

/******************************************************************************
 * LIST TEMPLATE
 ******************************************************************************/

template <class T>
class List
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                List        (void);
        virtual ~List       (void);

        int     add         (T& data);
        bool    remove      (int index);
        T&      get         (int index);
        bool    set         (int index, T& data, bool with_delete=true);
        int     length      (void);
        void    clear       (void);

        T&      operator[]  (int index);

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int BLOCK_SIZE = 256;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct list_block_t {
            T                       data[BLOCK_SIZE];
            int                     offset;
            struct list_block_t*    next;
        } list_node_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        list_node_t     head;
        list_node_t*    tail;
        int             len;
        list_node_t*    prevnode;
        int             prevblock;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        list_node_t* newNode(void);
        virtual void freeNode (typename List<T>::list_node_t* node, int index);
};

/******************************************************************************
 * MANAGED LIST TEMPLATE
 ******************************************************************************/

template <class T>
class MgList: public List<T>
{
    public:
        MgList (bool _is_array=false);
        ~MgList (void);
    private:
        bool isArray;
        void freeNode (typename List<T>::list_node_t* node, int index);
};

/******************************************************************************
 * LIST METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
List<T>::List(void)
{
    head.offset = 0;
    head.next = NULL;
    tail = &head;
    len = 0;
    prevnode = &head;
    prevblock = 0;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
List<T>::~List(void)
{
    clear();
}

/*----------------------------------------------------------------------------
 * add
 *----------------------------------------------------------------------------*/
template <class T>
int List<T>::add(T& data)
{
    /* Check if Current Node is Full */
    if(tail->offset >= BLOCK_SIZE)
    {
        tail->next = newNode();
        tail = tail->next;
    }

    /* Add Element to Tail */
    tail->data[tail->offset] = data;
    tail->offset++;

    /* Increment Length and Return Index */
    int index = len++;
    return index;
}

/*----------------------------------------------------------------------------
 * remove
 *----------------------------------------------------------------------------*/
template <class T>
bool List<T>::remove(int index)
{
    if( (index < len) && (index >= 0) )
    {
        int node_block = index / BLOCK_SIZE;
        int node_offset = index % BLOCK_SIZE;
        list_node_t* curr = &head;
        list_node_t* prev = NULL;
        for(int i = 0; i < node_block; i++)
        {
            prev = curr;
            curr = curr->next;
        }

        /* Remove the Data */
        freeNode(curr, node_offset);

        /* Reset Previous Node Memory */
        prevnode = &head;
        prevblock = 0;

        /* Last Item In List */
        if(node_offset == (curr->offset - 1))
        {
            curr->offset--;
            if(curr->offset == 0)
            {
                /* Current Block Is Empty */
                if(prev) // check that current block isn't head
                {
                    delete prev->next;
                    prev->next = NULL;
                }
            }
        }
        else /* Middle Item In List */
        {
            /* Shift for Each Block */
            int start_offset = node_offset;
            int curr_offset = index;
            while(curr != NULL)
            {
                /* Shift Current Block */
                for(int i = start_offset; (i < BLOCK_SIZE - 1) && (curr_offset < len - 1); i++)
                {
                    curr->data[i] = curr->data[i + 1];
                    curr_offset++;
                }

                /* Shift Last Item */
                if(curr_offset < (len - 1) && curr->next != NULL)
                {
                    curr->data[BLOCK_SIZE - 1] = curr->next->data[0];
                    curr_offset++;
                    if(curr_offset >= (len - 1))
                    {
                        /* Next Block Is Empty */
                        delete curr->next;
                        curr->next = NULL;
                    }
                }
                else
                {
                    curr->offset--;
                }

                /* Goto Next Block */
                start_offset = 0;
                curr = curr->next;
            }
        }

        /* Update Length */
        len--;

        /* Recalculate the Tail */
        int tail_block = len / BLOCK_SIZE;
        tail = &head;
        for (int i = 0; i < tail_block; i++) { assert(tail); tail = tail->next; }

        /* Return Success */
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
template <class T>
T& List<T>::get(int index)
{
    if( (index < len) && (index >= 0) )
    {
        int node_block = index / BLOCK_SIZE;
        int node_offset = index % BLOCK_SIZE;

        list_node_t* curr = &head;
        if(node_block == prevblock)
        {
            curr = prevnode;
        }
        else if(node_block > prevblock)
        {
            curr = prevnode;
            for(int i = prevblock; i < node_block; i++) { assert(curr); curr = curr->next; }
            prevblock = node_block;
            prevnode = curr;
        }
        else
        {
            for(int i = 0; i < node_block; i++) { assert(curr); curr = curr->next; }
            prevblock = node_block;
            prevnode = curr;
        }

        assert(curr);
        return curr->data[node_offset];
    }
    else
    {
        throw std::out_of_range("List::get index out of range");
    }
}

/*----------------------------------------------------------------------------
 * set
 *
 *  with_delete which is defaulted to true, can be set to false for times when
 *  the list is reordered in place and the caller wants control over deallocation
 *----------------------------------------------------------------------------*/
template <class T>
bool List<T>::set(int index, T& data, bool with_delete)
{
    if( (index < len) && (index >= 0) )
    {
        int node_block = index / BLOCK_SIZE;
        int node_offset = index % BLOCK_SIZE;

        list_node_t* curr = &head;
        if(node_block == prevblock)
        {
            curr = prevnode;
        }
        else if(node_block > prevblock)
        {
            curr = prevnode;
            for(int i = prevblock; i < node_block; i++) curr = curr->next;
            prevblock = node_block;
            prevnode = curr;
        }
        else
        {
            for(int i = 0; i < node_block; i++) curr = curr->next;
            prevblock = node_block;
            prevnode = curr;
        }

        if(with_delete) freeNode(curr, node_offset);
        curr->data[node_offset] = data;

        return true;
    }
    else
    {
        return false;
    }
}

/*----------------------------------------------------------------------------
 * length
 *----------------------------------------------------------------------------*/
template <class T>
int List<T>::length(void)
{
    return len;
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
template <class T>
void List<T>::clear(void)
{
    /* Delete Head */
    for(int i = 0; i < head.offset; i++) freeNode(&head, i);

    /* Delete Rest of List */
    list_node_t* curr = head.next;
    while(curr != NULL)
    {
        for(int i = 0; i < curr->offset; i++) freeNode(curr, i);
        curr->offset = 0;
        list_node_t* prev = curr;
        curr = curr->next;
        delete prev;
    }

    /* Clean Up Parameters */
    head.offset = 0;
    head.next = NULL;
    tail = &head;
    len = 0;
}

/*----------------------------------------------------------------------------
 * []]
 *----------------------------------------------------------------------------*/
template <class T>
T& List<T>::operator[](int index)
{
    return get(index);
}

/*----------------------------------------------------------------------------
 * newNode
 *----------------------------------------------------------------------------*/
template <class T>
typename List<T>::list_node_t* List<T>::newNode(void)
{
    list_node_t* node = new list_node_t;
    node->next = NULL;
    node->offset = 0;
    return node;
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T>
void List<T>::freeNode(typename List<T>::list_node_t* node, int index)
{
    (void)node;
    (void)index;
}

/******************************************************************************
 * MANAGED LIST METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
MgList<T>::MgList(bool _is_array): List<T>()
{
    isArray = _is_array;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
MgList<T>::~MgList(void)
{
    List<T>::clear();
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T>
void MgList<T>::freeNode(typename List<T>::list_node_t* node, int index)
{
    if(!isArray)    delete node->data[index];
    else            delete [] node->data[index];
}

#endif  /* __list__ */
