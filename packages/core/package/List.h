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

#include "OsApi.h"

/******************************************************************************
 * LIST TEMPLATE
 ******************************************************************************/

template <class T>
class List
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DEFAULT_LIST_BLOCK_SIZE = 32;

    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct list_block_t {
            T*                      data;
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
                explicit            Iterator    (const List& l);
                                    ~Iterator   (void);
                const T&            operator[]  (int index) const;
                const int           length;
                const int           blockSize;
            private:
                const list_node_t** blocks;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

      explicit  List        (int list_block_size=DEFAULT_LIST_BLOCK_SIZE);
                List        (const List& l1);
                ~List       (void);

        int     add         (const T& data);
        bool    remove      (int index);
        T&      get         (int index);
        bool    set         (int index, const T& data, bool with_delete=true);
        int     length      (void) const;
        bool    empty       (void) const;
        void    clear       (void);
        void    sort        (void);
        T*      array       (void);

        T       operator[]  (int index) const;
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
        int             listBlockSize;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void            initialize          (void);
        void            copy                (const List& l1);
        list_node_t*    newNode             (void);
        void            freeNode            (typename List<T>::list_node_t* node, int index);
        void            quicksort           (T* array, int start, int end); // NOLINT(misc-no-recursion)
        int             quicksortpartition  (T* array, int start, int end);
};

/******************************************************************************
 * ITERATOR METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
List<T>::Iterator::Iterator(const List& l):
    length(l.len),
    blockSize(l.listBlockSize)
{
    const int num_blocks = (length + (blockSize - 1)) / blockSize;
    blocks = new const List<T>::list_node_t* [num_blocks];

    const List<T>::list_node_t* curr_block = &l.head;
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
template <class T>
List<T>::Iterator::~Iterator(void)
{
    delete [] blocks;
}

/*----------------------------------------------------------------------------
 * []
 *----------------------------------------------------------------------------*/
template <class T>
const T& List<T>::Iterator::operator[](int index) const
{
    if( (index < length) && (index >= 0) )
    {
        const int node_block = index / blockSize;
        const int node_offset = index % blockSize;
        const List<T>::list_node_t* block = blocks[node_block]; // NOLINT(clang-analyzer-core.uninitialized.Assign)
        return block->data[node_offset];
    }

    throw RunTimeException(CRITICAL, RTE_FAILURE, "List::Iterator index out of range");
}


/******************************************************************************
 * LIST METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
List<T>::List(int list_block_size):
    listBlockSize(list_block_size)
{
    assert(listBlockSize >= 0);
    listBlockSize = listBlockSize == 0 ? DEFAULT_LIST_BLOCK_SIZE : listBlockSize;
    head.data = new T [listBlockSize];
    initialize();
}

/*----------------------------------------------------------------------------
 * Copy Constructor
 *----------------------------------------------------------------------------*/
