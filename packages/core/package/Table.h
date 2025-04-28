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

#ifndef __table__
#define __table__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <assert.h>
#include "OsApi.h"

/******************************************************************************
 * TABLE TEMPLATE
 ******************************************************************************/
/*
 * Table - time sorted hash of data type T and index type K
 */
template <class T, typename K=unsigned long>
class Table
{
    public:

        /*--------------------------------------------------------------------
         * CONSTANTS
         *--------------------------------------------------------------------*/

        static const int DEFAULT_TABLE_SIZE = 257;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef K (*hash_func_t) (K);

        typedef enum {
            MATCH_EXACTLY,
            MATCH_NEAREST_UNDER,
            MATCH_NEAREST_OVER
        } match_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit    Table       (K table_size=DEFAULT_TABLE_SIZE, hash_func_t _hash=identity, bool _no_throw=false);
        virtual     ~Table      (void);


        bool        add         (K key, const T& data, bool unique);
        T&          get         (K key, match_t match=MATCH_EXACTLY, bool resort=false);
        bool        find        (K key, match_t match, T* data, bool resort=false);
        bool        remove      (K key);
        long        length      (void) const;
        bool        isfull      (void) const;
        void        clear       (void);

        K           first       (T* data);
        K           next        (T* data);
        K           last        (T* data);
        K           prev        (T* data);

        Table&      operator=   (const Table& other);
        T&          operator[]  (K key);

        static K    identity    (K key);

    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            bool    occupied;
            T       data;
            K       key;
            K       next;   // next entry in chain
            K       prev;   // previous entry in chain
            K       after;  // next entry added to hash (time ordered)
            K       before; // previous entry added to hash (time ordered)
        } node_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        hash_func_t hash;
        node_t*     table;
        K           size;
        K           numEntries;
        K           oldestEntry;
        K           newestEntry;
        K           currentEntry;
        K           openEntry;
        bool        noThrow;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool            addNode         (K key, const T* data, bool unique, K* index);
        bool            writeNode       (K index, K key, const T* data);
        bool            overwriteNode   (K index, K key, const T* data);
        void            makeNewest      (K index);
        void            freeNode        (K index);
};

/******************************************************************************
 TABLE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, typename K>
Table<T,K>::Table(K table_size, hash_func_t _hash, bool _no_throw):
    hash(_hash),
    size(table_size),
    noThrow(_no_throw)
{
    assert(table_size > 0);

    /* Allocate Hash Structure */
    table = new node_t [size];

    /* Set All Entries to Empty */
    for(K i = 0; i < size; i++)
    {
        table[i].occupied = false;
    }

    /* Initialize Hash Table */
    clear();
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
template <class T, typename K>
Table<T,K>::~Table(void)
{
    /* Free All Nodes */
    clear();

    /* Free Hash Structure */
    delete [] table;
}

/*----------------------------------------------------------------------------
 * add
 *
 *  Note - mid-function returns
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::add(K key, const T& data, bool unique)
{
    return addNode(key, &data, unique, NULL);
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
template <class T, typename K>
T& Table<T,K>::get(K key, match_t match, bool resort)
{
    K curr_index = hash(key) % size;

    /* Find Node to Return */
    K best_delta = (K)INVALID_KEY;
    K best_index = (K)INVALID_KEY;
    while((curr_index != (K)INVALID_KEY) && table[curr_index].occupied)
    {
        if(table[curr_index].key == key)
        {
            /* equivalent key is always nearest */
            best_index = curr_index;
            break;
        }

        if(match == MATCH_NEAREST_UNDER)
        {
            if(table[curr_index].key < key)
            {
                K delta = key - table[curr_index].key;
                if(delta < best_delta)
                {
                    best_delta = delta;
                    best_index = curr_index;
                }
            }
        }
        else if(match == MATCH_NEAREST_OVER)
        {
            if(table[curr_index].key > key)
            {
                K delta = table[curr_index].key - key;
                if(delta < best_delta)
                {
                    best_delta = delta;
                    best_index = curr_index;
                }
            }
        }

        /* go to next */
        curr_index = table[curr_index].next;
    }

    /* Check if Node Found */
    if(best_index != (K)INVALID_KEY)
    {
        if(resort)
        {
            makeNewest(best_index);
        }

        return table[best_index].data;
    }
    else if(noThrow)
    {
        K index;
        if(addNode(key, NULL, true, &index))
        {
            if(index != (K)INVALID_KEY)
            {
                return table[index].data;
            }
        }
    }

    /* Throw Exception When Not Found */
    throw RunTimeException(CRITICAL, RTE_FAILURE, "key not found");
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::find(K key, match_t match, T* data, bool resort)
{
    try
    {
        const T& entry = get(key, match, resort);
        if(data) *data = entry;
        return true;
    }
    catch(const RunTimeException& e)
    {
        (void)e;
        return false;
    }
}

