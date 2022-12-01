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

#ifndef __arrow_buider__
#define __arrow_buider__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <arrow/api.h>

#include "MsgQ.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DispatchObject.h"
#include "OsApi.h"
#include "MsgQ.h"


/******************************************************************************
 * ATL06 DISPATCH CLASS
 ******************************************************************************/

class ArrowBuilder: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int LIST_BLOCK_SIZE = 32;

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);
        static void deinit      (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef List<RecordObject::field_t, LIST_BLOCK_SIZE> field_list_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        field_list_t                    fieldList;
        std::shared_ptr<arrow::Schema>  schema;
        Publisher*                      outQ;
        int                             rowSizeBytes;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            ArrowBuilder             (lua_State* L, const char* outq_name, const char* rec_type);
                            ~ArrowBuilder            (void);

        bool                processRecord           (RecordObject* record, okey_t key) override;
        bool                processTimeout          (void) override;
        bool                processTermination      (void) override;

        static bool         defineTableSchema       (std::shared_ptr<arrow::Schema>& _schema, field_list_t& field_list, const char* rec_type);
        static bool         addFieldsToSchema       (std::vector<std::shared_ptr<arrow::Field>>& schema_vector, field_list_t& field_list, const char* rectype);
};

#endif  /* __arrow_buider__ */
