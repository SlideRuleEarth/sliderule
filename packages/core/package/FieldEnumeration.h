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

#ifndef __field_enumeration__
#define __field_enumeration__

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
class FieldEnumeration: public Field
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                                FieldEnumeration    (std::initializer_list<bool> init_list);
                                FieldEnumeration    (void);
                                FieldEnumeration    (const FieldEnumeration<T,N>& array);
        virtual                 ~FieldEnumeration   (void) override = default;

        bool                    enabled             (int i) const;
        bool                    anyEnabled          (void) const;

        FieldEnumeration<T,N>&  operator=           (const FieldEnumeration<T,N>& array);
        bool                    operator[]          (T i) const;
        bool&                   operator[]          (T i);

        string                  toJson              (void) const override;
        int                     toLua               (lua_State* L) const override;
        int                     toLua               (lua_State* L, long key) const override;
        void                    fromLua             (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Inlines
         *--------------------------------------------------------------------*/

        long length (void) const override {
            return N;
        }

        const Field* get (long i) const override {
            (void)i;
            return this;
        }

        long serialize (uint8_t* buffer, size_t size) const override {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&values[0]);
            const size_t bytes_to_copy = MIN(size, sizeof(bool) * N);
            memcpy(buffer, ptr, bytes_to_copy);
            return bytes_to_copy;
        }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool values[N];
        bool providedAsSingle;  // provided as a single value as opposed to an array

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void copy (const FieldEnumeration<T,N>& array);
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

template <class T, int N>
inline string convertToJson(const FieldEnumeration<T, N>& v) {
    return v.toJson();
}

template <class T, int N>
inline int convertToLua(lua_State* L, const FieldEnumeration<T, N>& v) {
    return v.toLua(L);
}

template <class T, int N>
inline void convertFromLua(lua_State* L, int index, FieldEnumeration<T, N>& v) {
    v.fromLua(L, index);
}

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldEnumeration<T,N>::FieldEnumeration(std::initializer_list<bool> init_list):
    Field(ENUMERATION, getImpliedEncoding<T>()),
    providedAsSingle(false)
{
    assert(N > 0);
    std::copy(init_list.begin(), init_list.end(), values);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldEnumeration<T,N>::FieldEnumeration(void):
    Field(ENUMERATION, getImpliedEncoding<T>()),
    providedAsSingle(false)
{
    assert(N > 0);
}

/*----------------------------------------------------------------------------
 * Copy Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldEnumeration<T,N>::FieldEnumeration(const FieldEnumeration<T,N>& array)
{
    assert(N > 0);
    copy(array);
}

/*----------------------------------------------------------------------------
 * enabled
 *----------------------------------------------------------------------------*/
template <class T, int N>
bool FieldEnumeration<T,N>::enabled(int i) const
{
    if(i < 0 || i >= N)
    {
        return false;
    }
    return values[i];
}

/*----------------------------------------------------------------------------
 * anyEnabled
 *----------------------------------------------------------------------------*/
template <class T, int N>
bool FieldEnumeration<T,N>::anyEnabled(void) const
{
    bool status = false;
    for(int i = 0; i < N; i++)
    {
        status = status || values[i];
    }
    return status;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldEnumeration<T,N>& FieldEnumeration<T,N>::operator=(const FieldEnumeration<T,N>& array)
{
    if(this == &array) return *this;
    copy(array);
    return *this;
}

/*----------------------------------------------------------------------------
 * operator[] - rvalue
 *----------------------------------------------------------------------------*/
template <class T, int N>
bool FieldEnumeration<T,N>::operator[](T i) const
{
    const int index = convertToIndex(i);
    if(index < 0 || index >= N)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "index out of bounds: %d", index);
    }
    return values[index];
}

/*----------------------------------------------------------------------------
 * operator[] - lvalue
 *----------------------------------------------------------------------------*/
template <class T, int N>
bool& FieldEnumeration<T,N>::operator[](T i)
{
    const int index = convertToIndex(i);
    if(index < 0 || index >= N)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "index out of bounds: %d", index);
    }
    return values[index];
}

/*----------------------------------------------------------------------------
 * toJson
 *----------------------------------------------------------------------------*/
template <class T, int N>
string FieldEnumeration<T,N>::toJson (void) const
{
    string str("[");
    bool first = true;
    for(int i = 0; i < N; i++)
    {
        if(values[i])
        {
            T selection;
            if(!first) str += ",";
            else first = false;
            convertFromIndex(i, selection);
            str += convertToJson(selection);
        }
    }
    str += "]";
    return str;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template <class T, int N>
int FieldEnumeration<T,N>::toLua (lua_State* L) const
{
    int table_index = 1;
    lua_newtable(L);
    for(int i = 0; i < N; i++)
    {
        if(values[i])
        {
            T selection;
            convertFromIndex(i, selection);
            convertToLua(L, selection);
            lua_rawseti(L, -2, table_index++);
        }
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template <class T, int N>
int FieldEnumeration<T,N>::toLua (lua_State* L, long key) const
{
    const T selection = static_cast<T>(key);
    const int index = convertToIndex(selection);
    if(index >= 0 && index < N)
    {
        if(values[index])
        {
            convertToLua(L, selection);
        }
        else
        {
            lua_pushnil(L);
        }
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
void FieldEnumeration<T,N>::fromLua (lua_State* L, int index)
{
    T selection;

    if(lua_istable(L, index)) // provided as a table
    {
        memset(values, 0, sizeof(values)); // set all to false
        const int num_elements = lua_rawlen(L, index);
        for(int i = 0; i < num_elements; i++)
        {
            lua_rawgeti(L, index, i + 1);
            convertFromLua(L, -1, selection);
            lua_pop(L, 1);

            const int selection_index = convertToIndex(selection);
            if(selection_index >= 0 && selection_index < N)
            {
                values[selection_index] = true;
            }
            else
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "selection outside of bounds: %d", selection);
            }
        }

        providedAsSingle = false;
    }
    else if(!lua_isnil(L, index))// provided as a single selection
    {
        convertFromLua(L, index, selection); // throws on error
        memset(values, 0, sizeof(values)); // set all to false
        const int selection_index = convertToIndex(selection);
        if(selection_index >= 0 && selection_index < N)
        {
            values[selection_index] = true;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "selection outside of bounds: %d", selection);
        }

        providedAsSingle = true;
    }
}

/*----------------------------------------------------------------------------
 * copy
 *----------------------------------------------------------------------------*/
template <class T, int N>
void FieldEnumeration<T,N>::copy(const FieldEnumeration<T,N>& array)
{
    assert(N > 0);
    for(int i = 0; i < N; i++)
    {
        values[i] = array.values[i];
    }
    providedAsSingle = array.providedAsSingle;
    encoding = array.encoding;
}

#endif  /* __field_enumeration__ */