template <class T>
List<T>::List(const List<T>& l1)
{
    listBlockSize = l1.listBlockSize;
    head.data = new T [listBlockSize];
    initialize();
    copy(l1);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
List<T>::~List(void)
{
    clear();
    delete [] head.data;
}

/*----------------------------------------------------------------------------
 * add
 *----------------------------------------------------------------------------*/
template <class T>
int List<T>::add(const T& data)
{
    /* Check if Current Node is Full */
    if(tail->offset >= listBlockSize)
    {
        tail->next = newNode();
        tail = tail->next;
    }

    /* Add Element to Tail */
    tail->data[tail->offset] = data;
    tail->offset++;

    /* Increment Length and Return Index */
    const int index = len++;
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
        const int node_block = index / listBlockSize;
        const int node_offset = index % listBlockSize;
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
                    delete [] prev->next->data;
                    delete prev->next;
                    prev->next = NULL;
                }
            }
        }
        else /* Middle Item In List */
        {
            /* Shift for Each Block */
            int start_offset = node_offset;
            int curr_index = index;
            while(curr != NULL)
            {
                /* Shift Current Block */
                for(int i = start_offset; (i < listBlockSize - 1) && (curr_index < len - 1); i++)
                {
                    curr->data[i] = curr->data[i + 1];
                    curr_index++;
                }

                /* Shift Last Item */
                if(curr_index < (len - 1) && curr->next != NULL)
                {
                    curr->data[listBlockSize - 1] = curr->next->data[0];
                    curr_index++;
                    if(curr_index >= (len - 1))
                    {
                        /* Next Block Is Empty */
                        delete [] curr->next->data;
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
        const int tail_block = len / listBlockSize;
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
        const int node_block = index / listBlockSize;
        const int node_offset = index % listBlockSize;

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

    throw RunTimeException(CRITICAL, RTE_FAILURE, "List::get index out of range");
}

/*----------------------------------------------------------------------------
 * set
 *
 *  with_delete which is defaulted to true, can be set to false for times when
 *  the list is reordered in place and the caller wants control over deallocation
 *----------------------------------------------------------------------------*/
template <class T>
bool List<T>::set(int index, const T& data, bool with_delete)
{
    if( (index < len) && (index >= 0) )
    {
        int node_block = index / listBlockSize;
        int node_offset = index % listBlockSize;

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

    return false;
}

/*----------------------------------------------------------------------------
 * length
 *----------------------------------------------------------------------------*/
template <class T>
int List<T>::length(void) const
{
    return len;
}

/*----------------------------------------------------------------------------
 * empty
 *----------------------------------------------------------------------------*/
template <class T>
bool List<T>::empty(void) const
{
    return (len == 0);
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
        delete [] prev->data;
        delete prev;
    }

    /* Reset State of List */
    initialize();
}

/*----------------------------------------------------------------------------
 * sort
 *----------------------------------------------------------------------------*/
template <class T>
void List<T>::sort(void)
{
    /* Allocate Array */
    T* _array = new T[len];

    /* Build Array */
    list_node_t* curr = &head;
    int items_remaining = len;
    int index = 0;
    while(items_remaining > 0)
    {
        const int items_to_copy = MIN(items_remaining, listBlockSize);
        for(int i = 0; i < items_to_copy; i++)
        {
            _array[index++] = curr->data[i];
        }
        items_remaining -= items_to_copy;
        curr = curr->next;
    }

    /* Sort Array */
    quicksort(_array, 0, len - 1);

    /* Write Array */
    curr = &head;
    items_remaining = len;
    index = 0;
    while(items_remaining > 0)
    {
        const int items_to_copy = MIN(items_remaining, listBlockSize);
        for(int i = 0; i < items_to_copy; i++)
        {
            curr->data[i] = _array[index++];
        }
        items_remaining -= items_to_copy;
        curr = curr->next;
    }

    /* Deallocate Array */
    delete [] _array;
}

/*----------------------------------------------------------------------------
 * array
 *----------------------------------------------------------------------------*/
template <class T>
T* List<T>::array(void)
{
    /* Allocate Array */
    T* data = new T[len];

    /* Build Array */
    list_node_t* curr = &head;
    int items_remaining = len;
    int index = 0;
    while(items_remaining > 0)
    {
        const int items_to_copy = MIN(items_remaining, listBlockSize);
        for(int i = 0; i < items_to_copy; i++)
        {
            data[index++] = curr->data[i];
        }
        items_remaining -= items_to_copy;
        curr = curr->next;
    }

    /* Return Array */
    return data;
  }

/*----------------------------------------------------------------------------
 * [] - rvalue
 *----------------------------------------------------------------------------*/
template <class T>
T List<T>::operator[](int index) const
{
    if( (index < len) && (index >= 0) )
    {
        const int node_block = index / listBlockSize;
        const int node_offset = index % listBlockSize;

        const list_node_t* curr = &head;
        for(int i = 0; i < node_block; i++)
        {
            assert(curr);
            curr = curr->next;
        }

        assert(curr);
        return curr->data[node_offset];
    }

    throw RunTimeException(CRITICAL, RTE_FAILURE, "List::get index out of range");
}

/*----------------------------------------------------------------------------
 * [] - lvalue
 *----------------------------------------------------------------------------*/
template <class T>
T& List<T>::operator[](int index)
{
    return get(index);
}

/*----------------------------------------------------------------------------
 * =
 *----------------------------------------------------------------------------*/
template <class T>
List<T>& List<T>::operator= (const List<T>& l1)
{
    if(this == &l1) return *this;
    listBlockSize = l1.listBlockSize;
    clear();
    copy(l1);
    return *this;
}

/*----------------------------------------------------------------------------
 * initialize
 *----------------------------------------------------------------------------*/
template <class T>
void List<T>::initialize(void)
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
template <class T>
void List<T>::copy(const List<T>& l1)
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
template <class T>
typename List<T>::list_node_t* List<T>::newNode(void)
{
    list_node_t* node = new list_node_t;
    node->data = new T [listBlockSize];
    node->next = NULL;
    node->offset = 0;
    return node;
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T>
static void listDeleteIfPointer(const T& t) { (void)t; }

template <class T>
static void listDeleteIfPointer(T* t) { delete t; }

template <class T>
void List<T>::freeNode(typename List<T>::list_node_t* node, int index)
{
    listDeleteIfPointer(node->data[index]);
}

/*----------------------------------------------------------------------------
 * quicksort
 *----------------------------------------------------------------------------*/
template <class T>
void List<T>::quicksort(T* array, int start, int end) // NOLINT(misc-no-recursion)
{
    if(start < end)
    {
        const int partition = quicksortpartition(array, start, end);
        quicksort(array, start, partition); // NOLINT(misc-no-recursion)
        quicksort(array, partition + 1, end); // NOLINT(misc-no-recursion)
    }
}

/*----------------------------------------------------------------------------
 * quicksortpartition
 *----------------------------------------------------------------------------*/
template <class T>
int List<T>::quicksortpartition(T* array, int start, int end)
{
    const double pivot = array[(start + end) / 2];

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

#endif  /* __list__ */
