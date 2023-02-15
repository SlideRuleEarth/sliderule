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
#include <arrow/builder.h>
#include <arrow/table.h>
#include <arrow/io/file.h>
#include <arrow/util/key_value_metadata.h>
#include <parquet/arrow/writer.h>
#include <parquet/arrow/schema.h>
#include <parquet/properties.h>
#include <parquet/file_writer.h>

#include "core.h"
#include "ParquetBuilder.h"
#include "ArrowParms.h"

#ifdef __aws__
#include "aws.h"
#endif

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

    static shared_ptr<arrow::Schema> defineTableSchema (field_list_t& field_list, const char* rec_type, const geo_data_t& geo);
    static bool addFieldsToSchema (vector<shared_ptr<arrow::Field>>& schema_vector, field_list_t& field_list, const geo_data_t& geo, const char* rec_type, int offset);
};

/*----------------------------------------------------------------------------
 * defineTableSchema
 *----------------------------------------------------------------------------*/
shared_ptr<arrow::Schema> ParquetBuilder::impl::defineTableSchema (field_list_t& field_list, const char* rec_type, const geo_data_t& geo)
{
    vector<shared_ptr<arrow::Field>> schema_vector;
    addFieldsToSchema(schema_vector, field_list, geo, rec_type, 0);
    if(geo.as_geo) schema_vector.push_back(arrow::field("geometry", arrow::binary()));
    return make_shared<arrow::Schema>(schema_vector);
}

