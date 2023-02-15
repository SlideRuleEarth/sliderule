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

#ifndef __ordering__
#define __ordering__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <assert.h>
#include "OsApi.h"

/******************************************************************************
 * ORDERING TEMPLATE
 ******************************************************************************/
/*
 * Ordering - sorted linked list of data type T and index type K
 */
template <class T, typename K=unsigned long>
class Ordering
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            EXACT_MATCH,
            GREATER_THAN_OR_EQUAL,
            LESS_THAN_OR_EQUAL,
            GREATER_THAN,
            LESS_THAN
        } searchMode_t;

        typedef int (*postFunc_t) (void* data, int size, void* parm);

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const long INFINITE_LIST_SIZE = -1;

        /*--------------------------------------------------------------------
         * Iterator Subclass
         *--------------------------------------------------------------------*/

        typedef struct kv {
            kv(K _key, const T& _value): key(_key), value(_value) {};
            ~kv(void) {};
            K           key;
            const T&    value;
        } kv_t;

        class Iterator
        {
            public:
                                    Iterator    (const Ordering& o);
                                    ~Iterator   (void);
                kv_t                operator[]  (int index) const;
                const int           length;
            private:
                const T**           values;
                K*                  keys;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    Ordering    (typename Ordering<T,K>::postFunc_t post_func=NULL, void* post_parm=NULL, K max_list_size=INFINITE_LIST_SIZE);
        virtual     ~Ordering   (void);

        bool        add         (K key, const T& data, bool unique=false);
        T&          get         (K key, searchMode_t smode=EXACT_MATCH);
        bool        remove      (K key, searchMode_t smode=EXACT_MATCH);
        long        length      (void);
        void        flush       (void);
        void        clear       (void);

        K           first       (T* data);
        K           next        (T* data);
        K           last        (T* data);
        K           prev        (T* data);

        Ordering&   operator=   (const Ordering& other);
        T&          operator[]  (K key);

    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct sorted_block_t {
            K                       key;
            T                       data;
            struct sorted_block_t*  next;
            struct sorted_block_t*  prev;
        } sorted_node_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        sorted_node_t*  firstNode;
        sorted_node_t*  lastNode;
        sorted_node_t*  curr;
        long            len;
        long            maxListSize;
        postFunc_t      postFunc;
        void*           postParm;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool            setMaxListSize  (long _max_list_size);
        bool            addNode         (K key, const T& data, bool unique);
        void            postNode        (sorted_node_t* node);
        virtual void    freeNode        (sorted_node_t* node);
};

/******************************************************************************
 * MANAGED ORDERING TEMPLATE
 ******************************************************************************/

template <class T, typename K=unsigned long, bool is_array=false>
class MgOrdering: public Ordering<T,K>
{
    public:
        MgOrdering (typename Ordering<T,K>::postFunc_t post_func=NULL, void* post_parm=NULL, K max_list_size=Ordering<T,K>::INFINITE_LIST_SIZE);
        ~MgOrdering (void);
    private:
        void freeNode (typename Ordering<T,K>::sorted_node_t* node);
};

/******************************************************************************
 * ITERATOR METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, typename K>
Ordering<T,K>::Iterator::Iterator(const Ordering& o):
    length(o.len)
{
    values = new const T* [length];
    keys = new K [length];

    int j = 0;
    sorted_node_t* node_ptr = o.firstNode;
    while(node_ptr != NULL)
    {
        values[j] = &node_ptr->data;
        keys[j] = node_ptr->key;
        j++;
        node_ptr = node_ptr->next;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T, typename K>
Ordering<T,K>::Iterator::~Iterator(void)
{
    delete [] values;
    delete [] keys;
}

/*----------------------------------------------------------------------------
 * []
 *----------------------------------------------------------------------------*/
template <class T, typename K>
typename Ordering<T,K>::kv_t Ordering<T,K>::Iterator::operator[](int index) const
{
    if( (index < length) && (index >= 0) )
    {
        Ordering<T,K>::kv_t pair(keys[index], *values[index]);
        return pair;
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Ordering::Iterator index out of range");
    }
}

