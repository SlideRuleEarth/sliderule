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

#ifndef __field_dictionary__
#define __field_dictionary__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaEngine.h"
#include "Field.h"
#include "EventLib.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class FieldDictionary: public Field
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int DEFAULT_INITIAL_HASH_TABLE_SIZE = 32;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            const char* name;
            Field* field;
        } init_entry_t;

        typedef struct {
            Field* field;
            bool free_on_delete;
        } entry_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        FieldDictionary (std::initializer_list<init_entry_t> init_list, int hash_table_size=DEFAULT_INITIAL_HASH_TABLE_SIZE):
            Field(DICTIONARY, 0),
            fields(hash_table_size) {
            for(const init_entry_t& elem: init_list)
            {
                entry_t entry = {elem.field, false};
                fields.add(elem.name, entry);
            }
        };

        explicit FieldDictionary (int hash_table_size=DEFAULT_INITIAL_HASH_TABLE_SIZE):
            Field(DICTIONARY, 0),
            fields(hash_table_size) {};

        virtual ~FieldDictionary(void) override {
            FieldDictionary::clear();
        };

        bool add (const char* name, Field* field, bool free_on_delete) {
            entry_t entry = {field, free_on_delete};
            return fields.add(name, entry);
        }

        bool remove (const char* name) {
            entry_t entry;
            if(fields.find(name, &entry))
            {
                if(fields.remove(name))
                {
                    if(entry.free_on_delete)
                    {
                        delete entry.field;
                    }
                    else
                    {
                        // frees memory earlier than destructor
                        entry.field->clear();
                    }
                    return true;
                }
                return false;
            }
            return false;
        }

        Field* operator[] (const char* key) const {
            return fields[key].field;
        }

        Field& operator[] (const char* key) {
            return *(fields[key].field);
        }

        string toJson (void) const override {
            Dictionary<entry_t>::Iterator iter(fields);
            string str("{");
            for(int i = 0; i < iter.length; i++)
            {
                const Dictionary<entry_t>::kv_t kv = iter[i];
                str += "\"";
                str += kv.key;
                str += "\":";
                str += kv.value.field->toJson();
                if(i < iter.length - 1) str += ",";
            }
            str += "}";
            return str;
        }

        int toLua (lua_State* L) const override {
            Dictionary<entry_t>::Iterator iter(fields);
            lua_newtable(L);
            for(int i = 0; i < iter.length; i++)
            {
                const Dictionary<entry_t>::kv_t kv = iter[i];
                lua_pushstring(L, kv.key);
                kv.value.field->toLua(L);
                lua_settable(L, -3);
            }
            return 1;
        }

        int toLua (lua_State* L, const string& key) const override {
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

        void fromLua (lua_State* L, int index) override {
            if(lua_istable(L, index))
            {
                Dictionary<entry_t>::Iterator iter(fields);
                for(int i = 0; i < iter.length; i++)
                {
                    Dictionary<entry_t>::kv_t kv = iter[i];
                    lua_getfield(L, index, kv.key);
                    try
                    {
                        kv.value.field->fromLua(L, -1);
                    }
                    catch (const RunTimeException& e)
                    {
                        if(!lua_isnil(L, -1))
                        {
                            mlog(WARNING, "Field <%s> using default value: %s", kv.key, e.what());
                        }
                    }
                    lua_pop(L, 1);
                }
            }
        }

        void clear (void) override {
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
            fields.clear();
        }

        long length (void) const override {
            return fields.length();
        }

        const Field* get (long i) const override {
            Dictionary<entry_t>::Iterator iter(fields);
            return iter[i].value.field;
        }

        long serialize (uint8_t* buffer, size_t size) const override {
            (void)buffer;
            (void)size;
            return 0;
        }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Dictionary<entry_t> fields;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

inline string convertToJson(const FieldDictionary& v) {
    return v.toJson();
}

inline int convertToLua(lua_State* L, const FieldDictionary& v) {
    return v.toLua(L);
}

inline void convertFromLua(lua_State* L, int index, FieldDictionary& v) {
    v.fromLua(L, index);
}

inline uint32_t toEncoding(FieldDictionary& v) { (void)v; return Field::USER; }

#endif  /* __field_dictionary__ */
