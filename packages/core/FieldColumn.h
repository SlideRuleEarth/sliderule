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

        explicit        FieldColumn     (int chunk_size=DEFAULT_CHUNK_SIZE);
                        ~FieldColumn    (void) override = default;

        int             append          (const T& v);
        T*              array           (void) const;

        T               operator[]      (int i) const;
        T&              operator[]      (int i);

        int             toLua           (lua_State* L) const override;
        void            fromLua         (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        List<T> values;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

template <class T>
inline int convertToLua(lua_State* L, const FieldColumn<T>& v) {
    return v.toLua(L);
}

template <class T>
inline void convertFromLua(lua_State* L, int index, FieldColumn<T>& v) {
    v.fromLua(L, index);
}

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template<class T>
FieldColumn<T>::FieldColumn(int chunk_size):
    values(chunk_size)
{
}

/*----------------------------------------------------------------------------
 * append
 *----------------------------------------------------------------------------*/
template<class T>
int FieldColumn<T>::append(const T& v)
{
    return values.add(v);
}

/*----------------------------------------------------------------------------
 * array
 *----------------------------------------------------------------------------*/
template<class T>
T* FieldColumn<T>::array(void) const
{
    return values.array();
}

/*----------------------------------------------------------------------------
 * operator[] - rvalue
 *----------------------------------------------------------------------------*/
template<class T>
T FieldColumn<T>::operator[](int i) const
{
    return values[i];
}

/*----------------------------------------------------------------------------
 * operator[] - lvalue
 *----------------------------------------------------------------------------*/
template<class T>
T& FieldColumn<T>::operator[](int i)
{
    return values[i];
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template<class T>
int FieldColumn<T>::toLua (lua_State* L) const
{
    const int length = values.length();
    lua_newtable(L);
    for(int i = 0; i < length; i++)
    {
        convertToLua(L, values[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
template<class T>
void FieldColumn<T>::fromLua (lua_State* L, int index) 
{
    // clear out existing values
    values.clear();

    // convert all elements from lua
    const int num_elements = lua_rawlen(L, index);
    for(int i = 0; i < num_elements; i++)
    {
        T value;
        lua_rawgeti(L, index, i + 1);
        convertFromLua(L, -1, value);
        lua_pop(L, 1);
        values.add(value);
    }
}

#endif  /* __field_column__ */