/******************************************************************************
 ORDERING METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, typename K>
Ordering<T,K>::Ordering(postFunc_t post_func, void* post_parm, K max_list_size)
{
    firstNode   = NULL;
    lastNode    = NULL;
    curr        = NULL;
    len         = 0;

    postFunc    = post_func;
    postParm    = post_parm;

    setMaxListSize(max_list_size);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
template <class T, typename K>
Ordering<T,K>::~Ordering(void)
{
    clear();
}

/*----------------------------------------------------------------------------
 * add
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Ordering<T,K>::add(K key, const T& data, bool unique)
{
    return addNode(key, data, unique);
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
template <class T, typename K>
T& Ordering<T,K>::get(K key, searchMode_t smode)
{
    bool found = false;

    /* Reset Current if Necessary */
    if(curr == NULL) curr = lastNode;

    /* look for data */
    if(curr != NULL)
    {
        if (smode == EXACT_MATCH)
        {
            while (key < curr->key && curr->prev != NULL) curr = curr->prev;
            while (key > curr->key && curr->next != NULL) curr = curr->next;
            if (key == curr->key) found = true;
        }
        else if (smode == GREATER_THAN_OR_EQUAL) // i.e. you want the first node greater than or equal to the key
        {
            while (key < curr->key && curr->prev != NULL) curr = curr->prev;
            while (key > curr->key && curr->next != NULL) curr = curr->next;
            if (key <= curr->key) found = true;
        }
        else if (smode == LESS_THAN_OR_EQUAL) // i.e. you want the last node less than or equal to the key
        {
            while (key > curr->key && curr->next != NULL) curr = curr->next;
            while (key < curr->key && curr->prev != NULL) curr = curr->prev;
            if (key >= curr->key) found = true;
        }
        else if (smode == GREATER_THAN) // i.e. you want the first node greater than the key
        {
            while (key < curr->key && curr->prev != NULL) curr = curr->prev;
            while (key > curr->key && curr->next != NULL) curr = curr->next;
            if (key < curr->key) found = true;
        }
        else if (smode == LESS_THAN) // i.e. you want the last node less than the key
        {
            while (key > curr->key && curr->next != NULL) curr = curr->next;
            while (key < curr->key && curr->prev != NULL) curr = curr->prev;
            if (key > curr->key) found = true;
        }
        else // invalid search mode
        {
            assert(false);
        }
    }

    if (found)  return curr->data;
    else        throw RunTimeException(CRITICAL, RTE_ERROR, "key not found");
}

