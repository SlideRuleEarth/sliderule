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

#ifndef __parquet_builder__
#define __parquet_builder__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <arrow/api.h>
#include <parquet/arrow/writer.h>

#include "MsgQ.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DispatchObject.h"
#include "OsApi.h"
#include "MsgQ.h"

using std::shared_ptr;
using std::unique_ptr;
using std::vector;

/******************************************************************************
 * ATL06 DISPATCH CLASS
 ******************************************************************************/

class ParquetBuilder: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int LIST_BLOCK_SIZE = 32;
        static const int FILE_NAME_MAX_LEN = 128;
        static const int FILE_BUFFER_RSPS_SIZE = 0x1000000; // 16MB

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const char* metaRecType;
        static const RecordObject::fieldDef_t metaRecDef[];

        static const char* dataRecType;
        static const RecordObject::fieldDef_t dataRecDef[];

        static const char* TMP_FILE_PREFIX;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            char    filename[FILE_NAME_MAX_LEN];
            long    size;
        } arrow_file_meta_t;

        typedef struct {
            char    filename[FILE_NAME_MAX_LEN];
            uint8_t data[FILE_BUFFER_RSPS_SIZE];
        } arrow_file_data_t;

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
        typedef field_list_t::Iterator field_iterator_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                                    alive;
        field_list_t                            fieldList;
        field_iterator_t*                       fieldIterator;
        Mutex                                   tableMut;
        shared_ptr<arrow::Schema>               schema;
        unique_ptr<parquet::arrow::FileWriter>  parquetWriter;
        Publisher*                              outQ;
        int                                     rowSizeBytes;
        const char*                             fileName; // used locally to build file
        const char*                             outFileName; // name to send back to client

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            ParquetBuilder          (lua_State* L, const char* filename, const char* outq_name, const char* rec_type, const char* id);
                            ~ParquetBuilder         (void);

        bool                processRecord           (RecordObject* record, okey_t key) override;
        bool                processTimeout          (void) override;
        bool                processTermination      (void) override;
        bool                postRecord              (RecordObject* record, int data_size=0);

        static bool         defineTableSchema       (shared_ptr<arrow::Schema>& _schema, field_list_t& field_list, const char* rec_type);
        static bool         addFieldsToSchema       (vector<shared_ptr<arrow::Field>>& schema_vector, field_list_t& field_list, const char* rectype);
};

#endif  /* __parquet_builder__ */
