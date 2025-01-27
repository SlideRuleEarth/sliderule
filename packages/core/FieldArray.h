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

#ifndef __field_array__
#define __field_array__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaEngine.h"
#include "Field.h"

/******************************************************************************
 * CLASSES
 ******************************************************************************/

template <class T>
struct FieldUnsafeArray: public Field
{
    FieldUnsafeArray (T* _memPtr, int _size):
        Field(ARRAY, getImpliedEncoding<T>()),
        memPtr(_memPtr),
        size(_size) {};

    ~FieldUnsafeArray (void) = default;

    virtual long length (void) const override {
        return size;
    }

    virtual const Field* get (long i) const override {
        return reinterpret_cast<const Field*>(&memPtr[i]);
    }

    const T* memPtr;
    const int size;
};

template <class T, int N>
class FieldArray: public FieldUnsafeArray<T>
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            FieldArray      (std::initializer_list<T> init_list);
                            FieldArray      (void);
                            FieldArray      (const FieldArray<T,N>& array);
        virtual             ~FieldArray     (void) override = default;

        FieldArray<T,N>&    operator=       (const FieldArray<T,N>& array);
        FieldArray<T,N>&    operator=       (std::initializer_list<T> init_list);
        T                   operator[]      (int i) const;
        T&                  operator[]      (int i);

        string              toJson          (void) const override;
        int                 toLua           (lua_State* L) const override;
        int                 toLua           (lua_State* L, long key) const override;
        void                fromLua         (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Inlines
         *--------------------------------------------------------------------*/

        long length (void) const override {
            return N;
        }

        const Field* get (long i) const override {
            return reinterpret_cast<const Field*>(&values[i]);
        }

        long serialize (uint8_t* buffer, size_t _size) const override {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&values[0]);
            const size_t bytes_to_copy = MIN(_size, sizeof(T)*N);
            memcpy(buffer, ptr, bytes_to_copy);
            return bytes_to_copy;
        }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        T values[N];

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void copy (const FieldArray<T,N>& array);
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

template <class T, int N>
inline string convertToJson(const FieldArray<T, N>& v) {
    return v.toJson();
}

template <class T, int N>
inline int convertToLua(lua_State* L, const FieldArray<T, N>& v) {
    return v.toLua(L);
}

template <class T, int N>
inline void convertFromLua(lua_State* L, int index, FieldArray<T, N>& v) {
    v.fromLua(L, index);
}

template <int N> inline uint32_t toEncoding(FieldArray<bool, N>& v)     { (void)v; return Field::NESTED_ARRAY | Field::BOOL;   };
template <int N> inline uint32_t toEncoding(FieldArray<int8_t, N>& v)   { (void)v; return Field::NESTED_ARRAY | Field::INT8;   };
template <int N> inline uint32_t toEncoding(FieldArray<int16_t, N>& v)  { (void)v; return Field::NESTED_ARRAY | Field::INT16;  };
template <int N> inline uint32_t toEncoding(FieldArray<int32_t, N>& v)  { (void)v; return Field::NESTED_ARRAY | Field::INT32;  };
template <int N> inline uint32_t toEncoding(FieldArray<int64_t, N>& v)  { (void)v; return Field::NESTED_ARRAY | Field::INT64;  };
template <int N> inline uint32_t toEncoding(FieldArray<uint8_t, N>& v)  { (void)v; return Field::NESTED_ARRAY | Field::UINT8;  };
template <int N> inline uint32_t toEncoding(FieldArray<uint16_t, N>& v) { (void)v; return Field::NESTED_ARRAY | Field::UINT16; };
template <int N> inline uint32_t toEncoding(FieldArray<uint32_t, N>& v) { (void)v; return Field::NESTED_ARRAY | Field::UINT32; };
template <int N> inline uint32_t toEncoding(FieldArray<uint64_t, N>& v) { (void)v; return Field::NESTED_ARRAY | Field::UINT64; };
template <int N> inline uint32_t toEncoding(FieldArray<float, N>& v)    { (void)v; return Field::NESTED_ARRAY | Field::FLOAT;  };
template <int N> inline uint32_t toEncoding(FieldArray<double, N>& v)   { (void)v; return Field::NESTED_ARRAY | Field::DOUBLE; };
template <int N> inline uint32_t toEncoding(FieldArray<time8_t, N>& v)  { (void)v; return Field::NESTED_ARRAY | Field::TIME8;  };
template <int N> inline uint32_t toEncoding(FieldArray<string, N>& v)   { (void)v; return Field::NESTED_ARRAY | Field::STRING; };

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>::FieldArray(std::initializer_list<T> init_list):
    FieldUnsafeArray<T>(&values[0], N)
{
    std::copy(init_list.begin(), init_list.end(), values);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>::FieldArray(void):
    FieldUnsafeArray<T>(&values[0], N)
{
}

/*----------------------------------------------------------------------------
 * Copy Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>::FieldArray(const FieldArray<T,N>& array):
    FieldUnsafeArray<T>(&values[0], N)
{
    copy(array);
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>& FieldArray<T,N>::operator=(const FieldArray<T,N>& array)
{
    if(this == &array) return *this;
    copy(array);
    return *this;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>& FieldArray<T,N>::operator=(std::initializer_list<T> init_list)
{
    assert(N == init_list.size());
    std::copy(init_list.begin(), init_list.end(), values);
    return *this;
}

/*----------------------------------------------------------------------------
 * operator[] - rvalue
 *----------------------------------------------------------------------------*/
template <class T, int N>
T FieldArray<T,N>::operator[](int i) const
{
    return values[i];
}

/*----------------------------------------------------------------------------
 * operator[] - lvalue
 *----------------------------------------------------------------------------*/
template <class T, int N>
T& FieldArray<T,N>::operator[](int i)
{
    return values[i];
}

/*----------------------------------------------------------------------------
 * toJson
 *----------------------------------------------------------------------------*/
template <class T, int N>
string FieldArray<T,N>::toJson (void) const
{
    string str("[");
    for(int i = 0; i < N; i++)
    {
        str += convertToJson(values[i]);
        if(i < N - 1) str += ",";
    }
    str += "]";
    return str;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template <class T, int N>
int FieldArray<T,N>::toLua (lua_State* L) const
{
    lua_newtable(L);
    for(int i = 0; i < N; i++)
    {
        convertToLua(L, values[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template <class T, int N>
int FieldArray<T,N>::toLua (lua_State* L, long key) const
{
    if(key >= 0 && key < N)
    {
        convertToLua(L, values[key]);
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
template <class T, int N>
void FieldArray<T,N>::fromLua (lua_State* L, int index)
{
    const int num_elements = lua_rawlen(L, index);

    // check size
    if(num_elements != N)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "mismatch in array size, expected %d, got %d", N, num_elements);
    }

    // convert all elements from lua
    for(int i = 0; i < N; i++)
    {
        lua_rawgeti(L, index, i + 1);
        convertFromLua(L, -1, values[i]);
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * copy
 *----------------------------------------------------------------------------*/
template <class T, int N>
void FieldArray<T,N>::copy(const FieldArray<T,N>& array)
{
    assert(N > 0);
    for(int i = 0; i < N; i++)
    {
        values[i] = array.values[i];
    }
    this->type = array.type;
    this->encoding = array.encoding;
}

#endif  /* __field_array__ */