/*----------------------------------------------------------------------------
 * remove
 *
 *  TODO factor out common search code into its own function
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Ordering<T,K>::remove(K key, searchMode_t smode)
{
    bool found = false;

    /* Reset Current if Necessary */
    if(curr == NULL) curr = lastNode;

    /* look for data */
    if (curr != NULL)
    {
        if (smode == EXACT_MATCH)
        {
            while (key < curr->key && curr->prev != NULL) curr = curr->prev;
            while (key > curr->key && curr->next != NULL) curr = curr->next;
            if (key == curr->key) found = true;
        }
        else if (smode == GREATER_THAN_OR_EQUAL) // i.e. you want the first node greater than or equal to the key
        {
            while (key < curr->key && curr->prev != NULL) curr = curr->prev;
            while (key > curr->key && curr->next != NULL) curr = curr->next;
            if (key < curr->key) found = true;
        }
        else if (smode == LESS_THAN_OR_EQUAL) // i.e. you want the first node less than the key
        {
            while (key > curr->key && curr->next != NULL) curr = curr->next;
            while (key < curr->key && curr->prev != NULL) curr = curr->prev;
            if (key > curr->key) found = true;
        }
        else // invalid search mode
        {
            assert(false);
        }
    }

    if (found)
    {
        /* delete data */
        freeNode(curr);

        /* delete node */
        sorted_node_t* node = curr;

        if (curr->prev != NULL)  curr->prev->next = curr->next;
        else                     firstNode = curr->next; // reposition first node

        if (curr->next != NULL)  curr->next->prev = curr->prev;
        else                     lastNode = curr->prev; // reposition last node

        if (curr->next != NULL)  curr = curr->next;
        else                     curr = curr->prev;

        delete node;
        len--;

        /* return success */
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
template <class T, typename K>
long Ordering<T,K>::length(void)
{
    return len;
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
template <class T, typename K>
void Ordering<T,K>::clear(void)
{
    /* Set Current Pointer */
    curr = lastNode;

    /* Check for Empty List */
    if(curr == NULL)
    {
        firstNode = NULL;
        return;
    }

    /* Clear In Previous Direction */
    while(curr != NULL)
    {
        sorted_node_t* node = curr;
        curr = curr->prev;
        freeNode(node);
        delete node;
        len--;
    }

    /* Reset Parameters */
    firstNode = NULL;
    lastNode = NULL;
}

/*----------------------------------------------------------------------------
 * flush
 *----------------------------------------------------------------------------*/
template <class T, typename K>
void Ordering<T,K>::flush(void)
{
    /* Pop List */
    while(firstNode != NULL)
    {
        /* Save off First and Move it to Next */
        sorted_node_t* node = firstNode;
        firstNode = firstNode->next;

        /* Post Data */
        postNode(node);

        /* Delete Old Node */
        delete node;
        len--;
    }

    /* Reset Parameters */
    firstNode = NULL;
    lastNode = NULL;
    curr = NULL;
}

/*----------------------------------------------------------------------------
 * first
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Ordering<T,K>::first(T* data)
{
    curr = firstNode;

    if (curr != NULL)
    {
        if (data != NULL) *data = curr->data;
        return curr->key;
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * next
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Ordering<T,K>::next(T* data)
{
    if (curr != NULL)
    {
        curr = curr->next;
    }

    if (curr != NULL)
    {
        if (data != NULL) *data = curr->data;
        return curr->key;
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * last
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Ordering<T,K>::last(T* data)
{
    curr = lastNode;

    if (curr != NULL)
    {
        if (data != NULL) *data = curr->data;
        return curr->key;
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * prev
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Ordering<T,K>::prev(T* data)
{
    if (curr != NULL)
    {
        curr = curr->next;
    }

    if (curr != NULL)
    {
        if (data != NULL) *data = curr->data;
        return curr->key;
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template <class T, typename K>
Ordering<T,K>& Ordering<T,K>::operator=(const Ordering& other)
{
    /* clear existing list */
    clear();

    /* set parameters */
    maxListSize = other.maxListSize;

    /* copy over post attributes */
    postFunc = other.postFunc;
    postParm = other.postParm;

    /* build new list */
    T data;
    K key = first(&data);
    while (key != (K)INVALID_KEY)
    {
        add(key, data);
        key = next(&data);
    }

    /* return */
    return *this;
}

/*----------------------------------------------------------------------------
 * operator[]
 *----------------------------------------------------------------------------*/
template <class T, typename K>
T& Ordering<T,K>::operator[](K key)
{
    return get(key, EXACT_MATCH);
}

/*----------------------------------------------------------------------------
 * setMaxListSize
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Ordering<T,K>::setMaxListSize(long _max_list_size)
{
    if(_max_list_size >= INFINITE_LIST_SIZE)
    {
        maxListSize = _max_list_size;
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * addNode
 *
 *  take note of mid-function return point
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Ordering<T,K>::addNode(K key, const T& data, bool unique)
{
    /* Check for Valid Current Pointer */
    if (curr == NULL && lastNode != NULL) curr = lastNode;

    /* Find Insertion Point */
    if (curr != NULL)
    {
        if (key <= curr->key)
        {
            while (key <= curr->key && curr->prev != NULL)
            {
                curr = curr->prev;
            }
        }
        else // if(new_node->key > curr->key)
        {
            while (key > curr->key && curr->next != NULL)
            {
                curr = curr->next;
            }
        }
    }

    /* Check Uniqueness */
    if(unique && curr && curr->key == key)
    {
        return false;
    }

    /* Allocate New Node */
    sorted_node_t* new_node = new sorted_node_t;
    len++;

    /* Populate Node */
    new_node->key = key;
    new_node->data = data;
    new_node->next = NULL;
    new_node->prev = NULL;

    /* Add Node */
    if (curr == NULL)
    {
        curr = new_node;
        firstNode = new_node;
        lastNode = new_node;
    }
    else if (new_node->key <= curr->key)
    {
        new_node->next = curr;
        new_node->prev = curr->prev;

        if (curr->prev != NULL) curr->prev->next = new_node;
        else                    firstNode = new_node; // reposition first node

        curr->prev = new_node;
    }
    else // if(new_node->key > curr->key)
    {
        new_node->prev = curr;
        new_node->next = curr->next;

        if (curr->next != NULL) curr->next->prev = new_node;
        else                    lastNode = new_node; // reposition last node

        curr->next = new_node;
    }

    /* Post or Remove First Node */
    while ((maxListSize != INFINITE_LIST_SIZE) && (len > maxListSize))
    {
        /* Save off First and Move it to Next */
        sorted_node_t* old_node = firstNode;
        firstNode = firstNode->next;

        /* Check Curr Pointer */
        if (curr == old_node)
        {
            curr = firstNode;
        }

        /* Post/Free Data */
        postNode(old_node);

        /* Delete Old Node */
        delete old_node;
        len--;

        /* Check for Exit */
        if (firstNode != NULL)
        {
            firstNode->prev = NULL;
        }
        else // empty list
        {
            firstNode = NULL;
            curr = NULL;
            lastNode = NULL;
            break;
        }
    }

    return true;
}

/*----------------------------------------------------------------------------
 * postNode
 *----------------------------------------------------------------------------*/
template <class T, typename K>
void Ordering<T,K>::postNode(sorted_node_t* node)
{
    int status = 0;
    if(postFunc) status = postFunc(&(node->data), sizeof(T), postParm);
    if (status <= 0) freeNode(node);
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T, typename K>
void Ordering<T,K>::freeNode(sorted_node_t* node)
{
    (void)node;
}

/******************************************************************************
 MANAGED ORDERING METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, typename K, bool is_array>
MgOrdering<T,K,is_array>::MgOrdering(typename Ordering<T,K>::postFunc_t post_func, void* post_parm, K max_list_size):
    Ordering<T,K>(post_func, post_parm, max_list_size)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T, typename K, bool is_array>
MgOrdering<T,K,is_array>::~MgOrdering(void)
{
    Ordering<T,K>::clear();
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T, typename K, bool is_array>
void MgOrdering<T,K,is_array>::freeNode(typename Ordering<T,K>::sorted_node_t* node)
{
    if(!is_array)   delete node->data;
    else            delete [] node->data;
}

#endif  /* __ordering__ */
