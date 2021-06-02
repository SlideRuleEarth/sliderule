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

#ifndef __list__
#define __list__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "RTExcept.h"
#include <stdlib.h>
#include <assert.h>

/******************************************************************************
 * LIST TEMPLATE
 ******************************************************************************/

template <class T, int LIST_BLOCK_SIZE=256>
class List
{
    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct list_block_t {
            T                       data[LIST_BLOCK_SIZE];
            int                     offset;
            struct list_block_t*    next;
        } list_node_t;

    public:

        /*--------------------------------------------------------------------
         * Iterator Subclass
         *--------------------------------------------------------------------*/

        class Iterator
        {
            public:
                                    Iterator    (const List& l);
                                    ~Iterator   (void);
                const T&            operator[]  (int index) const;
                const int           length;
            private:
                const list_node_t** blocks;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                List        (void);
                List        (const List& l1);
        virtual ~List       (void);

        int     add         (const T& data);
        bool    remove      (int index);
        T&      get         (int index);
        bool    set         (int index, T& data, bool with_delete=true);
        int     length      (void) const;
        void    clear       (void);
        void    sort        (void);

        T&      operator[]  (int index);
        List&   operator=   (const List& l1);

    protected:

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

        void            initialize          (void);
        void            copy                (const List& l1);
        list_node_t*    newNode             (void);
        virtual void    freeNode            (typename List<T, LIST_BLOCK_SIZE>::list_node_t* node, int index);
        void            quicksort           (T* array, int start, int end);
        int             quicksortpartition  (T* array, int start, int end);
};

/******************************************************************************
 * MANAGED LIST TEMPLATE
 ******************************************************************************/

template <class T, int LIST_BLOCK_SIZE=256, bool is_array=false>
class MgList: public List<T, LIST_BLOCK_SIZE>
{
    public:
        MgList (void);
        ~MgList (void);
    private:
        void freeNode (typename List<T, LIST_BLOCK_SIZE>::list_node_t* node, int index);
};

/******************************************************************************
 * ITERATOR METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
List<T, LIST_BLOCK_SIZE>::Iterator::Iterator(const List& l):
    length(l.len)
{
    int num_blocks = (length + (LIST_BLOCK_SIZE - 1)) / LIST_BLOCK_SIZE;
    blocks = new const List<T, LIST_BLOCK_SIZE>::list_node_t* [num_blocks];

    const List<T, LIST_BLOCK_SIZE>::list_node_t* curr_block = &l.head;
    for(int b = 0; b < num_blocks; b++)
    {
        assert(curr_block);
        blocks[b] = curr_block;
        curr_block = curr_block->next;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
List<T, LIST_BLOCK_SIZE>::Iterator::~Iterator(void)
{
    delete [] blocks;
}

/*----------------------------------------------------------------------------
 * []
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
const T& List<T, LIST_BLOCK_SIZE>::Iterator::operator[](int index) const
{
    if( (index < length) && (index >= 0) )
    {
        int node_block = index / LIST_BLOCK_SIZE;
        int node_offset = index % LIST_BLOCK_SIZE;
        const List<T, LIST_BLOCK_SIZE>::list_node_t* block = blocks[node_block];
        return block->data[node_offset];
    }
    else
    {
        throw RunTimeException(CRITICAL, "List::Iterator index out of range");
    }
}


/******************************************************************************
 * LIST METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
List<T, LIST_BLOCK_SIZE>::List(void)
{
    initialize();
}

/*----------------------------------------------------------------------------
 * Copy Constructor
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
List<T, LIST_BLOCK_SIZE>::List(const List<T, LIST_BLOCK_SIZE>& l1)
{
    initialize();
    copy(l1);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
List<T, LIST_BLOCK_SIZE>::~List(void)
{
    clear();
}

/*----------------------------------------------------------------------------
 * add
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
int List<T, LIST_BLOCK_SIZE>::add(const T& data)
{
    /* Check if Current Node is Full */
    if(tail->offset >= LIST_BLOCK_SIZE)
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
template <class T, int LIST_BLOCK_SIZE>
bool List<T, LIST_BLOCK_SIZE>::remove(int index)
{
    if( (index < len) && (index >= 0) )
    {
        int node_block = index / LIST_BLOCK_SIZE;
        int node_offset = index % LIST_BLOCK_SIZE;
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
                for(int i = start_offset; (i < LIST_BLOCK_SIZE - 1) && (curr_offset < len - 1); i++)
                {
                    curr->data[i] = curr->data[i + 1];
                    curr_offset++;
                }

                /* Shift Last Item */
                if(curr_offset < (len - 1) && curr->next != NULL)
                {
                    curr->data[LIST_BLOCK_SIZE - 1] = curr->next->data[0];
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
        int tail_block = len / LIST_BLOCK_SIZE;
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
template <class T, int LIST_BLOCK_SIZE>
T& List<T, LIST_BLOCK_SIZE>::get(int index)
{
    if( (index < len) && (index >= 0) )
    {
        int node_block = index / LIST_BLOCK_SIZE;
        int node_offset = index % LIST_BLOCK_SIZE;

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
        throw RunTimeException(CRITICAL, "List::get index out of range");
    }
}

/*----------------------------------------------------------------------------
 * set
 *
 *  with_delete which is defaulted to true, can be set to false for times when
 *  the list is reordered in place and the caller wants control over deallocation
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
bool List<T, LIST_BLOCK_SIZE>::set(int index, T& data, bool with_delete)
{
    if( (index < len) && (index >= 0) )
    {
        int node_block = index / LIST_BLOCK_SIZE;
        int node_offset = index % LIST_BLOCK_SIZE;

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
template <class T, int LIST_BLOCK_SIZE>
int List<T, LIST_BLOCK_SIZE>::length(void) const
{
    return len;
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
void List<T, LIST_BLOCK_SIZE>::clear(void)
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
 * sort
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
void List<T, LIST_BLOCK_SIZE>::sort(void)
{
    /* Allocate Array */
    T* array = new T[len];

    /* Build Array */
    for(int i = 0; i < len; i++)
    {
        array[i] = get(i);
    }

    /* Sort Array */
    quicksort(array, 0, len - 1);

    /* Write Array */
    for(int i = 0; i < len; i++)
    {
        set(i, array[i], false);
    }

    /* Deallocate Array */
    delete [] array;
}

/*----------------------------------------------------------------------------
 * []
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
T& List<T, LIST_BLOCK_SIZE>::operator[](int index)
{
    return get(index);
}

/*----------------------------------------------------------------------------
 * =
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
List<T, LIST_BLOCK_SIZE>& List<T, LIST_BLOCK_SIZE>::operator= (const List<T, LIST_BLOCK_SIZE>& l1)
{
    clear();
    copy(l1);
    return *this;
}

/*----------------------------------------------------------------------------
 * initialize
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
void List<T, LIST_BLOCK_SIZE>::initialize(void)
{
    head.offset = 0;
    head.next = NULL;
    tail = &head;
    len = 0;
    prevnode = &head;
    prevblock = 0;
}

/*----------------------------------------------------------------------------
 * =
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
void List<T, LIST_BLOCK_SIZE>::copy(const List<T, LIST_BLOCK_SIZE>& l1)
{
    const list_node_t* curr = &l1.head;
    while(curr)
    {
        for(int i = 0; i < curr->offset; i++)
        {
            add(curr->data[i]);
        }
        curr = curr->next;
    }
}

/*----------------------------------------------------------------------------
 * newNode
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
typename List<T, LIST_BLOCK_SIZE>::list_node_t* List<T, LIST_BLOCK_SIZE>::newNode(void)
{
    list_node_t* node = new list_node_t;
    node->next = NULL;
    node->offset = 0;
    return node;
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
void List<T, LIST_BLOCK_SIZE>::freeNode(typename List<T, LIST_BLOCK_SIZE>::list_node_t* node, int index)
{
    (void)node;
    (void)index;
}

/*----------------------------------------------------------------------------
 * quicksort
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
void List<T, LIST_BLOCK_SIZE>::quicksort(T* array, int start, int end)
{
    if(start < end)
    {
        int partition = quicksortpartition(array, start, end);
        quicksort(array, start, partition);
        quicksort(array, partition + 1, end);
    }
}

/*----------------------------------------------------------------------------
 * quicksortpartition
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE>
int List<T, LIST_BLOCK_SIZE>::quicksortpartition(T* array, int start, int end)
{
    double pivot = array[(start + end) / 2];

    start--;
    end++;
    while(true)
    {
        while (array[++start] < pivot);
        while (array[--end] > pivot);
        if (start >= end) return end;

        T tmp = array[start];
        array[start] = array[end];
        array[end] = tmp;
    }
}

/******************************************************************************
 * MANAGED LIST METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE, bool is_array>
MgList<T, LIST_BLOCK_SIZE, is_array>::MgList(void): List<T, LIST_BLOCK_SIZE>()
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE, bool is_array>
MgList<T, LIST_BLOCK_SIZE, is_array>::~MgList(void)
{
    List<T, LIST_BLOCK_SIZE>::clear();
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T, int LIST_BLOCK_SIZE, bool is_array>
void MgList<T, LIST_BLOCK_SIZE, is_array>::freeNode(typename List<T, LIST_BLOCK_SIZE>::list_node_t* node, int index)
{
    if(!is_array)   delete node->data[index];
    else            delete [] node->data[index];
}

#endif  /* __list__ */
