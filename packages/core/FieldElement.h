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

        explicit        FieldElement    (T default_value);
                        FieldElement    (void);
                        FieldElement    (const FieldElement<T>& element);
                        ~FieldElement   (void) override = default;

        FieldElement&   operator=       (const FieldElement<T>& element);

        FieldElement&   operator=       (const T& v);
        bool            operator==      (const T& v) const;
        bool            operator!=      (const T& v) const;

        int             toLua           (lua_State* L) const override;
        void            fromLua         (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        T value;
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
template <class T>
FieldElement<T>::FieldElement(T default_value):
    Field(ELEMENT, getImpliedEncoding<T>()),
    value(default_value)
{
    initialized = true;
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
    Field(ELEMENT, getImpliedEncoding<T>())
{
    value = element.value;
    provided = element.provided;
    initialized = element.initialized;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
template <class T>
FieldElement<T>& FieldElement<T>::operator=(const FieldElement<T>& element)
{
    value = element.value;
    encoding = element.encoding;
    provided = element.provided;
    initialized = element.initialized;
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
    provided = true;
    initialized = true;
}

#endif  /* __field_element__ */
