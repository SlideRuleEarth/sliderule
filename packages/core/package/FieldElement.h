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

#ifndef __field_element__
#define __field_element__

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
class FieldElement: public Field
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit        FieldElement    (const T& default_value, uint32_t encoding_mask=0);
                        FieldElement    (void);
                        FieldElement    (const FieldElement<T>& element);
        virtual         ~FieldElement   (void) override = default;

        FieldElement&   operator=       (const FieldElement<T>& element);

        FieldElement&   operator=       (const T& v);
        bool            operator==      (const T& v) const;
        bool            operator!=      (const T& v) const;

        string          toJson          (void) const override;
        int             toLua           (lua_State* L) const override;
        void            fromLua         (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Inlines
         *--------------------------------------------------------------------*/

        long length (void) const override {
            return 1;
        }

        const Field* get (long i) const override {
            (void)i;
            return this;
        }

        long serialize (uint8_t* buffer, size_t size) const override {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
            const size_t bytes_to_copy = MIN(size, sizeof(T));
            memcpy(buffer, ptr, bytes_to_copy);
            return bytes_to_copy;
        }

        operator bool() const {
            return value != 0;
        }

        operator string() const {
            return toJson();
        }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        T value;
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * length - specialization for string
 *----------------------------------------------------------------------------*/
template <>
inline long FieldElement<string>::length() const
{
    return value.size();
}

/*----------------------------------------------------------------------------
 * serialize - specialization for string
 *----------------------------------------------------------------------------*/
template <>
inline long FieldElement<string>::serialize (uint8_t* buffer, size_t size) const
{
    const size_t bytes_to_copy = MIN(size, value.size());
    memcpy(buffer, value.c_str(), bytes_to_copy);
    return bytes_to_copy;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
FieldElement<T>::FieldElement(const T& default_value, uint32_t encoding_mask):
    Field(ELEMENT, getImpliedEncoding<T>() | encoding_mask),
    value(default_value)
{
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
FieldElement<T>::FieldElement():
    Field(ELEMENT, getImpliedEncoding<T>())
{
}

/*----------------------------------------------------------------------------
 * Copy Constructor
 *----------------------------------------------------------------------------*/
template <class T>
FieldElement<T>::FieldElement(const FieldElement<T>& element):
    Field(ELEMENT, getImpliedEncoding<T>()),
    value(element.value)
{
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template <class T>
FieldElement<T>& FieldElement<T>::operator=(const FieldElement<T>& element)
{
    value = element.value;
    encoding = element.encoding;
    return *this;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template <class T>
FieldElement<T>& FieldElement<T>::operator=(const T& v)
{
    value = v;
    return *this;
}

/*----------------------------------------------------------------------------
 * operator==
 *----------------------------------------------------------------------------*/
template <class T>
bool FieldElement<T>::operator==(const T& v) const
{
    return value == v;
}

/*----------------------------------------------------------------------------
 * operator!=
 *----------------------------------------------------------------------------*/
template <class T>
bool FieldElement<T>::operator!=(const T& v) const
{
    return value != v;
}

/*----------------------------------------------------------------------------
 * toJson
 *----------------------------------------------------------------------------*/
template <class T>
string FieldElement<T>::toJson (void) const
{
    return convertToJson(value);
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
template <class T>
int FieldElement<T>::toLua (lua_State* L) const
{
    return convertToLua(L, value);
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
template <class T>
void FieldElement<T>::fromLua (lua_State* L, int index)
{
    convertFromLua(L, index, value);
}

#endif  /* __field_element__ */
