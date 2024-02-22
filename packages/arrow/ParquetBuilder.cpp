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
#include <regex>

#include "core.h"
#include "ParquetBuilder.h"
#include "ArrowParms.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ParquetBuilder::OBJECT_TYPE = "ParquetBuilder";
const char* ParquetBuilder::LUA_META_NAME = "ParquetBuilder";
const struct luaL_Reg ParquetBuilder::LUA_META_TABLE[] = {
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

const char* ParquetBuilder::remoteRecType = "arrowrec.remote";
const RecordObject::fieldDef_t ParquetBuilder::remoteRecDef[] = {
    {"url",   RecordObject::STRING,   offsetof(arrow_file_remote_t, url),                URL_MAX_LEN,  NULL, NATIVE_FLAGS},
    {"size",  RecordObject::INT64,    offsetof(arrow_file_remote_t, size),                         1,  NULL, NATIVE_FLAGS}
};

const char* ParquetBuilder::TMP_FILE_PREFIX = "/tmp/";

/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

struct ParquetBuilder::impl
{
    shared_ptr<arrow::Schema>               schema;
    unique_ptr<parquet::arrow::FileWriter>  parquetWriter;

    /*----------------------------------------------------------------------------
    * addFieldsToSchema
    *----------------------------------------------------------------------------*/
    static bool addFieldsToSchema (vector<shared_ptr<arrow::Field>>& schema_vector, 
                                   field_list_t& field_list, 
                                   const char** batch_rec_type, 
                                   const geo_data_t& geo, 
                                   const char* rec_type, 
                                   int offset, 
                                   int flags)
    {
        /* Loop Through Fields in Record */
        Dictionary<RecordObject::field_t>* fields = RecordObject::getRecordFields(rec_type);
        Dictionary<RecordObject::field_t>::Iterator field_iter(*fields);
        for(int i = 0; i < field_iter.length; i++)
        {
            Dictionary<RecordObject::field_t>::kv_t kv = field_iter[i];
            const char* field_name = kv.key;
            const RecordObject::field_t& field = kv.value;
            bool add_field_to_list = true;

            /* Check for Geometry Columns */
            if(geo.as_geo)
            {
                if(field.offset == geo.x_field.offset || field.offset == geo.y_field.offset) 
                {
                    /* skip over source columns for geometry as they will be added
                    * separately as a part of the dedicated geometry column */
                    continue;
                }
            }

            /* Check for Batch Record Type */
            if((*batch_rec_type == NULL) && (field.flags & RecordObject::BATCH))
            {
                *batch_rec_type = field.exttype;
            }

            /* Add to Schema */
            if(field.elements == 1 || field.type == RecordObject::USER)
            {
                switch(field.type)
                {
                    case RecordObject::INT8:    schema_vector.push_back(arrow::field(field_name, arrow::int8()));       break;
                    case RecordObject::INT16:   schema_vector.push_back(arrow::field(field_name, arrow::int16()));      break;
                    case RecordObject::INT32:   schema_vector.push_back(arrow::field(field_name, arrow::int32()));      break;
                    case RecordObject::INT64:   schema_vector.push_back(arrow::field(field_name, arrow::int64()));      break;
                    case RecordObject::UINT8:   schema_vector.push_back(arrow::field(field_name, arrow::uint8()));      break;
                    case RecordObject::UINT16:  schema_vector.push_back(arrow::field(field_name, arrow::uint16()));     break;
                    case RecordObject::UINT32:  schema_vector.push_back(arrow::field(field_name, arrow::uint32()));     break;
                    case RecordObject::UINT64:  schema_vector.push_back(arrow::field(field_name, arrow::uint64()));     break;
                    case RecordObject::FLOAT:   schema_vector.push_back(arrow::field(field_name, arrow::float32()));    break;
                    case RecordObject::DOUBLE:  schema_vector.push_back(arrow::field(field_name, arrow::float64()));    break;
                    case RecordObject::TIME8:   schema_vector.push_back(arrow::field(field_name, arrow::timestamp(arrow::TimeUnit::NANO))); break;
                    case RecordObject::STRING:  schema_vector.push_back(arrow::field(field_name, arrow::utf8()));       break;

                    case RecordObject::USER:    addFieldsToSchema(schema_vector, field_list, batch_rec_type, geo, field.exttype, field.offset, field.flags);
                                                add_field_to_list = false;
                                                break;

                    default:                    add_field_to_list = false;
                                                break;
                }
            }
            else // array
            {
                switch(field.type)
                {
                    case RecordObject::INT8:    schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::int8())));      break;
                    case RecordObject::INT16:   schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::int16())));     break;
                    case RecordObject::INT32:   schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::int32())));     break;
                    case RecordObject::INT64:   schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::int64())));     break;
                    case RecordObject::UINT8:   schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::uint8())));     break;
                    case RecordObject::UINT16:  schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::uint16())));    break;
                    case RecordObject::UINT32:  schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::uint32())));    break;
                    case RecordObject::UINT64:  schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::uint64())));    break;
                    case RecordObject::FLOAT:   schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::float32())));   break;
                    case RecordObject::DOUBLE:  schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::float64())));   break;
                    case RecordObject::TIME8:   schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::timestamp(arrow::TimeUnit::NANO)))); break;
                    case RecordObject::STRING:  schema_vector.push_back(arrow::field(field_name, arrow::list(arrow::utf8())));      break;

                    case RecordObject::USER:    // arrays of user data types (i.e. nested structures) are not supported
                                                add_field_to_list = false;
                                                break;
                                                
                    default:                    add_field_to_list = false;
                                                break;
                }
            }

            /* Add to Field List */
            if(add_field_to_list)
            {
                RecordObject::field_t column_field = field;
                column_field.offset += offset;
                column_field.flags |= flags;
                field_list.add(column_field);
            }
        }

        /* Return Success */
        return true;
    }
    
    /*----------------------------------------------------------------------------
    * appendGeoMetaData
    *----------------------------------------------------------------------------*/
    static void appendGeoMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata)
    {
        /* Initialize Meta Data String */
        string geostr(R"json({
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

        /* Reformat JSON */
        geostr = std::regex_replace(geostr, std::regex("    "), "");
        geostr = std::regex_replace(geostr, std::regex("\n"), " ");

        /* Append Meta String */
        metadata->Append("geo", geostr.c_str());
    }

    /*----------------------------------------------------------------------------
    * appendServerMetaData
    *----------------------------------------------------------------------------*/
    static void appendServerMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata)
    {
        /* Build Launch Time String */
        int64_t launch_time_gps = TimeLib::sys2gpstime(OsApi::getLaunchTime());
        TimeLib::gmt_time_t timeinfo = TimeLib::gps2gmttime(launch_time_gps);
        TimeLib::date_t dateinfo = TimeLib::gmt2date(timeinfo);
        FString timestr("%04d-%02d-%02dT%02d:%02d:%02dZ", timeinfo.year, dateinfo.month, dateinfo.day, timeinfo.hour, timeinfo.minute, timeinfo.second);

        /* Build Duration String */
        int64_t duration = TimeLib::gpstime() - launch_time_gps;
        FString durationstr("%ld", duration);

        /* Build Package String */
        const char** pkg_list = LuaEngine::getPkgList();
        string packagestr("[");
        if(pkg_list)
        {
            int index = 0;
            while(pkg_list[index])
            {
                packagestr += pkg_list[index];
                if(pkg_list[index + 1]) packagestr += ", ";
                delete [] pkg_list[index];
                index++;
            }
        }
        packagestr += "]";
        delete [] pkg_list;

        /* Initialize Meta Data String */
        string metastr(R"json({
            "server":
            {
                "environment":"_1_",
                "version":"_2_",
                "duration":_3_,
                "packages":_4_,
                "commit":"_5_",
                "launch":"_6_"
            }
        })json");

        /* Fill In Meta Data String */
        metastr = std::regex_replace(metastr, std::regex("    "), "");
        metastr = std::regex_replace(metastr, std::regex("\n"), " ");
        metastr = std::regex_replace(metastr, std::regex("_1_"), OsApi::getEnvVersion());
        metastr = std::regex_replace(metastr, std::regex("_2_"), LIBID);
        metastr = std::regex_replace(metastr, std::regex("_3_"), durationstr.c_str());
        metastr = std::regex_replace(metastr, std::regex("_4_"), packagestr.c_str());
        metastr = std::regex_replace(metastr, std::regex("_5_"), BUILDINFO);
        metastr = std::regex_replace(metastr, std::regex("_6_"), timestr.c_str());

        /* Append Meta String */
        metadata->Append("sliderule", metastr.c_str());
    }

    /*----------------------------------------------------------------------------
    * appendPandasMetaData
    *----------------------------------------------------------------------------*/
    static void appendPandasMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata, 
                                      const shared_ptr<arrow::Schema>& _schema, 
                                      const field_iterator_t* field_iterator, 
                                      const char* index_key,
                                      bool as_geo)
    {
        /* Initialize Pandas Meta Data String */
        string pandasstr(R"json({
            "index_columns": [_INDEX_],
            "column_indexes": [
                {
                    "name": null,
                    "field_name": null,
                    "pandas_type": "unicode",
                    "numpy_type": "object",
                    "metadata": {"encoding": "UTF-8"}
                }
            ],
            "columns": [_COLUMNS_],
            "creator": {"library": "pyarrow", "version": "10.0.1"},
            "pandas_version": "1.5.3"
        })json");

        /* Build Columns String */
        string columns;
        int index = 0;
        for(const std::string& field_name: _schema->field_names())
        {
            /* Initialize Column String */
            string columnstr(R"json({"name": "_NAME_", "field_name": "_NAME_", "pandas_type": "_PTYPE_", "numpy_type": "_NTYPE_", "metadata": null})json");
            const char* pandas_type = "";
            const char* numpy_type = "";
            bool is_last_entry = false;

            if(index < field_iterator->length)
            {
                /* Add Column from Field List */
                RecordObject::field_t field = (*field_iterator)[index++];
                switch(field.type)
                {
                    case RecordObject::DOUBLE:  pandas_type = "float64";    numpy_type = "float64";         break;
                    case RecordObject::FLOAT:   pandas_type = "float32";    numpy_type = "float32";         break;
                    case RecordObject::INT8:    pandas_type = "int8";       numpy_type = "int8";            break;
                    case RecordObject::INT16:   pandas_type = "int16";      numpy_type = "int16";           break;
                    case RecordObject::INT32:   pandas_type = "int32";      numpy_type = "int32";           break;
                    case RecordObject::INT64:   pandas_type = "int64";      numpy_type = "int64";           break;
                    case RecordObject::UINT8:   pandas_type = "uint8";      numpy_type = "uint8";           break;
                    case RecordObject::UINT16:  pandas_type = "uint16";     numpy_type = "uint16";          break;
                    case RecordObject::UINT32:  pandas_type = "uint32";     numpy_type = "uint32";          break;
                    case RecordObject::UINT64:  pandas_type = "uint64";     numpy_type = "uint64";          break;
                    case RecordObject::TIME8:   pandas_type = "datetime";   numpy_type = "datetime64[ns]";  break;
                    case RecordObject::STRING:  pandas_type = "bytes";      numpy_type = "object";          break;
                    default:                    pandas_type = "bytes";      numpy_type = "object";          break;
                }

                /* Mark Last Column */
                if(!as_geo && (index == field_iterator->length))
                {
                    is_last_entry = true;
                }
            }
            else if(as_geo && StringLib::match(field_name.c_str(), "geometry"))
            {
                /* Add Column for Geometry */
                pandas_type = "bytes";
                numpy_type = "object";
                is_last_entry = true;
            }
            
            /* Fill In Column String */
            columnstr = std::regex_replace(columnstr, std::regex("_NAME_"), field_name.c_str());
            columnstr = std::regex_replace(columnstr, std::regex("_PTYPE_"), pandas_type);
            columnstr = std::regex_replace(columnstr, std::regex("_NTYPE_"), numpy_type);

            /* Add Comma */
            if(!is_last_entry)
            {
                columnstr += ", ";
            }

            /* Add Column String to Columns */
            columns += columnstr;
        }

        /* Build Index String */
        FString indexstr("\"%s\"", index_key ? index_key : "");

        /* Fill In Pandas Meta Data String */
        pandasstr = std::regex_replace(pandasstr, std::regex("    "), "");
        pandasstr = std::regex_replace(pandasstr, std::regex("\n"), " ");
        pandasstr = std::regex_replace(pandasstr, std::regex("_INDEX_"), index_key ? indexstr.c_str() : "");
        pandasstr = std::regex_replace(pandasstr, std::regex("_COLUMNS_"), columns.c_str());

        /* Append Meta String */
        metadata->Append("pandas", pandasstr.c_str());
    }
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :parquet(<outq_name>, <inq_name>, <rec_type>, <id>, [<x_key>, <y_key>], [<time_key>])
 *----------------------------------------------------------------------------*/
