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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaEngine.h"
#include "FieldDictionary.h"

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
FieldDictionary::FieldDictionary(std::initializer_list<entry_t> init_list, int hash_table_size):
    Field(DICTIONARY, 0),
    fields(hash_table_size)
{
    for(const entry_t elem: init_list)
    {
        fields.add(elem.name, elem);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
FieldDictionary::FieldDictionary(int hash_table_size):
    Field(DICTIONARY, 0),
    fields(hash_table_size)
{
}

/*----------------------------------------------------------------------------
 * Copy Constructor
 *----------------------------------------------------------------------------*/
//FieldDictionary::FieldDictionary(const FieldDictionary& dictionary):
//    Field(DICTIONARY, 0),
//    fields(dictionary.fields)
//{
//printf("\n\n\nTHIS IS PROBABLY A BUG\n\n\n");
//}

/*----------------------------------------------------------------------------
 * add
 *----------------------------------------------------------------------------*/
bool FieldDictionary::add(const entry_t& entry)
{
    return fields.add(entry.name, entry);
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
//FieldDictionary& FieldDictionary::operator= (const FieldDictionary& dictionary)
//{
//    if(this == &dictionary) return *this;
//printf("\n\n\nI MEAN IT, IT IS PROBABLY A BUG\n\n\n");
//    fields = dictionary.fields;
//    return *this;
//}

/*----------------------------------------------------------------------------
 * operator[] - rvalue
 *----------------------------------------------------------------------------*/
Field* FieldDictionary::operator[](const char* key) const
{
    return fields[key].field;
}

/*----------------------------------------------------------------------------
 * operator[] - lvalue
 *----------------------------------------------------------------------------*/
Field& FieldDictionary::operator[](const char* key)
{
    return *(fields[key].field);
}

/*----------------------------------------------------------------------------
 * toJson
 *----------------------------------------------------------------------------*/
string FieldDictionary::toJson (void) const
{
    Dictionary<entry_t>::Iterator iter(fields);
    string str("{");
    for(int i = 0; i < iter.length; i++)
    {
        const Dictionary<entry_t>::kv_t kv = iter[i];
        str += "\"";
        str += kv.value.name;
        str += "\":";
        str += kv.value.field->toJson();
        if(i < iter.length - 1) str += ",";
    }
    str += "}";
    return str;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
int FieldDictionary::toLua (lua_State* L) const
{
    Dictionary<entry_t>::Iterator iter(fields);
    lua_newtable(L);
    for(int i = 0; i < iter.length; i++)
    {
        const Dictionary<entry_t>::kv_t kv = iter[i];
        lua_pushstring(L, kv.value.name);
        kv.value.field->toLua(L);
        lua_settable(L, -3);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
int FieldDictionary::toLua (lua_State* L, const string& key) const
{
    try
    {
        const entry_t& entry = fields[key.c_str()];
        entry.field->toLua(L);
    }
    catch(const RunTimeException& e)
    {
        (void)e;
        lua_pushnil(L);
    }
    return 1;
}


/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void FieldDictionary::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        Dictionary<entry_t>::Iterator iter(fields);
        for(int i = 0; i < iter.length; i++)
        {
            Dictionary<entry_t>::kv_t kv = iter[i];
            lua_getfield(L, index, kv.value.name);
            try
            {
                kv.value.field->fromLua(L, -1);
            }
            catch (const RunTimeException& e)
            {
                if(!lua_isnil(L, -1))
                {
                    mlog(ERROR, "Field <%s> using default value: %s", kv.value.name, e.what());
                }
            }
            lua_pop(L, 1);
        }
    }
}
