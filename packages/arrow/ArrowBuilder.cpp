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

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <arrow/util/logging.h>

#include "core.h"
#include "ArrowBuilder.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArrowBuilder::LuaMetaName = "ArrowBuilder";
const struct luaL_Reg ArrowBuilder::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :arrow(<outq name>, <rec_type>)
 *----------------------------------------------------------------------------*/
int ArrowBuilder::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);
        const char* rec_type = getLuaString(L, 2);

        /* Create ATL06 Dispatch */
        return createLuaObject(L, new ArrowBuilder(L, outq_name, rec_type));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ArrowBuilder::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ArrowBuilder::deinit (void)
{
}

/******************************************************************************
 * PRIVATE METOHDS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowBuilder::ArrowBuilder (lua_State* L, const char* outq_name, const char* rec_type):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(outq_name);

    /* Define Table Schema */
    defineTableSchema(schema, fieldList, rec_type);

    /* Initialize Publisher */
    outQ = new Publisher(outq_name);

    /* Row Size */
    rowSizeBytes = RecordObject::getRecordDataSize(rec_type);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ArrowBuilder::~ArrowBuilder(void)
{
    delete outQ;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    /* Determine Number of Rows in Record */
    int record_size_bytes = record->getAllocatedDataSize();
    int num_rows = record_size_bytes / rowSizeBytes;
    int left_over = record_size_bytes % rowSizeBytes;
    if(left_over > 0)
    {
        mlog(ERROR, "Invalid record size received for %s: %d %% %d != 0", record->getRecordType(), record_size_bytes, rowSizeBytes);
        return false;
    }

    /* Loop Through Fields in Schema */
    std::vector<std::shared_ptr<arrow::Array>> columns;
    for(int i = 0; i < fieldList.length(); i++)
    {
        RecordObject::field_t field = fieldList[i];
        std::shared_ptr<arrow::Array> column;

        /* Loop Through Each Row */
        switch(field.type)
        {
            case RecordObject::DOUBLE:
            {
                arrow::DoubleBuilder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((double)record->getValueReal(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::FLOAT:
            {
                arrow::FloatBuilder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((float)record->getValueReal(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT8:
            {
                arrow::Int8Builder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((int8_t)record->getValueInteger(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT16:
            {
                arrow::Int16Builder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((int16_t)record->getValueInteger(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT32:
            {
                arrow::Int32Builder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((int32_t)record->getValueInteger(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT64:
            {
                arrow::Int64Builder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((int64_t)record->getValueInteger(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT8:
            {
                arrow::UInt8Builder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((uint8_t)record->getValueInteger(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT16:
            {
                arrow::UInt16Builder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((uint16_t)record->getValueInteger(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT32:
            {
                arrow::UInt32Builder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((uint32_t)record->getValueInteger(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT64:
            {
                arrow::UInt64Builder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((uint64_t)record->getValueInteger(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::TIME8:
            {
                arrow::Date64Builder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    builder.UnsafeAppend((int64_t)record->getValueInteger(field));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::STRING:
            {
                arrow::StringBuilder builder;
                (void)builder.Reserve(num_rows);
                for(int row = 0; row < num_rows; row++)
                {
                    const char* str = record->getValueText(field);
                    builder.UnsafeAppend(str, StringLib::size(str));
                    field.offset += record_size_bytes * 8;
                }
                (void)builder.Finish(&column);
                break;
            }

            default:
            {
                break;
            }
        }

        /* Add Column to Columns */
        columns.push_back(column);
    }

    /* Build Table */
    std::shared_ptr<arrow::Table> table = arrow::Table::Make(std::make_shared<arrow::Schema>(*schema), columns);

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * processTimeout
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::processTimeout (void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * processTermination
 *
 *  Note that RecordDispatcher will only call this once
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::processTermination (void)
{
//    std::shared_ptr<arrow::io::FileOutputStream> file_output_stream;
//    arrow::io::FileOutputStream::Open("test.parquet", &file_output_stream);
//    parquet::WriterProperties::Builder props_builder;
//    props_builder.compression(parquet::Compression::GZIP);
//    props_builder.compression("dip", parquet::Compression::SNAPPY);
//    auto props = props_builder.build();
//    parquet::arrow::WriteTable(*arrow_table, ::arrow::default_memory_pool(), file_output_stream, sip_array->length(), props);
//
//    auto arrow_output_stream = std::make_shared<parquet::ArrowOutputStream>(file_output_stream);
//    std::unique_ptr<parquet::arrow::FileWriter> writer;
//    parquet::arrow::FileWriter::Open(*(arrow_table->schema()), ::arrow::default_memory_pool(),
//        arrow_output_stream, props, parquet::arrow::default_arrow_writer_properties(),
//        &writer);
//    // write two row groups for the first table
//    writer->WriteTable(*arrow_table, sip_array->length()/2);
//    // ... code here would generate a new table ...
//    // for now, we'll just write out the same table again, to
//    // simulate writing more data to the same file, this
//    // time as one row group
//    writer->WriteTable(*arrow_table, sip_array->length());
//    writer->Close();
    return true;
}

/*----------------------------------------------------------------------------
 * defineTableSchema
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::defineTableSchema (std::shared_ptr<arrow::Schema>& _schema, field_list_t& field_list, const char* rec_type)
{
    std::vector<std::shared_ptr<arrow::Field>> schema_vector;
    addFieldsToSchema(schema_vector, field_list, rec_type);
    _schema = std::make_shared<arrow::Schema>(schema_vector);
    return true;
}

/*----------------------------------------------------------------------------
 * addFieldsToSchema
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::addFieldsToSchema (std::vector<std::shared_ptr<arrow::Field>>& schema_vector, field_list_t& field_list, const char* rec_type)
{
    /* Loop Through Fields in Record */
    char** field_names = NULL;
    RecordObject::field_t** fields = NULL;
    int num_fields = RecordObject::getRecordFields(rec_type, &field_names, &fields);
    for(int i = 0; i < num_fields; i++)
    {
        /* Add to Schema */
        switch(fields[i]->type)
        {
            case RecordObject::INT8:    schema_vector.push_back(arrow::field(field_names[i], arrow::int8()));       break;
            case RecordObject::INT16:   schema_vector.push_back(arrow::field(field_names[i], arrow::int16()));      break;
            case RecordObject::INT32:   schema_vector.push_back(arrow::field(field_names[i], arrow::int32()));      break;
            case RecordObject::INT64:   schema_vector.push_back(arrow::field(field_names[i], arrow::int64()));      break;
            case RecordObject::UINT8:   schema_vector.push_back(arrow::field(field_names[i], arrow::uint8()));      break;
            case RecordObject::UINT16:  schema_vector.push_back(arrow::field(field_names[i], arrow::uint16()));     break;
            case RecordObject::UINT32:  schema_vector.push_back(arrow::field(field_names[i], arrow::uint32()));     break;
            case RecordObject::UINT64:  schema_vector.push_back(arrow::field(field_names[i], arrow::uint64()));     break;
            case RecordObject::FLOAT:   schema_vector.push_back(arrow::field(field_names[i], arrow::float32()));    break;
            case RecordObject::DOUBLE:  schema_vector.push_back(arrow::field(field_names[i], arrow::float64()));    break;
            case RecordObject::TIME8:   schema_vector.push_back(arrow::field(field_names[i], arrow::date64()));     break;
            case RecordObject::STRING:  schema_vector.push_back(arrow::field(field_names[i], arrow::utf8()));       break;
            case RecordObject::USER:    addFieldsToSchema(schema_vector, field_list, fields[i]->exttype); break;
            default:                    break;
        }

        /* Add to Field List */
        field_list.add(*fields[i]);
    }

    /* Clean Up */
    if(fields) delete [] fields;
    if(field_names) delete [] field_names;

    /* Return Success */
    return true;
}