int ParquetBuilder::luaCreate (lua_State* L)
{
    ArrowParms* _parms = NULL;
    geo_data_t geo = {
        .x_key = NULL,
        .y_key = NULL
    };

    try
    {
        /* Get Parameters */
        _parms                      = dynamic_cast<ArrowParms*>(getLuaObject(L, 1, ArrowParms::OBJECT_TYPE));
        const char* outq_name       = getLuaString(L, 2);
        const char* inq_name        = getLuaString(L, 3);
        const char* rec_type        = getLuaString(L, 4);
        const char* id              = getLuaString(L, 5);
        const char* x_key           = getLuaString(L, 6, true, NULL);
        const char* y_key           = getLuaString(L, 7, true, NULL);
        const char* index_key       = getLuaString(L, 8, true, NULL);

        /* Build Geometry Fields */
        geo.as_geo = _parms->as_geo;
        if(geo.as_geo && (x_key != NULL) && (y_key != NULL))
        {
            geo.x_field = RecordObject::getDefinedField(rec_type, x_key);
            if(geo.x_field.type == RecordObject::INVALID_FIELD)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to extract x field [%s] from record type <%s>", x_key, rec_type);
            }

            geo.y_field = RecordObject::getDefinedField(rec_type, y_key);
            if(geo.y_field.type == RecordObject::INVALID_FIELD)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to extract y field [%s] from record type <%s>", y_key, rec_type);
            }

            geo.x_key = StringLib::duplicate(x_key);
            geo.y_key = StringLib::duplicate(y_key);
        }
        else
        {
            /* Unable to Create GeoParquet */
            geo.as_geo = false;
        }

        /* Create Dispatch */
        return createLuaObject(L, new ParquetBuilder(L, _parms, outq_name, inq_name, rec_type, id, geo, index_key));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        delete [] geo.x_key;
        delete [] geo.y_key;
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
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
    RECDEF(remoteRecType, remoteRecDef, sizeof(arrow_file_remote_t), NULL);
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
ParquetBuilder::ParquetBuilder (lua_State* L, ArrowParms* _parms,
                                const char* outq_name, const char* inq_name,
                                const char* rec_type, const char* id, const geo_data_t& geo, const char* index_key):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    recType(StringLib::duplicate(rec_type)),
    batchRecType(NULL),
    fieldList(LIST_BLOCK_SIZE),
    geoData(geo)
{
    assert(_parms);
    assert(outq_name);
    assert(inq_name);
    assert(rec_type);
    assert(id);

    /* Check Path */
    if((parms->path == NULL) || (parms->path[0] == '\0'))
    {
        if(parms->asset_name)
        {
            /* Generate Output Path */
            Asset* asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(parms->asset_name, Asset::OBJECT_TYPE));
            const char* path_prefix = StringLib::match(asset->getDriver(), "s3") ? "s3://" : "";
            const char* path_suffix = parms->as_geo ? ".geoparquet" : ".parquet";
            FString path_name("/%s.%016lX", id, OsApi::time(OsApi::CPU_CLK));
            FString path_str("%s%s%s%s", path_prefix, asset->getPath(), path_name.c_str(), path_suffix);
            asset->releaseLuaObject();

            /* Set Output Path */
            outputPath = path_str.c_str(true);
            mlog(INFO, "Generating unique path: %s", outputPath);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to determine output path");
        }
    }
    else
    {
        outputPath = StringLib::duplicate(parms->path);
    }

    /* Allocate Private Implementation */
    pimpl = new ParquetBuilder::impl;

    /* Define Table Schema */    
    vector<shared_ptr<arrow::Field>> schema_vector;
    ParquetBuilder::impl::addFieldsToSchema(schema_vector, fieldList, &batchRecType, geoData, rec_type, 0, 0);
    if(geoData.as_geo) schema_vector.push_back(arrow::field("geometry", arrow::binary()));
    pimpl->schema = make_shared<arrow::Schema>(schema_vector);
    fieldIterator = new field_iterator_t(fieldList);

    /* Row Based Parameters */
    batchRowSizeBytes = RecordObject::getRecordDataSize(batchRecType);
    rowSizeBytes = RecordObject::getRecordDataSize(rec_type) + batchRowSizeBytes;
    maxRowsInGroup = ROW_GROUP_SIZE / rowSizeBytes;

    /* Initialize Queues */
    int qdepth = maxRowsInGroup * QUEUE_BUFFER_FACTOR;
    outQ = new Publisher(outq_name, Publisher::defaultFree, qdepth);
    inQ = new Subscriber(inq_name, MsgQ::SUBSCRIBER_OF_CONFIDENCE, qdepth);

    /* Create Unique Temporary Filename */
    FString tmp_file("%s%s.parquet", TMP_FILE_PREFIX, id);
    fileName = tmp_file.c_str(true);

    /* Create Arrow Output Stream */
    shared_ptr<arrow::io::FileOutputStream> file_output_stream;
    PARQUET_ASSIGN_OR_THROW(file_output_stream, arrow::io::FileOutputStream::Open(fileName));

    /* Create Writer Properties */
    parquet::WriterProperties::Builder writer_props_builder;
    writer_props_builder.compression(parquet::Compression::SNAPPY);
    writer_props_builder.version(parquet::ParquetVersion::PARQUET_2_6);
    shared_ptr<parquet::WriterProperties> writer_props = writer_props_builder.build();

    /* Create Arrow Writer Properties */
    auto arrow_writer_props = parquet::ArrowWriterProperties::Builder().store_schema()->build();

    /* Build GeoParquet MetaData */
    auto metadata = pimpl->schema->metadata() ? pimpl->schema->metadata()->Copy() : std::make_shared<arrow::KeyValueMetadata>();
    if(geoData.as_geo) ParquetBuilder::impl::appendGeoMetaData(metadata);
    ParquetBuilder::impl::appendServerMetaData(metadata);
    ParquetBuilder::impl::appendPandasMetaData(metadata, pimpl->schema, fieldIterator, index_key, geoData.as_geo);
    pimpl->schema = pimpl->schema->WithMetadata(metadata);

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

    /* Start Builder Thread */
    active = true;
    builderPid = new Thread(builderThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ParquetBuilder::~ParquetBuilder(void)
{
    active = false;
    delete builderPid;
    parms->releaseLuaObject();
    delete [] fileName;
    delete [] outputPath;
    delete [] recType;
    delete outQ;
    delete inQ;
    delete fieldIterator;
    delete pimpl;
    if(geoData.as_geo)
    {
        delete [] geoData.x_key;
        delete [] geoData.y_key;
    }
}

/*----------------------------------------------------------------------------
 * builderThread
 *----------------------------------------------------------------------------*/
void* ParquetBuilder::builderThread(void* parm)
{
    ParquetBuilder* builder = static_cast<ParquetBuilder*>(parm);
    int row_cnt = 0;

    /* Early Exit on No Writer */
    if(!builder->pimpl->parquetWriter)
    {
        return NULL;
    }

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, builder->traceId, "parquet_builder", "{\"filename\":\"%s\"}", builder->fileName);
    EventLib::stashId(trace_id);

    /* Loop Forever */
    while(builder->active)
    {
        /* Receive Message */
        Subscriber::msgRef_t ref;
        int recv_status = builder->inQ->receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            /* Process Record */
            if(ref.size > 0)
            {
                /* Get Record and Match to Type being Processed */
                RecordInterface* record = new RecordInterface((unsigned char*)ref.data, ref.size);
                if(!StringLib::match(record->getRecordType(), builder->recType))
                {
                    delete record;
                    builder->outQ->postCopy(ref.data, ref.size);
                    builder->inQ->dereference(ref);
                    continue;
                }

                /* Determine Rows in Record */
                int record_size_bytes = record->getAllocatedDataSize();
                int batch_size_bytes = record_size_bytes - (builder->rowSizeBytes - builder->batchRowSizeBytes);
                int num_rows =  batch_size_bytes / builder->batchRowSizeBytes;
                int left_over = batch_size_bytes % builder->batchRowSizeBytes;
                if(left_over > 0)
                {
                    mlog(ERROR, "Invalid record size received for %s: %d %% %d != 0", record->getRecordType(), batch_size_bytes, builder->batchRowSizeBytes);
                    delete record; // record is not batched, so must delete here
                    builder->inQ->dereference(ref); // record is not batched, so must dereference here
                    continue;
                }

                /* Create Batch Structure */
                batch_t batch = {
                    .ref = ref,
                    .record = record,
                    .rows = num_rows
                };

                /* Add Batch to Ordering */
                builder->recordBatch.add(row_cnt, batch);
                row_cnt += num_rows;
                if(row_cnt >= builder->maxRowsInGroup)
                {
                    builder->processRecordBatch(row_cnt);
                    row_cnt = 0;
                }
            }
            else
            {
                /* Terminating Message */
                mlog(DEBUG, "Terminator received on %s, exiting parquet builder", builder->inQ->getName());
                builder->active = false; // breaks out of loop
                builder->inQ->dereference(ref); // terminator is not batched, so must dereference here
            }
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            /* Break Out on Failure */
            mlog(CRITICAL, "Failed queue receive on %s with error %d", builder->inQ->getName(), recv_status);
            builder->active = false; // breaks out of loop
        }
    }

    /* Process Remaining Records */
    builder->processRecordBatch(row_cnt);

    /* Close Parquet Writer */
    (void)builder->pimpl->parquetWriter->Close();

    /* Send File to User */
    const char* _path = builder->outputPath;
    uint32_t send_trace_id = start_trace(INFO, trace_id, "send_file", "{\"path\": \"%s\"}", _path);
    int _path_len = StringLib::size(_path);
    if( (_path_len > 5) &&
        (_path[0] == 's') &&
        (_path[1] == '3') &&
        (_path[2] == ':') &&
        (_path[3] == '/') &&
        (_path[4] == '/'))
    {
        /* Upload File to S3 */
        builder->send2S3(&_path[5]);
    }
    else
    {
        /* Stream File Back to Client */
        builder->send2Client();
    }

    /* Remove File */
    int rc = remove(builder->fileName);
    if(rc != 0)
    {
        mlog(CRITICAL, "Failed (%d) to delete file %s: %s", rc, builder->fileName, strerror(errno));
    }

    stop_trace(INFO, send_trace_id);

    /* Signal Completion */
    builder->signalComplete();

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Exit Thread */
    return NULL;
}

