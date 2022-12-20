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

#include <iostream>
#include "arrow/builder.h"
#include "arrow/table.h"
#include "arrow/io/file.h"
#include <parquet/arrow/writer.h>
#include <parquet/properties.h>

#include "core.h"
#include "ParquetBuilder.h"

using std::shared_ptr;
using std::unique_ptr;
using std::make_shared;
using std::vector;

/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

struct ParquetBuilder::impl
{
    shared_ptr<arrow::Schema>               schema;
    unique_ptr<parquet::arrow::FileWriter>  parquetWriter;

    static shared_ptr<arrow::Schema> defineTableSchema (field_list_t& field_list, const char* rec_type);
    static bool addFieldsToSchema (vector<shared_ptr<arrow::Field>>& schema_vector, field_list_t& field_list, const char* rec_type, int offset);
};

/*----------------------------------------------------------------------------
 * defineTableSchema
 *----------------------------------------------------------------------------*/
shared_ptr<arrow::Schema> ParquetBuilder::impl::defineTableSchema (field_list_t& field_list, const char* rec_type)
{
    vector<shared_ptr<arrow::Field>> schema_vector;
    addFieldsToSchema(schema_vector, field_list, rec_type, 0);
    return make_shared<arrow::Schema>(schema_vector);
}

/*----------------------------------------------------------------------------
 * addFieldsToSchema
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::impl::addFieldsToSchema (vector<shared_ptr<arrow::Field>>& schema_vector, field_list_t& field_list, const char* rec_type, int offset)
{
    /* Loop Through Fields in Record */
    char** field_names = NULL;
    RecordObject::field_t** fields = NULL;
    int num_fields = RecordObject::getRecordFields(rec_type, &field_names, &fields);
    for(int i = 0; i < num_fields; i++)
    {
        bool add_field_to_list = true;

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

            case RecordObject::USER:    addFieldsToSchema(schema_vector, field_list, fields[i]->exttype, fields[i]->offset);
                                        add_field_to_list = false;
                                        break;

            default:                    add_field_to_list = false;
                                        break;
        }

        /* Add to Field List */
        if(add_field_to_list)
        {
            RecordObject::field_t column_field = *fields[i];
            column_field.offset += offset;
            field_list.add(column_field);
        }
    }

    /* Clean Up */
    if(fields) delete [] fields;
    if(field_names) delete [] field_names;

    /* Return Success */
    return true;
}

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ParquetBuilder::LuaMetaName = "ParquetBuilder";
const struct luaL_Reg ParquetBuilder::LuaMetaTable[] = {
    {NULL,          NULL}
};

const char* ParquetBuilder::metaRecType = "arrowrec.meta";
const RecordObject::fieldDef_t ParquetBuilder::metaRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(arrow_file_meta_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"size",       RecordObject::INT64,    offsetof(arrow_file_meta_t, size),                      1,  NULL, NATIVE_FLAGS}
};

const char* ParquetBuilder::dataRecType = "arrowrec.data";
const RecordObject::fieldDef_t ParquetBuilder::dataRecDef[] = {
    {"filename",   RecordObject::STRING,   offsetof(arrow_file_data_t, filename),  FILE_NAME_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"data",       RecordObject::UINT8,    offsetof(arrow_file_data_t, data),                      0,  NULL, NATIVE_FLAGS} // variable length
};

const char* ParquetBuilder::TMP_FILE_PREFIX = "/tmp/";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :arrow(<filename>, <outq name>, <rec_type>, <id>)
 *----------------------------------------------------------------------------*/
