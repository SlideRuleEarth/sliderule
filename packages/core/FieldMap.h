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

#ifndef __field_map__
#define __field_map__

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
class FieldMap: public Field
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        FieldMap    (void);
                        FieldMap    (const FieldMap<T>& field_map);
        virtual         ~FieldMap   (void) override = default;

        long            add         (const string& key, const T& v);
        long            length      (void) const override;

        FieldMap<T>&    operator=   (const FieldMap<T>& field_map);
        const T&        operator[]  (const char* key);

        string          toJson      (void) const override;
        int             toLua       (lua_State* L) const override;
        void            fromLua     (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        map<string, T> values;
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
FieldMap<T>::FieldMap():
    Field(LIST, getImpliedEncoding<T>())
{
}

/*----------------------------------------------------------------------------
 * Constructor - Copy
 *----------------------------------------------------------------------------*/
template <class T>
FieldMap<T>::FieldMap(const FieldMap<T>& field_map):
    Field(LIST, getImpliedEncoding<T>()),
    values(field_map.values)
{
}

/*----------------------------------------------------------------------------
 * add
 *----------------------------------------------------------------------------*/
template<class T>
long FieldMap<T>::add(const string& key, const T& v)
{
    values[key] = v;
    return static_cast<long>(values.size());
}

/*----------------------------------------------------------------------------
 * length
 *----------------------------------------------------------------------------*/
template<class T>
long FieldMap<T>::length(void) const
{
    return static_cast<long>(values.size());
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template<class T>
FieldMap<T>& FieldMap<T>::operator= (const FieldMap<T>& field_map)
{
    if(&field_map == this) return *this;
    values = field_map.values;
    return *this;
}

/*----------------------------------------------------------------------------
 * operator[] - rvalue
 *----------------------------------------------------------------------------*/
template <class T>
const T& FieldMap<T>::operator[](const char* key)
{
    return values[key];
}

/*----------------------------------------------------------------------------
 * toJson
 *----------------------------------------------------------------------------*/
template <class T>
string FieldMap<T>::toJson (void) const
{
    long cnt = 0;
    long size = length();
    string str("{");
    for(const auto& pair: values)
    {
        str += "\"";
        str += pair.first;
        str += "\":";
        str += convertToJson(pair.second);
        if(cnt < size - 1) str += ",";
        cnt++;
    }
    str += "}";
    return str;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template <class T>
int FieldMap<T>::toLua (lua_State* L) const
{
    lua_newtable(L);
    for(auto& pair: values)
    {
        lua_pushstring(L, pair.first.c_str());
        convertToLua(L, pair.second);
        lua_settable(L, -3);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
template <class T>
void FieldMap<T>::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        lua_pushnil(L);
        while(lua_next(L, index) != 0)
        {
            try
            {
                const char* key = LuaObject::getLuaString(L, -2);
                values.emplace(key, T());
                convertFromLua(L, -1, values[key]);
            }
            catch(const RunTimeException& e)
            {
                mlog(ERROR, "Failed to read field: %s", e.what());
            }
            lua_pop(L, 1);
        }
    }
}

#endif  /* __field_map__ */