/*----------------------------------------------------------------------------
 * addFieldsToSchema
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::impl::addFieldsToSchema (vector<shared_ptr<arrow::Field>>& schema_vector, field_list_t& field_list, const geo_data_t& geo, const char* rec_type, int offset)
{
    /* Loop Through Fields in Record */
    char** field_names = NULL;
    RecordObject::field_t** fields = NULL;
    int num_fields = RecordObject::getRecordFields(rec_type, &field_names, &fields);
    for(int i = 0; i < num_fields; i++)
    {
        bool add_field_to_list = true;

        /* Check for Geometry Columns */
        if(geo.as_geo)
        {
            if( (geo.lat_field.offset == (fields[i]->offset + offset)) ||
                (geo.lon_field.offset == (fields[i]->offset + offset)) )
            {
                /* skip over source columns for geometry as they will be added
                 * separately as a part of the dedicated geometry column */
                continue;
            }
        }

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

            case RecordObject::USER:    addFieldsToSchema(schema_vector, field_list, geo, fields[i]->exttype, fields[i]->offset);
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

        /* Free Allocations */
        delete [] field_names[i];
        delete  fields[i];
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
 * luaCreate - :parquet(<filename>, <outq name>, <rec_type>, <id>, <lon_key>, <lat_key>)
 *----------------------------------------------------------------------------*/
int ParquetBuilder::luaCreate (lua_State* L)
{
    ArrowParms* parms = NULL;

    try
    {
        /* Get Parameters */
        parms                       = (ArrowParms*)getLuaObject(L, 1, ArrowParms::OBJECT_TYPE);
        const char* outq_name       = getLuaString(L, 2);
        const char* rec_type        = getLuaString(L, 3);
        const char* id              = getLuaString(L, 4);
        const char* lon_key         = getLuaString(L, 5, true, NULL);
        const char* lat_key         = getLuaString(L, 6, true, NULL);

        /* Build Geometry Fields */
        geo_data_t geo;
        geo.as_geo = false;
        if((lat_key != NULL) && (lon_key != NULL))
        {
            geo.as_geo = true;

            geo.lon_field = RecordObject::getDefinedField(rec_type, lon_key);
            if(geo.lon_field.type == RecordObject::INVALID_FIELD)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to extract longitude field [%s] from record type <%s>", lon_key, rec_type);
            }

            geo.lat_field = RecordObject::getDefinedField(rec_type, lat_key);
            if(geo.lat_field.type == RecordObject::INVALID_FIELD)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to extract latitude field [%s] from record type <%s>", lat_key, rec_type);
            }
        }

        /* Create Dispatch */
        return createLuaObject(L, new ParquetBuilder(L, parms, outq_name, rec_type, id, geo));
    }
    catch(const RunTimeException& e)
    {
        if(parms) parms->releaseLuaObject();
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
ParquetBuilder::ParquetBuilder (lua_State* L, ArrowParms* _parms, const char* outq_name, const char* rec_type, const char* id, geo_data_t geo):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(parms);
    assert(outq_name);
    assert(rec_type);
    assert(id);

    /* Save Parms */
    parms = _parms;

    /* Allocate Private Implementation */
    pimpl = new ParquetBuilder::impl;

    /* Initialize Publisher */
    outQ = new Publisher(outq_name);

    /* Row Size */
    rowSizeBytes = RecordObject::getRecordDataSize(rec_type);

    /* Initialize GeoParquet Option */
    geoData = geo;

    /* Define Table Schema */
    pimpl->schema = pimpl->defineTableSchema(fieldList, rec_type, geoData);
    fieldIterator = new field_iterator_t(fieldList);

    /* Create Unique Temporary Filename */
    SafeString tmp_file("%s%s.parquet", TMP_FILE_PREFIX, id);
    fileName = tmp_file.str(true);

    /* Create Arrow Output Stream */
    shared_ptr<arrow::io::FileOutputStream> file_output_stream;
    PARQUET_ASSIGN_OR_THROW(file_output_stream, arrow::io::FileOutputStream::Open(fileName));

    /* Create Writer Properties */
    parquet::WriterProperties::Builder writer_props_builder;
    writer_props_builder.compression(parquet::Compression::GZIP);
    writer_props_builder.version(parquet::ParquetVersion::PARQUET_2_6);
    shared_ptr<parquet::WriterProperties> writer_props = writer_props_builder.build();

    /* Create Arrow Writer Properties */
    auto arrow_writer_props = parquet::ArrowWriterProperties::Builder().store_schema()->build();

    /* Build GeoParquet MetaData */
    if(geoData.as_geo)
    {
        auto metadata = pimpl->schema->metadata() ? pimpl->schema->metadata()->Copy() : std::make_shared<arrow::KeyValueMetadata>();
        const char* geodata_str = buildGeoMetaData();
        const char* serverdata_str = buildServerMetaData();
        metadata->Append("geo", geodata_str);
        metadata->Append("sliderule", serverdata_str);
        pimpl->schema = pimpl->schema->WithMetadata(metadata);
        delete [] geodata_str;
        delete [] serverdata_str;
    }

    /* Create Parquet Writer */
    #ifdef APACHE_ARROW_10_COMPAT
        (void)parquet::arrow::FileWriter::Open(*pimpl->schema, ::arrow::default_memory_pool(), file_output_stream, writer_props, arrow_writer_props, &pimpl->parquetWriter);
    #elif 0 // alternative method of creating file writer
        std::shared_ptr<parquet::SchemaDescriptor> parquet_schema;
        (void)parquet::arrow::ToParquetSchema(pimpl->schema.get(), *writer_props, *arrow_writer_props, &parquet_schema);
        auto schema_node = std::static_pointer_cast<parquet::schema::GroupNode>(parquet_schema->schema_root());
        std::unique_ptr<parquet::ParquetFileWriter> base_writer;
        base_writer = parquet::ParquetFileWriter::Open(std::move(file_output_stream), schema_node, std::move(writer_props), metadata);
        auto schema_ptr = std::make_shared<::arrow::Schema>(*pimpl->schema);
        (void)parquet::arrow::FileWriter::Make(::arrow::default_memory_pool(), std::move(base_writer), std::move(schema_ptr), std::move(arrow_writer_props), &pimpl->parquetWriter);
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
    parms->releaseLuaObject();
    delete [] fileName;
    delete outQ;
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

    /* Add Geometry Column (if GeoParquet) */
    if(geoData.as_geo)
    {
        RecordObject::field_t lon_field = geoData.lon_field;
        RecordObject::field_t lat_field = geoData.lat_field;
        shared_ptr<arrow::Array> column;
        arrow::BinaryBuilder builder;
        for(int row = 0; row < num_rows; row++)
        {
            wkbpoint_t point = {
                #ifdef __be__
                .byteOrder = 0,
                #else
                .byteOrder = 1,
                #endif
                .wkbType = 1,
                .x = record->getValueReal(lon_field),
                .y = record->getValueReal(lat_field)
            };
            (void)builder.Append((uint8_t*)&point, sizeof(wkbpoint_t));
            lon_field.offset += rowSizeBytes * 8;
            lat_field.offset += rowSizeBytes * 8;
        }
        (void)builder.Finish(&column);
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
    /* Early Exit on No Writer */
    if(!pimpl->parquetWriter) return false;

    /* Close Parquet Writer */
    (void)pimpl->parquetWriter->Close();

    /* Send File to User */
    int file_path_len = StringLib::size(parms->path);
    if((file_path_len > 5) &&
       (parms->path[0] == 's') &&
       (parms->path[1] == '3') &&
       (parms->path[2] == ':') &&
       (parms->path[3] == '/') &&
       (parms->path[4] == '/'))
    {
        #ifdef __aws__
        return send2S3(&parms->path[5]);
        #else
        LuaEndpoint::generateExceptionStatus(RTE_ERROR, CRITICAL, outQ, NULL, "Output path specifies S3, but server not compiled with AWS support");
        #endif
    }
    else
    {
        /* Stream Back to Client */
        return send2Client();
    }
}

/*----------------------------------------------------------------------------
 * send2S3
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::send2S3 (const char* s3dst)
{
    #ifdef __aws__

    bool status = true;

    /* Check Path */
    if(!s3dst) return false;

    /* Get Bucket and Key */
    char* bucket = StringLib::duplicate(s3dst);
    char* key = bucket;
    while(*key != '\0' && *key != '/') key++;
    if(*key == '/')
    {
        *key = '\0';
    }
    else
    {
        status = false;
        mlog(CRITICAL, "invalid S3 url: %s", s3dst);
    }
    key++;

    /* Put File */
    if(status)
    {
        /* Send Initial Status */
        LuaEndpoint::generateExceptionStatus(RTE_INFO, INFO, outQ, NULL, "Initiated upload of results to S3, bucket = %s, key = %s", bucket, key);

        try
        {
            /* Upload to S3 */
            int64_t bytes_uploaded = S3CurlIODriver::put(fileName, bucket, key, parms->region, &parms->credentials);

            /* Send Successful Status */
            LuaEndpoint::generateExceptionStatus(RTE_INFO, INFO, outQ, NULL, "Upload to S3 completed, bucket = %s, key = %s, size = %ld", bucket, key, bytes_uploaded);
        }
        catch(const RunTimeException& e)
        {
            status = false;

            /* Send Error Status */
            LuaEndpoint::generateExceptionStatus(RTE_ERROR, e.level(), outQ, NULL, "Upload to S3 failed, bucket = %s, key = %s, error = %s", bucket, key, e.what());
        }
    }

    /* Clean Up */
    delete [] bucket;

    /* Return Status */
    return status;

    #else
    return false;
    #endif
}

