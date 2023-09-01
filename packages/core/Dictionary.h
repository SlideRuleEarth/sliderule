
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

#ifndef __dictionary__
#define __dictionary__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include <climits>
#include <assert.h>

/******************************************************************************
 * DICTIONARY TEMPLATE
 ******************************************************************************/

template <class T>
class Dictionary
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int            DEFAULT_HASH_TABLE_SIZE = 256;
        static const unsigned int   EMPTY_ENTRY             = 0; // must be 0 because hashTable initialized to 0's
        static const unsigned int   NULL_INDEX              = UINT_MAX;
        static const double         DEFAULT_HASH_TABLE_LOAD; // statically defined below

        /*--------------------------------------------------------------------
         * Iterator Subclass
         *--------------------------------------------------------------------*/

        typedef struct kv {
            kv(const char* _key, const T& _value): key(_key), value(_value) {};
            ~kv(void) {};
            const char* key;
            const T&    value;
        } kv_t;

        class Iterator
        {
            public:
                                    Iterator    (const Dictionary& d);
                                    ~Iterator   (void);
                kv_t                operator[]  (int index);
                const int           length;
            private:
                const Dictionary&   source;
                int                 table_index; // where in source hashtable
                int                 curr_index; // how many elements in hashtable to the table_index
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    Dictionary      (int hash_size=DEFAULT_HASH_TABLE_SIZE, double hash_load=DEFAULT_HASH_TABLE_LOAD);
        virtual     ~Dictionary     (void);

        bool        add             (const char* key, T& data, bool unique=false);
        T&          get             (const char* key) const;
        bool        find            (const char* key, T* data=NULL) const;
        bool        remove          (const char* key);
        int         length          (void) const;
        int         getHashSize     (void) const;
        int         getMaxChain     (void) const;
        int         getKeys         (char*** keys) const;
        void        clear           (void);

        const char* first           (T* data);
        const char* next            (T* data);
        const char* prev            (T* data);
        const char* last            (T* data);

        Dictionary& operator=       (const Dictionary& other);
        T&          operator[]      (const char* key) const;

    protected:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            const char*     key;
            T               data;
            unsigned int    chain;  // depth of the chain to reach this entry, 0 indicates empty
            unsigned int    hash;   // unconstrained hash value
            unsigned int    next;
            unsigned int    prev;
        } hash_node_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        typename Dictionary<T>::hash_node_t* hashTable;
        unsigned int hashSize;
        unsigned int numEntries;
        unsigned int maxChain;
        double hashLoad;
        unsigned int currIndex;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        unsigned int    hashKey     (const char* key) const;  // returns unconstrained hash
        unsigned int    getNode     (const char* key) const;  // returns index into hash table
        void            addNode     (const char* key, T& data, unsigned int hash, bool rehashed=false);
        virtual void    freeNode    (unsigned int hash_index);
};

/******************************************************************************
 * MANAGED DICTIONARY TEMPLATE
 ******************************************************************************/

template <class T, bool is_array=false>
class MgDictionary: public Dictionary<T>
{
    public:
        MgDictionary (int hash_size=Dictionary<T>::DEFAULT_HASH_TABLE_SIZE, double hash_load=Dictionary<T>::DEFAULT_HASH_TABLE_LOAD);
        ~MgDictionary (void);
    private:
        void freeNode (unsigned int hash_index);
};

/******************************************************************************
 * ITERATOR METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
Dictionary<T>::Iterator::Iterator(const Dictionary& d):
    length(d.numEntries),
    source(d),
    table_index(-1),
    curr_index(-1)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
Dictionary<T>::Iterator::~Iterator(void)
{
}

/*----------------------------------------------------------------------------
 * []
 *----------------------------------------------------------------------------*/
template <class T>
typename Dictionary<T>::kv_t Dictionary<T>::Iterator::operator[](int index)
{
    if( (index < length) && (index >= 0) )
    {
        while(curr_index < index) // move forward
        {
            table_index++;
            if(source.hashTable[table_index].chain != EMPTY_ENTRY)
            {
                curr_index++;
            }
        }

        while(curr_index > index) // move backwards
        {
            table_index--;
            if(source.hashTable[table_index].chain != EMPTY_ENTRY)
            {
                curr_index--;
            }
        }

        return Dictionary<T>::kv_t(source.hashTable[table_index].key, source.hashTable[table_index].data);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Dictionary::Iterator index out of range");
    }
}