/*----------------------------------------------------------------------------
 * processRecordBatch
 *----------------------------------------------------------------------------*/
void ParquetBuilder::processRecordBatch (int num_rows)
{
    batch_t batch;

    /* Start Trace */
    uint32_t parent_trace_id = EventLib::grabId();
    uint32_t trace_id = start_trace(INFO, parent_trace_id, "process_batch", "{\"num_rows\": %d}", num_rows);

    /* Loop Through Fields in Schema */
    vector<shared_ptr<arrow::Array>> columns;
    for(int i = 0; i < fieldIterator->length; i++)
    {
        uint32_t field_trace_id = start_trace(INFO, trace_id, "append_field", "{\"field\": %d}", i);
        RecordObject::field_t field = (*fieldIterator)[i];
        shared_ptr<arrow::Array> column;

        /* Loop Through Each Row */
        switch(field.type)
        {
            case RecordObject::DOUBLE:
            {
                arrow::DoubleBuilder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((double)batch.record->getValueReal(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        double value = (double)batch.record->getValueReal(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::FLOAT:
            {
                arrow::FloatBuilder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((float)batch.record->getValueReal(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        float value = (float)batch.record->getValueReal(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT8:
            {
                arrow::Int8Builder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((int8_t)batch.record->getValueInteger(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        int8_t value = (int8_t)batch.record->getValueInteger(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT16:
            {
                arrow::Int16Builder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((int16_t)batch.record->getValueInteger(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        int16_t value = (int16_t)batch.record->getValueInteger(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT32:
            {
                arrow::Int32Builder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((int32_t)batch.record->getValueInteger(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        int32_t value = (int32_t)batch.record->getValueInteger(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT64:
            {
                arrow::Int64Builder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((int64_t)batch.record->getValueInteger(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        int64_t value = (int64_t)batch.record->getValueInteger(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT8:
            {
                arrow::UInt8Builder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((uint8_t)batch.record->getValueInteger(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        uint8_t value = (uint8_t)batch.record->getValueInteger(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT16:
            {
                arrow::UInt16Builder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((uint16_t)batch.record->getValueInteger(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        uint16_t value = (uint16_t)batch.record->getValueInteger(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT32:
            {
                arrow::UInt32Builder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((uint32_t)batch.record->getValueInteger(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        uint32_t value = (uint32_t)batch.record->getValueInteger(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT64:
            {
                arrow::UInt64Builder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((uint64_t)batch.record->getValueInteger(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        uint64_t value = (uint64_t)batch.record->getValueInteger(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::TIME8:
            {
                arrow::TimestampBuilder builder(arrow::timestamp(arrow::TimeUnit::NANO), arrow::default_memory_pool());
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend((int64_t)batch.record->getValueInteger(field));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        int64_t value = (int64_t)batch.record->getValueInteger(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(value);
                        }
                    }
                    key = recordBatch.next(&batch);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::STRING:
            {
                arrow::StringBuilder builder;
                (void)builder.Reserve(num_rows);
                unsigned long key = recordBatch.first(&batch);
                while(key != (unsigned long)INVALID_KEY)
                {
                    if(field.flags & RecordObject::BATCH)
                    {
                        int32_t starting_offset = field.offset;
                        for(int row = 0; row < batch.rows; row++)
                        {
                            const char* str = batch.record->getValueText(field);
                            builder.UnsafeAppend(str, StringLib::size(str));
                            field.offset += batchRowSizeBytes * 8;
                        }
                        field.offset = starting_offset;
                    }
                    else // non-batch field
                    {
                        const char* str = batch.record->getValueText(field);
                        for(int row = 0; row < batch.rows; row++)
                        {
                            builder.UnsafeAppend(str, StringLib::size(str));
                        }
                    }
                    key = recordBatch.next(&batch);
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
        stop_trace(INFO, field_trace_id);
    }

    /* Add Geometry Column (if GeoParquet) */
    if(geoData.as_geo)
    {
        uint32_t geo_trace_id = start_trace(INFO, trace_id, "geo_column", "%s", "{}");
        RecordObject::field_t x_field = geoData.x_field;
        RecordObject::field_t y_field = geoData.y_field;
        shared_ptr<arrow::Array> column;
        arrow::BinaryBuilder builder;
        (void)builder.Reserve(num_rows);
        (void)builder.ReserveData(num_rows * sizeof(wkbpoint_t));
        unsigned long key = recordBatch.first(&batch);
        while(key != (unsigned long)INVALID_KEY)
        {
            int32_t starting_x_offset = x_field.offset;
            int32_t starting_y_offset = y_field.offset;
            for(int row = 0; row < batch.rows; row++)
            {
                wkbpoint_t point = {
                    #ifdef __be__
                    .byteOrder = 0,
                    #else
                    .byteOrder = 1,
                    #endif
                    .wkbType = 1,
                    .x = batch.record->getValueReal(x_field),
                    .y = batch.record->getValueReal(y_field)
                };
                (void)builder.UnsafeAppend((uint8_t*)&point, sizeof(wkbpoint_t));
                if(x_field.flags & RecordObject::BATCH) x_field.offset += batchRowSizeBytes * 8;
                if(x_field.flags & RecordObject::BATCH) y_field.offset += batchRowSizeBytes * 8;
            }
            x_field.offset = starting_x_offset;
            y_field.offset = starting_y_offset;
            key = recordBatch.next(&batch);
        }
        (void)builder.Finish(&column);
        columns.push_back(column);
        stop_trace(INFO, geo_trace_id);
    }

    /* Build and Write Table */
    uint32_t write_trace_id = start_trace(INFO, trace_id, "write_table", "%s", "{}");
    if(pimpl->parquetWriter)
    {
        shared_ptr<arrow::Table> table = arrow::Table::Make(pimpl->schema, columns);
        (void)pimpl->parquetWriter->WriteTable(*table, num_rows);
    }
    stop_trace(INFO, write_trace_id);

    /* Clear Record Batch */
    uint32_t clear_trace_id = start_trace(INFO, trace_id, "clear_batch", "%s", "{}");
    unsigned long key = recordBatch.first(&batch);
    while(key != (unsigned long)INVALID_KEY)
    {
        delete batch.record;
        inQ->dereference(batch.ref);
        key = recordBatch.next(&batch);
    }
    recordBatch.clear();
    stop_trace(INFO, clear_trace_id);

    /* Stop Trace */
    stop_trace(INFO, trace_id);
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
        alert(RTE_INFO, INFO, outQ, NULL, "Initiated upload of results to S3, bucket = %s, key = %s", bucket, key);

        try
        {
            /* Upload to S3 */
            int64_t bytes_uploaded = S3CurlIODriver::put(fileName, bucket, key, parms->region, &parms->credentials);

            /* Send Successful Status */
            alert(RTE_INFO, INFO, outQ, NULL, "Upload to S3 completed, bucket = %s, key = %s, size = %ld", bucket, key, bytes_uploaded);

            /* Send Remote Record */
            RecordObject remote_record(remoteRecType);
            arrow_file_remote_t* remote = (arrow_file_remote_t*)remote_record.getRecordData();
            StringLib::copy(&remote->url[0], outputPath, URL_MAX_LEN);
            remote->size = bytes_uploaded;
            if(!remote_record.post(outQ))
            {
                mlog(CRITICAL, "Failed to send remote record back to user for %s", outputPath);
            }
        }
        catch(const RunTimeException& e)
        {
            status = false;

            /* Send Error Status */
            alert(RTE_ERROR, e.level(), outQ, NULL, "Upload to S3 failed, bucket = %s, key = %s, error = %s", bucket, key, e.what());
        }
    }

    /* Clean Up */
    delete [] bucket;

    /* Return Status */
    return status;

    #else
    alert(RTE_ERROR, CRITICAL, outQ, NULL, "Output path specifies S3, but server compiled without AWS support");
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
        int rc = fclose(fp);
        if(rc != 0)
        {
            status = false;
            mlog(CRITICAL, "Failed (%d) to close file %s: %s", rc, fileName, strerror(errno));
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