/*----------------------------------------------------------------------------
 * send2Client
 *----------------------------------------------------------------------------*/
bool ParquetBuilder::send2Client (void)
{
    bool status = true;

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
            StringLib::copy(&meta->filename[0], parms->path, FILE_NAME_MAX_LEN);
            meta->size = file_size;
            if(!meta_record.post(outQ))
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
                StringLib::copy(&data->filename[0], parms->path, FILE_NAME_MAX_LEN);
                size_t bytes_read = fread(data->data, 1, FILE_BUFFER_RSPS_SIZE, fp);
                if(!data_record.post(outQ, offsetof(arrow_file_data_t, data) + bytes_read))
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
 * buildGeoMetaData
 *----------------------------------------------------------------------------*/
const char* ParquetBuilder::buildGeoMetaData (void)
{
    SafeString geostr(R"json({
        "version": "1.0.0-beta.1",
        "primary_column": "geometry",
        "columns": {
            "geometry": {
                "encoding": "WKB",
                "geometry_types": ["Point"],
                "crs": {
                    "$schema": "https://proj.org/schemas/v0.5/projjson.schema.json",
                    "type": "GeographicCRS",
                    "name": "WGS 84 longitude-latitude",
                    "datum": {
                        "type": "GeodeticReferenceFrame",
                        "name": "World Geodetic System 1984",
                        "ellipsoid": {
                            "name": "WGS 84",
                            "semi_major_axis": 6378137,
                            "inverse_flattening": 298.257223563
                        }
                    },
                    "coordinate_system": {
                        "subtype": "ellipsoidal",
                        "axis": [
                            {
                                "name": "Geodetic longitude",
                                "abbreviation": "Lon",
                                "direction": "east",
                                "unit": "degree"
                            },
                            {
                                "name": "Geodetic latitude",
                                "abbreviation": "Lat",
                                "direction": "north",
                                "unit": "degree"
                            }
                        ]
                    },
                    "id": {
                        "authority": "OGC",
                        "code": "CRS84"
                    }
                },
                "edges": "planar",
                "bbox": [-180.0, -90.0, 180.0, 90.0],
                "epoch": 2018.0
            }
        }
    })json");

    const char* oldtxt[2] = { "    ", "\n" };
    const char* newtxt[2] = { "",     " " };
    geostr.inreplace(oldtxt, newtxt, 2);

    return geostr.str(true);
}

