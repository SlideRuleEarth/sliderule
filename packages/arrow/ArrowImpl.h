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

#ifndef __arrow_impl__
#define __arrow_impl__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <iostream>
#include <arrow/builder.h>
#include <arrow/table.h>
#include <arrow/csv/writer.h>
#include <arrow/io/file.h>
#include <arrow/util/key_value_metadata.h>
#include <parquet/arrow/writer.h>
#include <parquet/arrow/schema.h>
#include <parquet/properties.h>
#include <parquet/file_writer.h>
#include <regex>

#include "MsgQ.h"
#include "LuaObject.h"
#include "Ordering.h"
#include "RecordObject.h"
#include "ArrowParms.h"
#include "ParquetBuilder.h"
#include "OsApi.h"
#include "MsgQ.h"

/******************************************************************************
 * PARQUET BUILDER CLASS
 ******************************************************************************/

class ArrowImpl
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef ParquetBuilder::batch_list_t batch_list_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit ArrowImpl          (ParquetBuilder* _builder);
        ~ArrowImpl                  (void);

        bool processRecordBatch     (batch_list_t& record_batch, int num_rows, 
                                     int batch_row_size_bits, bool file_finished=false);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int LIST_BLOCK_SIZE = 32;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef List<RecordObject::field_t> field_list_t;
        typedef field_list_t::Iterator field_iterator_t;
                
        typedef struct WKBPoint {
            uint8_t                 byteOrder;
            uint32_t                wkbType;
            double                  x;
            double                  y;
        } ALIGN_PACKED wkbpoint_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        ParquetBuilder*                             parquetBuilder;
        shared_ptr<arrow::Schema>                   schema;
        unique_ptr<parquet::arrow::FileWriter>      parquetWriter;
        shared_ptr<arrow::io::FileOutputStream>     csvWriter;
        ArrowParms::format_t                        writerFormat;
        vector<shared_ptr<arrow::Field>>            fieldVector;
        field_list_t                                fieldList;
        bool                                        firstTime;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        ArrowParms::format_t createSchema (void);
        
        bool buildFieldList         (const char* rec_type, int offset, int flags);
        void appendGeoMetaData      (const std::shared_ptr<arrow::KeyValueMetadata>& metadata);
        void appendServerMetaData   (const std::shared_ptr<arrow::KeyValueMetadata>& metadata);
        void appendPandasMetaData   (const std::shared_ptr<arrow::KeyValueMetadata>& metadata);
        void processField           (RecordObject::field_t& field, 
                                     shared_ptr<arrow::Array>* column, 
                                     batch_list_t& record_batch, 
                                     int num_rows, 
                                     int batch_row_size_bits);
        void processArray           (RecordObject::field_t& field, 
                                     shared_ptr<arrow::Array>* column, 
                                     batch_list_t& record_batch, 
                                     int batch_row_size_bits);
        void processGeometry        (RecordObject::field_t& x_field, 
                                     RecordObject::field_t& y_field, 
                                     shared_ptr<arrow::Array>* column, 
                                     batch_list_t& record_batch, 
                                     int num_rows, 
                                     int batch_row_size_bits);
        void processAncillaryFields  (vector<shared_ptr<arrow::Array>>& columns,
                                     batch_list_t& record_batch);
        void processAncillaryElements(vector<shared_ptr<arrow::Array>>& columns,
                                     batch_list_t& record_batch);
};

#endif  /* __arrow_impl__ */