/*----------------------------------------------------------------------------
 * remove
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::remove(K key)
{
    K curr_index = hash(key) % size;

    /* Find Node to Remove */
    while(curr_index != (K)INVALID_KEY)
    {
        if(!table[curr_index].occupied) /* end of chain */
        {
            curr_index = (K)INVALID_KEY;
        }
        else if(table[curr_index].key == key) /* matched key */
        {
            break;
        }
        else /* go to next */
        {
            curr_index = table[curr_index].next;
        }
    }

    /* Check if Node Found */
    if(curr_index == (K)INVALID_KEY)
    {
        return false;
    }

    /* Remove Bundle */
    freeNode(curr_index);

    /* Update Time Order (Bridge) */
    K after_index  = table[curr_index].after;
    K before_index = table[curr_index].before;
    if(after_index != (K)INVALID_KEY)   table[after_index].before = before_index;
    if(before_index != (K)INVALID_KEY)  table[before_index].after = after_index;

    /* Update Newest and Oldest Entry */
    if(curr_index == newestEntry)  newestEntry = before_index;
    if(curr_index == oldestEntry)  oldestEntry = after_index;

    /* Remove End of Chain */
    K end_index = curr_index;
    K next_index = table[end_index].next;
    if(next_index != (K)INVALID_KEY)
    {
        /* Transverse to End of Chain */
        end_index = next_index;
        while(table[end_index].next != (K)INVALID_KEY)
        {
            end_index = table[end_index].next;
        }

        /* Copy End of Chain into Removed Slot */
        table[curr_index].occupied  = true;
        table[curr_index].key       = table[end_index].key;
        table[curr_index].data      = table[end_index].data;
        table[curr_index].before    = table[end_index].before;
        table[curr_index].after     = table[end_index].after;

        /* Update Time Order (Move) */
        after_index  = table[end_index].after;
        before_index = table[end_index].before;
        if(after_index != (K)INVALID_KEY)     table[after_index].before = curr_index;
        if(before_index != (K)INVALID_KEY)    table[before_index].after = curr_index;

        /* Update Newest and Oldest Entry */
        if(end_index == newestEntry)  newestEntry = curr_index;
        if(end_index == oldestEntry)  oldestEntry = curr_index;
    }

    /* Remove End of Chain */
    K open_index = end_index;
    table[open_index].occupied = false;

    /* Update Hash Order */
    K prev_index = table[open_index].prev;
    if(prev_index != (K)INVALID_KEY) table[prev_index].next = (K)INVALID_KEY;

    /* Add to Open List */
    table[open_index].prev = (K)INVALID_KEY;
    table[open_index].next = openEntry;
    openEntry = open_index;

    /* Update Statistics */
    numEntries--;

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * length
 *----------------------------------------------------------------------------*/
template <class T, typename K>
long Table<T,K>::length(void) const
{
    return numEntries;
}

