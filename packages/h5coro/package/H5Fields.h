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

#ifndef __h5_fields__
#define __h5_fields__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "H5CoroLib.h"
#include "LuaEngine.h"
#include "FieldElement.h"
#include "RequestFields.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class H5Fields: public RequestFields
{
    public:

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        FieldElement<long>      col {0};
        FieldElement<long>      startRow {0};
        FieldElement<long>      numRows {H5Coro::ALL_ROWS};
        FieldElement<string>    crs;
        FieldElement<string>    index_column;
        FieldElement<string>    time_column;
        FieldElement<string>    x_column;
        FieldElement<string>    y_column;
        FieldElement<string>    z_column;
        FieldList<string>       groups;
        FieldList<string>       variables;

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        static int luaCreate (lua_State* L);
        static const char* defaultCRS (void);

    protected:

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        H5Fields (lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const std::initializer_list<FieldMap<Field>::init_entry_t>& init_list);
        virtual ~H5Fields (void) override = default;
};

#endif  /* __h5_fields__ */
