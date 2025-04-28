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
#include "Dictionary.h"
#include "Field.h"
#include "FieldColumn.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

 template <class T>
 class FieldMap: public Field
{
    public:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            const char* name;
            T* field;
        } init_entry_t;

        typedef struct {
            T* field;
            bool free_on_delete;
        } entry_t;

         /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        FieldMap    (void);
                        FieldMap    (const FieldMap<T>& other);
                        FieldMap    (std::initializer_list<init_entry_t> init_list);
        virtual         ~FieldMap   (void) override;

        long            add         (const char* key, T* field, bool free_on_delete=true);

        void            clear       (void) override;
        long            length      (void) const override;

        FieldMap<T>&    operator=   (const FieldMap<T>& other);
        const T&        operator[]  (const char* key) const;

        string          toJson      (void) const override;
        int             toLua       (lua_State* L) const override;
        void            fromLua     (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Dictionary<entry_t> fields;
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
FieldMap<T>::FieldMap():
    Field(MAP, getImpliedEncoding<T>())
{
}

/*----------------------------------------------------------------------------
 * Constructor - Copy
 *----------------------------------------------------------------------------*/
template <class T>
FieldMap<T>::FieldMap(const FieldMap<T>& other):
    Field(MAP, other.encoding),
    fields(other.fields)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
FieldMap<T>::FieldMap(std::initializer_list<init_entry_t> init_list):
    Field(MAP, getImpliedEncoding<T>())
{
    for(const init_entry_t& elem: init_list)
    {
        add(elem.name, elem.field, false);
    }
};

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
template <class T>
FieldMap<T>::~FieldMap(void)
{
    FieldMap<T>::clear();
}

/*----------------------------------------------------------------------------
 * add
 *----------------------------------------------------------------------------*/
template<class T>
long FieldMap<T>::add(const char* key, T* field, bool free_on_delete)
{
    entry_t entry = {
        .field = field,
        .free_on_delete = free_on_delete
    };
    fields.add(key, entry);
    return fields.length();
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
template<class T>
void FieldMap<T>::clear(void)
{
    entry_t entry;
    const char* key = fields.first(&entry);
    while(key != NULL)
    {
        if(entry.free_on_delete)
        {
            delete entry.field;
        }
        key = fields.next(&entry);
    }
    return fields.clear();
}

/*----------------------------------------------------------------------------
 * length
 *----------------------------------------------------------------------------*/
template<class T>
long FieldMap<T>::length(void) const
{
    return fields.length();
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template<class T>
FieldMap<T>& FieldMap<T>::operator= (const FieldMap<T>& other)
{
    if(&other == this) return *this;
    fields = other.fields;
    return *this;
}

/*----------------------------------------------------------------------------
 * operator[] - rvalue
 *----------------------------------------------------------------------------*/
template <class T>
const T& FieldMap<T>::operator[](const char* key) const
{
    return *fields[key].field;
}

/*----------------------------------------------------------------------------
 * toJson
 *----------------------------------------------------------------------------*/
template <class T>
string FieldMap<T>::toJson (void) const
{
    typename Dictionary<entry_t>::Iterator iter(fields);
    string str("{");
    for(int i = 0; i < iter.length; i++)
    {
        str += "\"";
        str += iter[i].key;
        str += "\":";
        str += convertToJson(*iter[i].value.field);
        if(i < iter.length - 1) str += ",";
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
    typename Dictionary<entry_t>::Iterator iter(fields);
    lua_newtable(L);
    for(int i = 0; i < iter.length; i++)
    {
        lua_pushstring(L, iter[i].key);
        convertToLua(L, *iter[i].value.field);
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
        int table_index = index < 0 ? lua_gettop(L) + index + 1 : index;
        lua_pushnil(L);
        while(lua_next(L, table_index) != 0)
        {
            entry_t entry = {NULL, true};
            try
            {
                const char* key = LuaObject::getLuaString(L, -2);
                entry.field = new T;
                if(!fields.add(key, entry))
                {
                    throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to add entry <%s> to column fields", key);
                }
                convertFromLua(L, -1, *entry.field);
            }
            catch(const RunTimeException& e)
            {
                delete entry.field;
                mlog(ERROR, "Failed to read field: %s", e.what());
            }
            lua_pop(L, 1);
        }
    }
}

#endif  /* __field_map__ */