/******************************************************************************
 * PUBLIC STATIC DATA
 ******************************************************************************/

template <class T>
const double Dictionary<T>::DEFAULT_HASH_TABLE_LOAD = 0.75;

/******************************************************************************
 * DICTIONARY METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
Dictionary<T>::Dictionary(int hash_size, double hash_load)
{
    assert(hash_size > 0);

    hashSize = hash_size;

    if(hash_load <= 0.0 || hash_load > 1.0)
    {
        hashLoad = DEFAULT_HASH_TABLE_LOAD;
    }
    else
    {
        hashLoad = hash_load;
    }

    hashTable = new hash_node_t [hashSize];
    for(unsigned int i = 0; i < hashSize; i++) hashTable[i].chain = EMPTY_ENTRY;

    currIndex = 0;
    numEntries = 0;
    maxChain = 0;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
Dictionary<T>::~Dictionary(void)
{
    clear();
    delete [] hashTable;
}

/*----------------------------------------------------------------------------
 * add
 *
 *  if not unique then old data is automatically deleted and overwritten
 *----------------------------------------------------------------------------*/
template <class T>
bool Dictionary<T>::add(const char* key, T& data, bool unique)
{
    assert(key);

    bool status = true;

    /* Insert Entry into Dictionary */
    unsigned int index = getNode(key);
    if(index == NULL_INDEX)
    {
        /* Check for Rehash Needed */
        if(numEntries > (hashSize * hashLoad))
        {
            unsigned int old_hash_size = hashSize;
            hash_node_t* old_hash_table = hashTable;

            unsigned int new_hash_size = hashSize * 2;              // double size of hash table
            if(new_hash_size > 0 && new_hash_size > hashSize)
            {
                hashSize = new_hash_size;
                hashTable = new hash_node_t [hashSize];
                for(unsigned int i = 0; i < hashSize; i++)
                {
                    hashTable[i].chain = EMPTY_ENTRY;
                }

                maxChain = 0;

                for(unsigned int i = 0; i < old_hash_size; i++)
                {
                    if(old_hash_table[i].chain != EMPTY_ENTRY)
                    {
                        addNode(old_hash_table[i].key,
                                old_hash_table[i].data,
                                old_hash_table[i].hash,
                                true); // rehash doesn't reallocate key
                    }
                }

                delete [] old_hash_table;
            }
            else
            {
                /* Unable to Make Hash Larger */
                status = false;
            }
        }

        /* Add Node */
        if(status == true)
        {
            addNode(key, data, hashKey(key));
            numEntries++;
        }
    }
    else if(!unique)
    {
        /* Free Old Data */
        freeNode(index);

        /* Add New Data */
        hashTable[index].data = data;
    }
    else
    {
        /* Refuse to Overwrite Existing Node */
        status = false;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * get
 *----------------------------------------------------------------------------*/
template <class T>
T& Dictionary<T>::get(const char* key) const
{
    unsigned int index = getNode(key);
    if(index != NULL_INDEX) return hashTable[index].data;
    else                    throw RunTimeException(CRITICAL, RTE_ERROR, "key <%s> not found", key);
}

/*----------------------------------------------------------------------------
 * find
 *
 *  returns false if key not in dictionary, else returns true
 *----------------------------------------------------------------------------*/
template <class T>
bool Dictionary<T>::find(const char* key, T* data) const
{
    bool found = false;

    if(key != NULL)
    {
        unsigned int index = getNode(key);
        if(index != NULL_INDEX)
        {
            found = true;
            if(data) *data = hashTable[index].data;
        }
    }

    return found;
}

/*----------------------------------------------------------------------------
 * remove
 *----------------------------------------------------------------------------*/
template <class T>
bool Dictionary<T>::remove(const char* key)
{
    bool status = true;

    /* Check Pointers */
    unsigned int index = getNode(key);
    if(index != NULL_INDEX)
    {
        /* Delete Node */
        delete [] hashTable[index].key;
        freeNode(index);

        /* Get List Indices */
        unsigned int next_index = hashTable[index].next;
        unsigned int prev_index = hashTable[index].prev;

        if((hashTable[index].chain == 1) && (next_index != NULL_INDEX))
        {
            hashTable[next_index].chain = EMPTY_ENTRY;

            /* Copy Next Item into Start of Chain */
            hashTable[index].key   = hashTable[next_index].key;
            hashTable[index].data  = hashTable[next_index].data;
            hashTable[index].hash  = hashTable[next_index].hash;
            hashTable[index].next  = hashTable[next_index].next;
            hashTable[index].prev  = NULL_INDEX;

            /* Goto Next Entry (for later transversal) */
            next_index = hashTable[index].next;
            if(next_index != NULL_INDEX) hashTable[next_index].prev = index;
        }
        else
        {
            hashTable[index].chain = EMPTY_ENTRY;

            /* Bridge Over Removed Entry */
            if(next_index != NULL_INDEX) hashTable[next_index].prev = prev_index;
            if(prev_index != NULL_INDEX) hashTable[prev_index].next = next_index;
        }

        /* Transverse to End of Chain */
        while(next_index != NULL_INDEX)
        {
            hashTable[next_index].chain--;
            next_index = hashTable[next_index].next;
        }

        /* Update Statistics */
        numEntries--;
    }
    else
    {
        /* Key Not Found */
        status = false;
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * length
 *----------------------------------------------------------------------------*/
template <class T>
int Dictionary<T>::length(void) const
{
    return numEntries;
}

/*----------------------------------------------------------------------------
 * getHashSize
 *----------------------------------------------------------------------------*/
template <class T>
int Dictionary<T>::getHashSize(void) const
{
    return hashSize;
}

/*----------------------------------------------------------------------------
 * getMaxChain
 *----------------------------------------------------------------------------*/
template <class T>
int Dictionary<T>::getMaxChain(void) const
{
    return maxChain;
}

/*----------------------------------------------------------------------------
 * getKeys
 *----------------------------------------------------------------------------*/
template <class T>
int Dictionary<T>::getKeys (char*** keys) const
{
    if (numEntries <= 0) return 0;

    *keys = new char* [numEntries];
    for(unsigned int i = 0, j = 0; i < hashSize; i++)
    {
        if(hashTable[i].chain != EMPTY_ENTRY)
        {
            char* new_key = NULL;
            OsApi::dupstr(&new_key, hashTable[i].key);
            (*keys)[j++] = new_key;
        }
    }

    return numEntries;
}

/*----------------------------------------------------------------------------
 * clear
 *
 *  deletes everything in the dictionary
 *----------------------------------------------------------------------------*/
template <class T>
void Dictionary<T>::clear (void)
{
    /* Clear Hash */
    for(unsigned int i = 0; numEntries > 0 && i < hashSize; i++)
    {
        if(hashTable[i].chain != EMPTY_ENTRY)
        {
            hashTable[i].chain = EMPTY_ENTRY;
            delete [] hashTable[i].key;
            numEntries--;
            freeNode(i);
        }
    }

    /* Clear Attributes */
    maxChain = 0;
}

/*----------------------------------------------------------------------------
 * first
 *----------------------------------------------------------------------------*/
template <class T>
const char* Dictionary<T>::first (T* data)
{
    const char* key = NULL;

    currIndex = 0;
    while(currIndex < hashSize)
    {
        if(hashTable[currIndex].chain != EMPTY_ENTRY)
        {
            if(data) *data = hashTable[currIndex].data;
            key = hashTable[currIndex].key;
            break;
        }
        currIndex++;
    }

    return key;
}

/*----------------------------------------------------------------------------
 * next
 *----------------------------------------------------------------------------*/
template <class T>
const char* Dictionary<T>::next (T* data)
{
    const char* key = NULL;

    while(++currIndex < hashSize)
    {
        if(hashTable[currIndex].chain != EMPTY_ENTRY)
        {
            if(data) *data = hashTable[currIndex].data;
            key = hashTable[currIndex].key;
            break;
        }
    }

    return key;
}

/*----------------------------------------------------------------------------
 * prev
 *----------------------------------------------------------------------------*/
template <class T>
const char* Dictionary<T>::prev (T* data)
{
    const char* key = NULL;

    while(--currIndex < hashSize) // since we are using unsigned math, the .lt. is appropriate
    {
        if(hashTable[currIndex].chain != EMPTY_ENTRY)
        {
            if(data) *data = hashTable[currIndex].data;
            key = hashTable[currIndex].key;
            break;
        }
    }

    return key;
}

/*----------------------------------------------------------------------------
 * last
 *----------------------------------------------------------------------------*/
template <class T>
const char* Dictionary<T>::last (T* data)
{
    const char* key = NULL;

    currIndex = hashSize - 1;
    while(currIndex < hashSize)
    {
        if(hashTable[currIndex].chain != EMPTY_ENTRY)
        {
            if(data) *data = hashTable[currIndex].data;
            key = hashTable[currIndex].key;
            break;
        }
        currIndex--;
    }

    return key;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template <class T>
Dictionary<T>& Dictionary<T>::operator=(const Dictionary& other)
{
    /* Clear Existing Dictionary */
    clear();
    delete [] hashTable;

    /* Copy Other Dictionary */
    hashSize = other.hashSize;
    hashLoad = other.hashLoad;
    hashTable = new hash_node_t [hashSize];
    for(unsigned int i = 0; i < hashSize; i++)
    {
        hashTable[i].chain = EMPTY_ENTRY;
        if(other.hashTable[i].chain != EMPTY_ENTRY)
        {
            /* copy fields */
            hashTable[i].data = other.hashTable[i].data;
            hashTable[i].chain = other.hashTable[i].chain;
            hashTable[i].hash = other.hashTable[i].hash;
            hashTable[i].next = other.hashTable[i].next;
            hashTable[i].prev = other.hashTable[i].prev;

            /* copy key */
            char* new_key = NULL;
            OsApi::dupstr(&new_key, other.hashTable[i].key);
            hashTable[i].key = new_key;
        }
    }
    currIndex = 0;
    numEntries = other.numEntries;
    maxChain = other.maxChain;

    /* return */
    return *this;
}

/*----------------------------------------------------------------------------
 * []
 *
 *  indexed by key
 *----------------------------------------------------------------------------*/
template <class T>
T& Dictionary<T>::operator[](const char* key) const
{
    return get(key);
}

/*----------------------------------------------------------------------------
 * hashKey
 *----------------------------------------------------------------------------*/
template <class T>
unsigned int Dictionary<T>::hashKey(const char *key) const
{
    const char* ptr = key;
    int         h   = 0;

    while(*ptr != '\0')
    {
        h += *ptr;
        h += ( h << 10 );
        h ^= ( h >> 6 );
        ptr++;
    }

    h += ( h << 3 );
    h ^= ( h >> 11 );
    h += ( h << 15 );

    return (unsigned int)h;
}

/*----------------------------------------------------------------------------
 * getNode
 *
 *  must be called from locked context
 *----------------------------------------------------------------------------*/
template <class T>
unsigned int Dictionary<T>::getNode(const char* key) const
{
    /* Check Pointer */
    if(key != NULL)
    {
        /* Grab Hash Entry */
        unsigned int index = hashKey(key) % hashSize;

        /* Search */
        while(index != NULL_INDEX && hashTable[index].chain != EMPTY_ENTRY)
        {
            /* Compare Hash Key to Key */
            for(int i = 0; i < MAX_STR_SIZE; i++)
            {
                if(hashTable[index].key[i] != key[i])
                {
                    /* As soon as there is a difference, go to next */
                    index = hashTable[index].next;
                    break;
                }
                else if(key[i] == '\0')
                {
                    /* If there is no difference AND key is at null, return match */
                    return index;
                }
            }
        }
    }

    return NULL_INDEX;
}

/*----------------------------------------------------------------------------
 * addNode
 *----------------------------------------------------------------------------*/
template <class T>
void Dictionary<T>::addNode (const char* key, T& data, unsigned int hash, bool rehashed)
{
    /* Constrain the Hash */
    unsigned int curr_index = hash % hashSize;

    /* Optimize Creation of New Key */
    const char* new_key = NULL;
    if(rehashed)
    {
        new_key = key;
    }
    else
    {
        char* tmp_key = NULL;
        OsApi::dupstr(&tmp_key, key);
        new_key = tmp_key;
    }

    /* Add Entry Directly to Hash */
    if(hashTable[curr_index].chain == EMPTY_ENTRY)
    {
        hashTable[curr_index].key   = new_key;
        hashTable[curr_index].data  = data;
        hashTable[curr_index].chain = 1;
        hashTable[curr_index].hash  = hash;
        hashTable[curr_index].next  = NULL_INDEX;
        hashTable[curr_index].prev  = NULL_INDEX;
    }
    /* Add Entry Linked into Hash */
    else
    {
        /* Find First Open Hash Slot */
        unsigned int open_index = (curr_index + 1) % hashSize;
        while(hashTable[open_index].chain != EMPTY_ENTRY)
        {
            assert(open_index != curr_index); // previous checks prevent this
            open_index = (open_index + 1) % hashSize;
        }

        /* Get Indices into List */
        unsigned int next_index = hashTable[curr_index].next;
        unsigned int prev_index = hashTable[curr_index].prev;

        if(hashTable[curr_index].chain == 1) // collision
        {
            /* Transverse to End of Chain */
            prev_index = curr_index;
            while(next_index != NULL_INDEX)
            {
                prev_index = next_index;
                next_index = hashTable[next_index].next;
            }

            /* Add Entry to Open Slot at End of Chain */
            hashTable[prev_index].next  = open_index;
            hashTable[open_index].key   = new_key;
            hashTable[open_index].data  = data;
            hashTable[open_index].chain = hashTable[prev_index].chain + 1;
            hashTable[open_index].hash  = hash;
            hashTable[open_index].next  = NULL_INDEX;
            hashTable[open_index].prev  = prev_index;

            /* Check For New Max Chain */
            if(hashTable[open_index].chain > maxChain)
            {
                maxChain = hashTable[open_index].chain;
            }
        }
        else // chain > 1, robin hood the slot
        {
            /* Bridge Over Current Slot */
            if(next_index != NULL_INDEX) hashTable[next_index].prev = prev_index;
            hashTable[prev_index].next = next_index;

            /* Transverse to End of Chain */
            while(next_index != NULL_INDEX)
            {
                hashTable[next_index].chain--;
                prev_index = next_index;
                next_index = hashTable[next_index].next;
            }

            /* Copy Current Slot to Open Slot at End of Chain */
            hashTable[prev_index].next  = open_index;
            hashTable[open_index].key   = hashTable[curr_index].key;
            hashTable[open_index].data  = hashTable[curr_index].data;
            hashTable[open_index].chain = hashTable[prev_index].chain + 1;
            hashTable[open_index].hash  = hashTable[curr_index].hash;
            hashTable[open_index].next  = NULL_INDEX;
            hashTable[open_index].prev  = prev_index;

            /* Check For New Max Chain */
            if(hashTable[open_index].chain > maxChain)
            {
                maxChain = hashTable[open_index].chain;
            }

            /* Add Entry to Current Slot */
            hashTable[curr_index].key   = new_key;
            hashTable[curr_index].data  = data;
            hashTable[curr_index].chain = 1;
            hashTable[curr_index].hash  = hash;
            hashTable[curr_index].next  = NULL_INDEX;
            hashTable[curr_index].prev  = NULL_INDEX;
        }
    }
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T>
void Dictionary<T>::freeNode(unsigned int hash_index)
{
    (void)hash_index;
}

/******************************************************************************
 * MANAGED DICTIONARY METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, bool is_array>
MgDictionary<T, is_array>::MgDictionary(int hash_size, double hash_load): Dictionary<T>(hash_size, hash_load)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T, bool is_array>
MgDictionary<T, is_array>::~MgDictionary(void)
{
    /* This call is needed here because the data in the hash table
     * must be cleared in the context of the freeNode method below
     * which belongs to MgDictionary.  The Dictionary destructor
     * will clear the hash table using its own freeNode method which
     * does not call the destructor for the objects. */
    Dictionary<T>::clear();
}

/*----------------------------------------------------------------------------
 * freeNode
 *----------------------------------------------------------------------------*/
template <class T, bool is_array>
void MgDictionary<T, is_array>::freeNode(unsigned int hash_index)
{
    if(!is_array)   delete Dictionary<T>::hashTable[hash_index].data;
    else            delete [] Dictionary<T>::hashTable[hash_index].data;
}

#endif  /* __dictionary__ */
