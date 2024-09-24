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

#ifndef __field_column__
#define __field_column__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaEngine.h"
#include "Field.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

template <class T>
class FieldColumn: public Field
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DEFAULT_CHUNK_SIZE = 256;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit        FieldColumn     (uint32_t encoding_mask=0, long _chunk_size=DEFAULT_CHUNK_SIZE);
                        FieldColumn     (const FieldColumn<T>& column);
        virtual         ~FieldColumn    (void) override;

        long            append          (const T& v);
        void            initialize      (long size, const T& v);
        void            clear           (void);
        long            length          (void) const;

        FieldColumn<T>& operator=       (const FieldColumn<T>& column);
        T               operator[]      (int i) const;
        T&              operator[]      (int i);

        string          toJson          (void) const override;
        int             toLua           (lua_State* L) const override;
        int             toLua           (lua_State* L, long key) const override;
        void            fromLua         (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        vector<T*> chunks;
        long currChunk;
        long currChunkOffset;
        long numElements;
        long chunkSize;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

template <class T>
inline string convertToJson(const FieldColumn<T>& v) {
    return v.toJson();
}

template <class T>
inline int convertToLua(lua_State* L, const FieldColumn<T>& v) {
    return v.toLua(L);
}

template <class T>
inline void convertFromLua(lua_State* L, int index, FieldColumn<T>& v) {
    v.fromLua(L, index);
}

// encoding
inline uint32_t toEncoding(FieldColumn<bool>& v)     { (void)v; return Field::NESTED_COLUMN | Field::BOOL;   };
inline uint32_t toEncoding(FieldColumn<int8_t>& v)   { (void)v; return Field::NESTED_COLUMN | Field::INT8;   };
inline uint32_t toEncoding(FieldColumn<int16_t>& v)  { (void)v; return Field::NESTED_COLUMN | Field::INT16;  };
inline uint32_t toEncoding(FieldColumn<int32_t>& v)  { (void)v; return Field::NESTED_COLUMN | Field::INT32;  };
inline uint32_t toEncoding(FieldColumn<int64_t>& v)  { (void)v; return Field::NESTED_COLUMN | Field::INT64;  };
inline uint32_t toEncoding(FieldColumn<uint8_t>& v)  { (void)v; return Field::NESTED_COLUMN | Field::UINT8;  };
inline uint32_t toEncoding(FieldColumn<uint16_t>& v) { (void)v; return Field::NESTED_COLUMN | Field::UINT16; };
inline uint32_t toEncoding(FieldColumn<uint32_t>& v) { (void)v; return Field::NESTED_COLUMN | Field::UINT32; };
inline uint32_t toEncoding(FieldColumn<uint64_t>& v) { (void)v; return Field::NESTED_COLUMN | Field::UINT64; };
inline uint32_t toEncoding(FieldColumn<float>& v)    { (void)v; return Field::NESTED_COLUMN | Field::FLOAT;  };
inline uint32_t toEncoding(FieldColumn<double>& v)   { (void)v; return Field::NESTED_COLUMN | Field::DOUBLE; };
inline uint32_t toEncoding(FieldColumn<time8_t>& v)  { (void)v; return Field::NESTED_COLUMN | Field::TIME8;  };
inline uint32_t toEncoding(FieldColumn<string>& v)   { (void)v; return Field::NESTED_COLUMN | Field::STRING; };

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template<class T>
FieldColumn<T>::FieldColumn(uint32_t encoding_mask, long _chunk_size):
    Field(COLUMN, getImpliedEncoding<T>() & encoding_mask),
    currChunk(-1),
    currChunkOffset(_chunk_size),
    numElements(0),
    chunkSize(_chunk_size)
{
}

/*----------------------------------------------------------------------------
 * Copy Constructor
 *----------------------------------------------------------------------------*/
template<class T>
FieldColumn<T>::FieldColumn(const FieldColumn<T>& column):
    Field(COLUMN, getImpliedEncoding<T>()),
    currChunk(column.currChunk),
    currChunkOffset(column.currChunkOffset),
    numElements(column.numElements),
    chunkSize(column.chunkSize)
{
    // all but last chunk
    for(long c = 0; c < currChunk; c++)
    {
        T* chunk = new T[chunkSize];
        for(long i = 0; i < chunkSize; i++)
        {
            chunk[i] = column.chunks[c][i];
        }
        chunks.push_back(chunk);
    }
    // last chunk
    if(currChunk >= 0)
    {
        T* chunk = new T[chunkSize];
        for(long i = 0; i < currChunkOffset; i++)
        {
            chunk[i] = column.chunks[currChunk][i];
        }
        chunks.push_back(chunk);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template<class T>
FieldColumn<T>::~FieldColumn(void)
{
    for(T* chunk: chunks)
    {
        delete [] chunk;
    }
}

/*----------------------------------------------------------------------------
 * append
 *----------------------------------------------------------------------------*/
template<class T>
long FieldColumn<T>::append(const T& v)
{
    if(currChunkOffset < chunkSize)
    {
        chunks[currChunk][currChunkOffset] = v;
        currChunkOffset++;
    }
    else
    {
        T* chunk = new T[chunkSize];
        chunk[0] = v;
        chunks.push_back(chunk);
        currChunk++;
        currChunkOffset = 1;
    }

    return ++numElements;
}

/*----------------------------------------------------------------------------
 * initialize
 *----------------------------------------------------------------------------*/
template<class T>
void FieldColumn<T>::initialize(long size, const T& v)
{
    clear();
    chunkSize = size;
    currChunkOffset = size;
    currChunk = 0;
    T* chunk = new T[chunkSize];
    for(int i = 0; i < chunkSize; i++)
    {
        chunk[i] = v;
    }
    chunks.push_back(chunk);
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
template<class T>
void FieldColumn<T>::clear(void)
{
    // delete all chunks
    for(T* chunk: chunks)
    {
        delete [] chunk;
    }
    chunks.clear();

    // reset indices
    currChunk = -1;
    currChunkOffset = chunkSize;
    numElements = 0;
}

/*----------------------------------------------------------------------------
 * length
 *----------------------------------------------------------------------------*/
template<class T>
long FieldColumn<T>::length(void) const
{
    return numElements;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template<class T>
FieldColumn<T>& FieldColumn<T>::operator= (const FieldColumn<T>& column)
{
    if(this == &column) return *this;

    clear();
    for(long i = 0; i < column.numElements; i++)
    {
        append(column[i]);
    }

    encoding = column.encoding;

    return *this;
}

/*----------------------------------------------------------------------------
 * operator[] - rvalue
 *----------------------------------------------------------------------------*/
template<class T>
T FieldColumn<T>::operator[](int i) const
{
    const long chunk_index = i / chunkSize;
    const long chunk_offset = i % chunkSize;
    return chunks[chunk_index][chunk_offset];
}

/*----------------------------------------------------------------------------
 * operator[] - lvalue
 *----------------------------------------------------------------------------*/
template<class T>
T& FieldColumn<T>::operator[](int i)
{
    const long chunk_index = i / chunkSize;
    const long chunk_offset = i % chunkSize;
    return chunks[chunk_index][chunk_offset];
}

/*----------------------------------------------------------------------------
 * toJson
 *----------------------------------------------------------------------------*/
template <class T>
string FieldColumn<T>::toJson (void) const
{
    string str("[");
    for(int i = 0; i < numElements-1; i++)
    {
        str += convertToJson(this->operator[](i));
        str += ",";
    }
    if(numElements > 0)
    {
        str += convertToJson(this->operator[](numElements-1));
    }
    str += "]";
    return str;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template<class T>
int FieldColumn<T>::toLua (lua_State* L) const
{
    lua_newtable(L);
    for(int i = 0; i < numElements; i++)
    {
        convertToLua(L, this->operator[](i));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template <class T>
int FieldColumn<T>::toLua (lua_State* L, long key) const
{
    if(key >= 0 && key < numElements)
    {
        convertToLua(L, this->operator[](key));
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
template<class T>
void FieldColumn<T>::fromLua (lua_State* L, int index) 
{
    // clear out existing elements
    clear();

    // convert all elements from lua
    const int num_elements = lua_rawlen(L, index);
    for(int i = 0; i < num_elements; i++)
    {
        T value;
        lua_rawgeti(L, index, i + 1);
        convertFromLua(L, -1, value);
        lua_pop(L, 1);
        append(value);
    }
}

#endif  /* __field_column__ */
