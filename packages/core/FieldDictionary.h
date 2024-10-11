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
        } entry_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        FieldDictionary (std::initializer_list<entry_t> init_list, int hash_table_size=DEFAULT_INITIAL_HASH_TABLE_SIZE);
        explicit        FieldDictionary (int hash_table_size=DEFAULT_INITIAL_HASH_TABLE_SIZE);
//                        FieldDictionary (const FieldDictionary& dictionary);
        virtual         ~FieldDictionary(void) override = default;

        bool            add             (const entry_t& entry);

//        FieldDictionary& operator=      (const FieldDictionary& dictionary);
        Field*          operator[]      (const char* key) const;
        Field&          operator[]      (const char* key);

        string          toJson          (void) const override;
        int             toLua           (lua_State* L) const override;
        int             toLua           (lua_State* L, const string& key) const override;
        void            fromLua         (lua_State* L, int index) override;

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