/*----------------------------------------------------------------------------
 * buildServerMetaData
 *----------------------------------------------------------------------------*/
const char* ParquetBuilder::buildServerMetaData (void)
{
    /* Build Launch Time String */
    int64_t launch_time_gps = TimeLib::sys2gpstime(OsApi::getLaunchTime());
    TimeLib::gmt_time_t timeinfo = TimeLib::gps2gmttime(launch_time_gps);
    TimeLib::date_t dateinfo = TimeLib::gmt2date(timeinfo);
    SafeString timestr("%04d-%02d-%02dT%02d:%02d:%02dZ", timeinfo.year, dateinfo.month, dateinfo.day, timeinfo.hour, timeinfo.minute, timeinfo.second);

    /* Build Duration String */
    int64_t duration = TimeLib::gpstime() - launch_time_gps;
    SafeString durationstr("%ld", duration);

    /* Build Package String */
    const char** pkg_list = LuaEngine::getPkgList();
    SafeString packagestr("[");
    if(pkg_list)
    {
        int index = 0;
        while(pkg_list[index])
        {
            packagestr += pkg_list[index];
            index++;
            if(pkg_list[index]) packagestr += ", ";
        }
    }
    packagestr += "]";

    /* Initialize Meta Data String */
    SafeString metastr(R"json({
        "server":
        {
            "environment":"$1",
            "version":"$2",
            "duration":$3,
            "packages":$4,
            "commit":"$5",
            "launch":"$6"
        }
    })json");

    /* Fill In Meta Data String */
    const char* oldtxt[8] = { "    ", "\n", "$1", "$2", "$3", "$4", "$5", "$6" };
    const char* newtxt[8] = { "", " ", OsApi::getEnvVersion(), LIBID, durationstr.str(), packagestr.str(), BUILDINFO, timestr.str() };
    metastr.inreplace(oldtxt, newtxt, 8);

    /* Return Copy of Meta String */
    return metastr.str(true);
}
