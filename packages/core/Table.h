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
#include <stdexcept>
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

        static const int DEFAULT_TABLE_SIZE = 256;

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

                    Table       (K table_size=DEFAULT_TABLE_SIZE, hash_func_t _hash=identity);
        virtual     ~Table      (void);


        bool        add         (K key, T& data, bool overwrite=false, bool with_delete=true);
        T&          get         (K key, match_t match=MATCH_EXACTLY);
        bool        find        (K key, match_t match, T* data);
        bool        remove      (K key);
        long        length      (void);
        bool        isfull      (void);
        void        clear       (void);
    
        K           first       (T* data);
        K           next        (T* data);
        K           last        (T* data);
        K           prev        (T* data);

        Table&      operator=   (const Table& other);
        T&          operator[]  (K key);

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
        K           num_entries;
        K           oldest_entry;
        K           newest_entry;
        K           current_entry;
        K           open_entry;
        
        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/
        
        static K        identity        (K key);
        bool            writeNode       (K index, K key, T& data);
        bool            overwriteNode   (K index, K key, T& data, bool with_delete);
        virtual void    freeNode        (K index);
};

/******************************************************************************
 * MANAGED TABLE TEMPLATE
 ******************************************************************************/

template <class T, typename K=unsigned long, bool is_array=false>
class MgTable: public Table<T,K>
{
    public:
        MgTable (K table_size=Table<T,K>::DEFAULT_TABLE_SIZE, typename Table<T,K>::hash_func_t _hash=Table<T,K>::identity);
        ~MgTable (void);
    private:
        void freeNode (K index) override;
};

/******************************************************************************
 ORDERING METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, typename K>
Table<T,K>::Table(K table_size, hash_func_t _hash)
{
    assert(table_size > 0);

    /*  Set Hash Function */
    hash = _hash;
    
    /* Allocate Hash Structure */
    size = table_size;
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
    /* Free Hash Structure */
    delete [] table;
}