/*----------------------------------------------------------------------------
 * isfull
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::isfull(void) const
{
    return numEntries >= size;
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
template <class T, typename K>
void Table<T,K>::clear(void)
{
    /* Initialize Hash Table */
    for(K i = 0; i < size; i++)
    {
        /* Free Data */
        if(table[i].occupied)
        {
            freeNode(i);
        }

        /* Initialize Entry */
        table[i].occupied = false;
        table[i].key    = (K)INVALID_KEY;
        table[i].next   = (K)INVALID_KEY;
        table[i].prev   = (K)INVALID_KEY;
        table[i].before = (K)INVALID_KEY;
        table[i].after  = (K)INVALID_KEY;
    }

    /* Initialize Hash Attributes */
    numEntries     = 0;
    oldestEntry    = (K)INVALID_KEY;
    newestEntry    = (K)INVALID_KEY;
    currentEntry   = (K)INVALID_KEY;

    /* Build Open List */
    openEntry = (K)0;
    for(K i = 0; i < size; i++)
    {
        table[i].prev = i - 1;
        table[i].next = i + 1;
    }
    table[0].prev = (K)INVALID_KEY;
    table[size - 1].next = (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * first
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Table<T,K>::first(T* data)
{
    currentEntry = oldestEntry;
    if(currentEntry != (K)INVALID_KEY)
    {
        assert(table[currentEntry].occupied);
        if(data) *data = table[currentEntry].data;
        return table[currentEntry].key;
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * next
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Table<T,K>::next(T* data)
{
    if(currentEntry != (K)INVALID_KEY)
    {
        currentEntry = table[currentEntry].after;
        if(currentEntry != (K)INVALID_KEY)
        {
            assert(table[currentEntry].occupied);
            if(data) *data = table[currentEntry].data;
            return table[currentEntry].key;
        }
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * last
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Table<T,K>::last(T* data)
{
    currentEntry = newestEntry;
    if(currentEntry != (K)INVALID_KEY)
    {
        assert(table[currentEntry].occupied);
        if(data) *data = table[currentEntry].data;
        return table[currentEntry].key;
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * prev
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Table<T,K>::prev(T* data)
{
    if(currentEntry != (K)INVALID_KEY)
    {
        currentEntry = table[currentEntry].before;
        if(currentEntry != (K)INVALID_KEY)
        {
            assert(table[currentEntry].occupied);
            if(data) *data = table[currentEntry].data;
            return table[currentEntry].key;
        }
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template <class T, typename K>
Table<T,K>& Table<T,K>::operator=(const Table& other)
{
    /* check for self assignment */
    if(this == &other) return *this;

    /* set hash function */
    hash = other.hash;

    /* clear existing table */
    clear(); // calls freeNode needed for managed tables
    delete [] table;

    /* set parameters */
    size = other.size;
    table = new node_t [size];

    /* initialize new table */
    clear();

    /* build new table */
    T data;
    K key = first(&data);
    while(key != (K)INVALID_KEY)
    {
        add(key, data);
        key = next(&data);
    }

    /* return */
    return *this;
}

/*----------------------------------------------------------------------------
 * operator[]
 *
 *   NOTE:  this operator does not support matching other than EXACTLY,
 *          and does not support re-sorting the returned node as the newest
 *----------------------------------------------------------------------------*/
template <class T, typename K>
T& Table<T,K>::operator[](K key)
{
    return get(key);
}

/*----------------------------------------------------------------------------
 * identity
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Table<T,K>::identity(K key)
{
    return key;
}

/*----------------------------------------------------------------------------
 * addNode
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::addNode (K key, const T* data, bool unique, K* index)
{
    K curr_index = hash(key) % size;

    /* Add Entry to Hash */
    if(!table[curr_index].occupied)
    {
        /* Remove Index from Open List */
        K next_index = table[curr_index].next;
        K prev_index = table[curr_index].prev;
        if(next_index != (K)INVALID_KEY) table[next_index].prev = prev_index;
        if(prev_index != (K)INVALID_KEY) table[prev_index].next = next_index;

        /* Update Open Entry if Collision on Head */
        if(openEntry == curr_index) openEntry = next_index;

        /* Populate Index */
        writeNode(curr_index, key, data);
        if(index) *index = curr_index;
    }
    else /* collision */
    {
        /* Check Current Slot for Duplicate */
        if(table[curr_index].key == key)
        {
            if(index) *index = curr_index;
            if(!unique) return overwriteNode(curr_index, key, data);
            return false;
        }

        /* Transverse to End of Chain */
        K end_index = curr_index;
        K scan_index = table[curr_index].next;
        while(scan_index != (K)INVALID_KEY)
        {
            /* Check Slot for Duplicate */
            if(table[scan_index].key == key)
            {
                if(index) *index = scan_index;
                if(!unique) return overwriteNode(scan_index, key, data);
                return false;
            }

            /* Go To Next Slot */
            end_index = scan_index;
            scan_index = table[scan_index].next;
        }

        /* Find First Open Hash Slot */
        K open_index = openEntry;
        if(open_index == (K)INVALID_KEY)
        {
            /* Hash Full */
            if(index) *index = (K)INVALID_KEY;
            return false;
        }

        /* Move Open Entry to Next Open Index */
        openEntry = table[openEntry].next;
        if(openEntry != (K)INVALID_KEY) table[openEntry].prev = (K)INVALID_KEY;

        /* Insert Node */
        if(table[curr_index].prev == (K)INVALID_KEY) /* End of Chain Insertion (chain == 1) */
        {
            /* Add Entry to Open Slot at End of Chain */
            if(index) *index = open_index;
            writeNode(open_index, key, data);
            table[end_index].next = open_index;
            table[open_index].prev = end_index;
        }
        else /* Robin Hood Insertion (chain > 1) */
        {
            /* Copy Current Slot to Open Slot */
            table[open_index] = table[curr_index];

            /* Update Hash Links */
            K next_index = table[curr_index].next;
            K prev_index = table[curr_index].prev;
            if(next_index != (K)INVALID_KEY) table[next_index].prev = open_index;
            if(prev_index != (K)INVALID_KEY) table[prev_index].next = open_index;

            /* Update Time Order (Move) */
            K after_index  = table[curr_index].after;
            K before_index = table[curr_index].before;
            if(after_index != (K)INVALID_KEY)   table[after_index].before = open_index;
            if(before_index != (K)INVALID_KEY)  table[before_index].after = open_index;

            /* Update Oldest Entry */
            if(oldestEntry == curr_index)
            {
                oldestEntry = open_index;
                table[oldestEntry].before = (K)INVALID_KEY;
            }

            /* Update Newest Entry */
            if(newestEntry == curr_index)
            {
                newestEntry = open_index;
                table[newestEntry].after = (K)INVALID_KEY;
            }

            /* Add Entry to Current Slot */
            if(index) *index = curr_index;
            writeNode(curr_index, key, data);
        }
    }

    /* New Entry Added */
    numEntries++;

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * writeNode
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::writeNode(K index, K key, const T* data)
{
    table[index].occupied   = true;
    table[index].key        = key;
    table[index].next       = (K)INVALID_KEY;
    table[index].prev       = (K)INVALID_KEY;
    table[index].after      = (K)INVALID_KEY;
    table[index].before     = newestEntry;

    /* Conditionally Write Data */
    if(data) table[index].data = *data;

    /* Update Time Order */
    if(oldestEntry == (K)INVALID_KEY)
    {
        /* First Entry */
        oldestEntry = index;
        newestEntry = index;
    }
    else
    {
        /* Not First Entry */
        table[newestEntry].after = index;
        newestEntry = index;
    }

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * overwriteNode
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::overwriteNode(K index, K key, const T* data)
{
    /* Delete Entry being Overritten (if requested) */
    freeNode(index);

    /* Set Key */
    table[index].key = key;

    /* Conditionally Write Data */
    if(data) table[index].data = *data;

    /* Make Current Node the Newest Node */
    makeNewest(index);

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * makeNewest
 *----------------------------------------------------------------------------*/
template <class T, typename K>
void Table<T,K>::makeNewest(K index)
{
    /* Bridge Over Entry */
    K before_index = table[index].before;
    K after_index = table[index].after;
    if(before_index != (K)INVALID_KEY) table[before_index].after = after_index;
    if(after_index != (K)INVALID_KEY) table[after_index].before = before_index;

    /* Check if Overwriting Oldest/Newest */
    if(index == oldestEntry) oldestEntry = after_index;
    if(index == newestEntry) newestEntry = before_index;

    /* Set Current Entry as Newest */
    K oldest_index = oldestEntry;
    K newest_index = newestEntry;
    table[index].after = (K)INVALID_KEY;
    table[index].before = newest_index;
    newestEntry = index;

    /* Update Newest/Oldest */
    if(newest_index != (K)INVALID_KEY) table[newest_index].after = index;
    if(oldest_index == (K)INVALID_KEY) oldestEntry = index;
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T>
void tableDeleteIfPointer(const T& t) { (void)t; }

template <class T>
void tableDeleteIfPointer(T* t) { delete t; }

template <class T, typename K>
void Table<T,K>::freeNode(K index)
{
    tableDeleteIfPointer(table[index].data);
}

#endif  /* __table__ */