int ParquetBuilder::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* filename = getLuaString(L, 1);
        const char* outq_name = getLuaString(L, 2);
        const char* rec_type = getLuaString(L, 3);
        const char* id = getLuaString(L, 4);

        /* Create Dispatch */
        return createLuaObject(L, new ParquetBuilder(L, filename, outq_name, rec_type, id));
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
void ParquetBuilder::init (void)
{
    RECDEF(metaRecType, metaRecDef, sizeof(arrow_file_meta_t), NULL);
    RECDEF(dataRecType, dataRecDef, sizeof(arrow_file_data_t), NULL);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void ParquetBuilder::deinit (void)
{
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ParquetBuilder::ParquetBuilder (lua_State* L, const char* filename, const char* outq_name, const char* rec_type, const char* id):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(filename);
    assert(outq_name);
    assert(rec_type);
    assert(id);

    /* Allocate Private Implementation */
    pimpl = new ParquetBuilder::impl;

    /* Save Output Filename */
    outFileName = StringLib::duplicate(filename);

    /* Define Table Schema */
    pimpl->schema = pimpl->defineTableSchema(fieldList, rec_type);
    fieldIterator = new field_iterator_t(fieldList);

    /* Initialize Publisher */
    outQ = new Publisher(outq_name);

    /* Row Size */
    rowSizeBytes = RecordObject::getRecordDataSize(rec_type);

    /* Create Unique Temporary Filename */
    SafeString tmp_file("%s%s.parquet", TMP_FILE_PREFIX, id);
    fileName = tmp_file.getString(true);

    /* Create Arrow Output Stream */
    shared_ptr<arrow::io::FileOutputStream> file_output_stream;
    PARQUET_ASSIGN_OR_THROW(file_output_stream, arrow::io::FileOutputStream::Open(fileName));

    /* Create Writer Properties */
    parquet::WriterProperties::Builder writer_props_builder;
    writer_props_builder.compression(parquet::Compression::GZIP);
    shared_ptr<parquet::WriterProperties> writer_props = writer_props_builder.build();

    /* Create Arrow Writer Properties */
    parquet::ArrowWriterProperties::Builder arrow_writer_props_builder;
    shared_ptr<parquet::ArrowWriterProperties> arrow_writer_props = arrow_writer_props_builder.build();

    /* Create Parquet Writer */
    #ifdef APACHE_ARROW_10_COMPAT
        (void)parquet::arrow::FileWriter::Open(*pimpl->schema, ::arrow::default_memory_pool(), file_output_stream, writer_props, arrow_writer_props, &pimpl->parquetWriter);
    #else
        arrow::Result<std::unique_ptr<parquet::arrow::FileWriter>> result = parquet::arrow::FileWriter::Open(*pimpl->schema, ::arrow::default_memory_pool(), file_output_stream, writer_props, arrow_writer_props);
        if(result.ok()) pimpl->parquetWriter = std::move(result).ValueOrDie();
        else mlog(CRITICAL, "Failed to open parquet writer: %s", result.status().ToString().c_str());
    #endif
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ParquetBuilder::~ParquetBuilder(void)
{
    delete [] fileName;
    delete outQ;
    delete [] outFileName;
    delete fieldIterator;
    delete pimpl;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::processRecord (RecordObject* record, okey_t key)
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
    vector<shared_ptr<arrow::Array>> columns;
    for(int i = 0; i < fieldIterator->length; i++)
    {
        RecordObject::field_t field = (*fieldIterator)[i];
        shared_ptr<arrow::Array> column;

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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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
                    field.offset += rowSizeBytes * 8;
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

    /* Write Table */
    if(pimpl->parquetWriter)
    {
        tableMut.lock();
        {
            /* Build and Write Table */
            shared_ptr<arrow::Table> table = arrow::Table::Make(pimpl->schema, columns);
            (void)pimpl->parquetWriter->WriteTable(*table, num_rows);
        }
        tableMut.unlock();
    }

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * processTimeout
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::processTimeout (void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * processTermination
 *
 *  Note that RecordDispatcher will only call this once
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::processTermination (void)
{
    bool status = true;

    /* Early Exit on No Writer */
    if(!pimpl->parquetWriter) return false;

    /* Close Parquet Writer */
    (void)pimpl->parquetWriter->Close();

    /* Reopen Parquet File to Stream Back as Response */
    FILE* fp = fopen(fileName, "r");
    if(fp)
    {
        /* Get Size of File */
        fseek(fp, 0L, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        do
        {
            /* Send Meta Record */
            RecordObject meta_record(metaRecType);
            arrow_file_meta_t* meta = (arrow_file_meta_t*)meta_record.getRecordData();
            StringLib::copy(&meta->filename[0], outFileName, FILE_NAME_MAX_LEN);
            meta->size = file_size;
            if(!postRecord(&meta_record))
            {
                status = false;
                break; // early exit on error
            }

            /* Send Data Records */
            long offset = 0;
            while(offset < file_size)
            {
                RecordObject data_record(dataRecType, 0, false);
                arrow_file_data_t* data = (arrow_file_data_t*)data_record.getRecordData();
                StringLib::copy(&data->filename[0], outFileName, FILE_NAME_MAX_LEN);
                size_t bytes_read = fread(data->data, 1, FILE_BUFFER_RSPS_SIZE, fp);
                if(!postRecord(&data_record, offsetof(arrow_file_data_t, data) + bytes_read))
                {
                    status = false;
                    break; // early exit on error
                }
                offset += bytes_read;
            }
        } while(false);

        /* Close File */
        int rc1 = fclose(fp);
        if(rc1 != 0)
        {
            status = false;
            mlog(CRITICAL, "Failed (%d) to close file %s: %s", rc1, fileName, strerror(errno));
        }

        /* Remove File */
        int rc2 = remove(fileName);
        if(rc2 != 0)
        {
            status = false;
            mlog(CRITICAL, "Failed (%d) to delete file %s: %s", rc2, fileName, strerror(errno));
        }
    }
    else // unable to open file
    {
        status = false;
        mlog(CRITICAL, "Failed (%d) to read parquet file %s: %s", errno, fileName, strerror(errno));
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * postRecord
 *
 *  The while loop should not be an infinite loop because when the output queue
 *  is cleaned up, the call to postRef should fail with no subscribers
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::postRecord (RecordObject* record, int data_size)
{
    uint8_t* rec_buf = NULL;
    int rec_bytes = record->serialize(&rec_buf, RecordObject::TAKE_OWNERSHIP, data_size);
    int post_status = MsgQ::STATE_TIMEOUT;
    while((post_status = outQ->postRef(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT);
    if(post_status <= 0)
    {
        delete [] rec_buf; // we've taken ownership
        mlog(ERROR, "Parquet builder failed to post %s to stream %s: %d", record->getRecordType(), outQ->getName(), post_status);
        return false;
    }
    return true;
}
