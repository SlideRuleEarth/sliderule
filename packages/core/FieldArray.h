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
                        ~FieldArray     (void) override = default;

        T               operator[]      (int i) const;
        T&              operator[]      (int i);

        string          toJson          (void) override;
        int             toLua           (lua_State* L) override;
        void            fromJson        (const string& str) override;
        void            fromLua         (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        T values[N];
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>::FieldArray(std::initializer_list<T> init_list):
    Field(Field::INVALID)
{
    assert(N > 0);
    std::copy(init_list.begin(), init_list.end(), values);
    encoding = getFieldEncoding(values[0]);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T, int N>
FieldArray<T,N>::FieldArray(void):
    Field(Field::INVALID)
{
    assert(N > 0);
    for(int i = 0; i < N; i++)
    {
        values[i] = 0;
    }
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
string FieldArray<T,N>::toJson (void) 
{
    string str("[");
    for(int i = 0; i < N-1; i++)
    {
        str += convertToJson(values[i]);
        str += ",";
    }
    str += convertToJson(values[N-1]);
    str += "]";
    return str;
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template <class T, int N>
int FieldArray<T,N>::toLua (lua_State* L) 
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
 * fromJson
 *----------------------------------------------------------------------------*/
template <class T, int N>
void FieldArray<T,N>::fromJson (const string& str) 
{
    bool in_error = false;

    // build list of element tokens
    const char* old_txt[2] = {"[", "]"};
    const char* new_txt[2] = {"", ""};
    char* list_str = StringLib::replace(str.c_str(), old_txt, new_txt, 2);
    List<string*>* tokens = StringLib::split(list_str, StringLib::size(list_str), ',');
    
    try
    {
        // check size
        if(tokens->length() != N)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "mismatch in array size, expected %d, got %d", N, tokens->length());
        }

        // convert all elements in array
        for(int i = 0; i < N; i++)
        {
            convertFromJson(*tokens->get(i), values[i]);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "error parsing json array: %s", e.what());
        in_error = true;
    }
    catch(const std::runtime_error& e)
    {
        mlog(CRITICAL, "error parsing json array: %s", e.what());
        in_error = true;
    }

    // clean up memory
    delete [] list_str;
    delete tokens;
    
    // throw exception on any errors
    if(in_error) throw RunTimeException(CRITICAL, RTE_ERROR, "json array parse error");
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

#endif  /* __field_array__ */
