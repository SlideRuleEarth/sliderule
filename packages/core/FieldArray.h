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
 * CLASS
 ******************************************************************************/

template <class T, int N>
class FieldArray: public Field
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            FieldArray      (std::initializer_list<T> init_list);
                            FieldArray      (void);
                            FieldArray      (const FieldArray<T,N>& array);
                            ~FieldArray     (void) override = default;

        FieldArray<T,N>&    operator=       (const FieldArray<T,N>& array);
        FieldArray<T,N>&    operator=       (std::initializer_list<T> init_list);
        T                   operator[]      (int i) const;
        T&                  operator[]      (int i);

        int                 toLua           (lua_State* L) const override;
        void                fromLua         (lua_State* L, int index) override;

        int                 toLua           (lua_State* L, long key) const override;

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
inline int convertToLua(lua_State* L, const FieldArray<T, N>& v) {
    return v.toLua(L);
}

template <class T, int N>
inline void convertFromLua(lua_State* L, int index, FieldArray<T, N>& v) {
    v.fromLua(L, index);
}

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>::FieldArray(std::initializer_list<T> init_list):
    Field(ARRAY, getImpliedEncoding<T>())
{
    assert(N == init_list.size());
    std::copy(init_list.begin(), init_list.end(), values);
    initialized = true;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>::FieldArray(void):
    Field(ARRAY, getImpliedEncoding<T>())
{
    assert(N > 0);
}

/*----------------------------------------------------------------------------
 * Copy Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>::FieldArray(const FieldArray<T,N>& array):
    Field(ARRAY, getImpliedEncoding<T>())
{
    assert(N > 0);
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
    initialized = true;
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

    // set provided
    provided = true;
    initialized = true;
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
    type = array.type;
    encoding = array.encoding;
    provided = array.provided;
    initialized = array.initialized;
}

#endif  /* __field_array__ */