/*----------------------------------------------------------------------------
 * add
 * 
 *  Note - mid-function returns
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::add(K key, T& data, bool overwrite, bool with_delete)
{
    K curr_index = hash(key) % size;

    /* Add Entry to Hash */
    if(table[curr_index].occupied == false)
    {
        /* Remove Index from Open List */
        K next_index = table[curr_index].next;
        K prev_index = table[curr_index].prev;
        if(next_index != (K)INVALID_KEY) table[next_index].prev = prev_index;
        if(prev_index != (K)INVALID_KEY) table[prev_index].next = next_index;

        /* Update Open Entry if Collision on Head */
        if(open_entry == curr_index) open_entry = next_index;
        
        /* Populate Index */
        writeNode(curr_index, key, data);
    }
    else /* collision */
    {
        /* Check Current Slot for Duplicate */
        if(table[curr_index].key == key)
        {
            if(overwrite)   return overwriteNode(curr_index, key, data, with_delete);
            else            return false;
        }

        /* Transverse to End of Chain */
        K end_index = curr_index;
        K scan_index = table[curr_index].next;
        while(scan_index != (K)INVALID_KEY)
        {
            /* Check Slot for Duplicate */
            if(table[scan_index].key == key)
            {
                if(overwrite)   return overwriteNode(scan_index, key, data, with_delete);
                else            return false;
            }

            /* Go To Next Slot */
            end_index = scan_index;
            scan_index = table[scan_index].next;
        }

        /* Find First Open Hash Slot */
        K open_index = open_entry;
        if(open_index == (K)INVALID_KEY)
        {
            /* Hash Full */
            return false;
        }

        /* Move Open Entry to Next Open Index */
        open_entry = table[open_entry].next;
        if(open_entry != (K)INVALID_KEY) table[open_entry].prev = (K)INVALID_KEY;

        /* Insert Node */
        if(table[curr_index].prev == (K)INVALID_KEY) /* End of Chain Insertion (chain == 1) */
        {
            /* Add Entry to Open Slot at End of Chain */
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
            if(oldest_entry == curr_index)
            {
                oldest_entry = open_index;
                table[oldest_entry].before = (K)INVALID_KEY;
            }

            /* Update Newest Entry */
            if(newest_entry == curr_index)
            {
                newest_entry = open_index;
                table[newest_entry].after = (K)INVALID_KEY;
            }

            /* Add Entry to Current Slot */
            writeNode(curr_index, key, data);
        }
    }

    /* New Entry Added */
    num_entries++;

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
template <class T, typename K>
T& Table<T,K>::get(K key, match_t match)
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
            best_delta = (K)0;
            best_index = curr_index;
            break;
        }
        else if(match == MATCH_NEAREST_UNDER)
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
        return table[best_index].data;
    }

    /* Throw Exception When Not Found */
    throw std::out_of_range("key not found");
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::find(K key, match_t match, T* data)
{
    try
    {
        T& entry = get(key, match);
        if(data) *data = entry;
        return true;
    }
    catch(const std::exception& e)
    {
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
        if(table[curr_index].occupied == false) /* end of chain */
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
    if(curr_index == newest_entry)  newest_entry = before_index;
    if(curr_index == oldest_entry)  oldest_entry = after_index;

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
        if(end_index == newest_entry)  newest_entry = curr_index;
        if(end_index == oldest_entry)  oldest_entry = curr_index;
    }

    /* Remove End of Chain */
    K open_index = end_index;
    table[open_index].occupied = false;

    /* Update Hash Order */
    K prev_index = table[open_index].prev;
    if(prev_index != (K)INVALID_KEY) table[prev_index].next = (K)INVALID_KEY;

    /* Add to Open List */
    table[open_index].prev = (K)INVALID_KEY;
    table[open_index].next = open_entry;
    open_entry = open_index;

    /* Update Statistics */
    num_entries--;

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * length
 *----------------------------------------------------------------------------*/
template <class T, typename K>
long Table<T,K>::length(void)
{
    return num_entries;
}

/*----------------------------------------------------------------------------
 * isfull
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::isfull(void)
{
    return num_entries >= size;
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
        if(table[i].occupied == true)
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
    num_entries     = 0;
    oldest_entry    = (K)INVALID_KEY;
    newest_entry    = (K)INVALID_KEY;
    current_entry   = (K)INVALID_KEY;

    /* Build Open List */
    open_entry = (K)0;
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
    current_entry = oldest_entry;
    if(current_entry != (K)INVALID_KEY)
    {
        assert(table[current_entry].occupied);
        if(data) *data = table[current_entry].data;
        return table[current_entry].key;
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * next
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Table<T,K>::next(T* data)
{
    if(current_entry != (K)INVALID_KEY)
    {
        current_entry = table[current_entry].after;
        if(current_entry != (K)INVALID_KEY)
        {
            assert(table[current_entry].occupied);
            if(data) *data = table[current_entry].data;
            return table[current_entry].key;
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
    current_entry = newest_entry;
    if(current_entry != (K)INVALID_KEY)
    {
        assert(table[current_entry].occupied);
        if(data) *data = table[current_entry].data;
        return table[current_entry].key;
    }

    return (K)INVALID_KEY;
}

/*----------------------------------------------------------------------------
 * prev
 *----------------------------------------------------------------------------*/
template <class T, typename K>
K Table<T,K>::prev(T* data)
{
    if(current_entry != (K)INVALID_KEY)
    {
        current_entry = table[current_entry].before;
        if(current_entry != (K)INVALID_KEY)
        {
            assert(table[current_entry].occupied);
            if(data) *data = table[current_entry].data;
            return table[current_entry].key;
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
 * writeNode
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::writeNode(K index, K key, T& data)
{
    table[index].occupied   = true;
    table[index].data       = data;
    table[index].key        = key;
    table[index].next       = (K)INVALID_KEY;
    table[index].prev       = (K)INVALID_KEY;
    table[index].after      = (K)INVALID_KEY;
    table[index].before     = newest_entry;

    /* Update Time Order */
    if(oldest_entry == (K)INVALID_KEY)
    {
        /* First Entry */
        oldest_entry = index;
        newest_entry = index;
    }
    else
    {
        /* Not First Entry */
        table[newest_entry].after = index;
        newest_entry = index;
    }

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * overwriteNode
 *----------------------------------------------------------------------------*/
template <class T, typename K>
bool Table<T,K>::overwriteNode(K index, K key, T& data, bool with_delete)
{
    /* Delete Entry being Overritten (if requested) */
    if(with_delete)
    {
        freeNode(index);
    }

    /* Set Data */
    table[index].key = key;
    table[index].data = data;

    /* Bridge Over Entry */
    K before_index = table[index].before;
    K after_index = table[index].after;
    if(before_index != (K)INVALID_KEY) table[before_index].after = after_index;
    if(after_index != (K)INVALID_KEY) table[after_index].before = before_index;

    /* Check if Overwriting Oldest/Newest */
    if(index == oldest_entry) oldest_entry = after_index;
    if(index == newest_entry) newest_entry = before_index;

    /* Set Current Entry as Newest */
    K oldest_index = oldest_entry;
    K newest_index = newest_entry;
    table[index].after = (K)INVALID_KEY;
    table[index].before = newest_index;
    newest_entry = index;

    /* Update Newest/Oldest */
    if(newest_index != (K)INVALID_KEY) table[newest_index].after = index;
    if(oldest_index == (K)INVALID_KEY) oldest_entry = index;

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T, typename K>
void Table<T,K>::freeNode(K index)
{
    (void)index;
}

/******************************************************************************
 MANAGED ORDERING METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, typename K, bool is_array>
MgTable<T,K,is_array>::MgTable(K table_size, typename Table<T,K>::hash_func_t _hash): 
    Table<T,K>(table_size, _hash)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T, typename K, bool is_array>
MgTable<T,K,is_array>::~MgTable(void)
{
    /* clearing table required to free nodes */
    Table<T,K>::clear();
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T, typename K, bool is_array>
void MgTable<T,K,is_array>::freeNode(K index)
{
    if(!is_array)   delete Table<T,K>::table[index].data;
    else            delete [] Table<T,K>::table[index].data;
}

#endif  /* __table__ */
