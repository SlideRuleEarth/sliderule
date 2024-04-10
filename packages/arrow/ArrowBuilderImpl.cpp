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

/*
 * The order of the columns in the parquet file are:
 *  - Fields from the primary record
 *  - Geometry
 *  - Ancillary fields
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "core.h"
#include "ArrowBuilderImpl.h"

#include <arrow/csv/writer.h>
#include <regex>

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructors
 *----------------------------------------------------------------------------*/
ArrowBuilderImpl::ArrowBuilderImpl (ArrowBuilder* _builder):
    arrowBuilder(_builder),
    schema(NULL),
    writerFormat(ArrowParms::UNSUPPORTED),
    fieldList(LIST_BLOCK_SIZE),
    firstTime(true)
{
    /* Build Field List and Iterator */
    buildFieldList(arrowBuilder->getRecType(), 0, 0);
    if(arrowBuilder->getAsGeo()) fieldVector.push_back(arrow::field("geometry", arrow::binary()));
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArrowBuilderImpl::~ArrowBuilderImpl (void)
{
}

/*----------------------------------------------------------------------------
 * processRecordBatch
 *----------------------------------------------------------------------------*/
bool ArrowBuilderImpl::processRecordBatch (batch_list_t& record_batch, int num_rows, int batch_row_size_bits, bool file_finished)
{
    bool status = false;

    /* Start Trace */
    uint32_t parent_trace_id = EventLib::grabId();
    uint32_t trace_id = start_trace(INFO, parent_trace_id, "process_batch", "{\"num_rows\": %d}", num_rows);

    /* Allocate Columns for this Batch */
    vector<shared_ptr<arrow::Array>> columns;

    /* Loop Through Fields in Primary Record */
    for(int i = 0; i < fieldList.length(); i++)
    {
        uint32_t field_trace_id = start_trace(INFO, trace_id, "append_field", "{\"field\": %d}", i);
        RecordObject::field_t& field = fieldList[i];

        /* Build Column */
        shared_ptr<arrow::Array> column;
        if(field.elements <= 1) processField(field, &column, record_batch, num_rows, batch_row_size_bits);
        else processArray(field, &column, record_batch, batch_row_size_bits);

        /* Add Column to Columns */
        columns.push_back(column);
        stop_trace(INFO, field_trace_id);
    }

    /* Add Geometry Column (if GeoParquet) */
    if(arrowBuilder->getAsGeo())
    {
        uint32_t geo_trace_id = start_trace(INFO, trace_id, "geo_column", "%s", "{}");
        shared_ptr<arrow::Array> column;
        processGeometry(arrowBuilder->getXField(), arrowBuilder->getYField(), &column, record_batch, num_rows, batch_row_size_bits);
        columns.push_back(column);
        stop_trace(INFO, geo_trace_id);
    }

    /* Add Ancillary Columns */
    if(arrowBuilder->hasAncFields())      processAncillaryFields(columns, record_batch);
    if(arrowBuilder->hasAncElements())    processAncillaryElements(columns, record_batch);

    /* Create Parquet Writer (on first time) */
    if(firstTime)
    {
        createSchema();
        firstTime = false;
    }

    if(writerFormat == ArrowParms::PARQUET)
    {
        /* Build and Write Table */
        uint32_t write_trace_id = start_trace(INFO, trace_id, "write_table", "%s", "{}");
        shared_ptr<arrow::Table> table = arrow::Table::Make(schema, columns);
        arrow::Status s = parquetWriter->WriteTable(*table, num_rows);
        if(s.ok()) status = true;
        else mlog(CRITICAL, "Failed to write parquet table: %s", s.CodeAsString().c_str());
        stop_trace(INFO, write_trace_id);

        /* Close Parquet Writer */
        if(file_finished)
        {
            (void)parquetWriter->Close();
        }
    }
    else if(writerFormat == ArrowParms::CSV)
    {
        /* Write the Table to a CSV file */
        shared_ptr<arrow::Table> table = arrow::Table::Make(schema, columns);
        arrow::Status s = arrow::csv::WriteCSV(*table, arrow::csv::WriteOptions::Defaults(), csvWriter.get());
        if(s.ok()) status = true;
        else mlog(CRITICAL, "Failed to write CSV table: %s", s.CodeAsString().c_str());

        /* Close Parquet Writer */
        if(file_finished)
        {
            (void)csvWriter.get()->Close();
        }
    }

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return Status */
    return status;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
* createSchema
*----------------------------------------------------------------------------*/
void ArrowBuilderImpl::createSchema (void)
{
    /* Create Schema */
    schema = make_shared<arrow::Schema>(fieldVector);

    if(arrowBuilder->getParms()->format == ArrowParms::PARQUET)
    {
        /* Set Arrow Output Stream */
        shared_ptr<arrow::io::FileOutputStream> file_output_stream;
        PARQUET_ASSIGN_OR_THROW(file_output_stream, arrow::io::FileOutputStream::Open(arrowBuilder->getFileName()));

        /* Set Writer Properties */
        parquet::WriterProperties::Builder writer_props_builder;
        writer_props_builder.compression(parquet::Compression::SNAPPY);
        writer_props_builder.version(parquet::ParquetVersion::PARQUET_2_6);
        shared_ptr<parquet::WriterProperties> writer_props = writer_props_builder.build();

        /* Set Arrow Writer Properties */
        auto arrow_writer_props = parquet::ArrowWriterProperties::Builder().store_schema()->build();

        /* Set MetaData */
        auto metadata = schema->metadata() ? schema->metadata()->Copy() : std::make_shared<arrow::KeyValueMetadata>();
        if(arrowBuilder->getAsGeo()) appendGeoMetaData(metadata);
        appendServerMetaData(metadata);
        appendPandasMetaData(metadata);
        schema = schema->WithMetadata(metadata);

        /* Create Parquet Writer */
        auto result = parquet::arrow::FileWriter::Open(*schema, ::arrow::default_memory_pool(), file_output_stream, writer_props, arrow_writer_props);
        if(result.ok())
        {
            parquetWriter = std::move(result).ValueOrDie();
            writerFormat = ArrowParms::PARQUET;
        }
        else
        {
            mlog(CRITICAL, "Failed to open parquet writer: %s", result.status().ToString().c_str());
        }
    }
    else if(arrowBuilder->getParms()->format == ArrowParms::CSV)
    {
        /* Create CSV Writer */
        auto result = arrow::io::FileOutputStream::Open(arrowBuilder->getFileName());
        if(result.ok())
        {
            csvWriter = result.ValueOrDie();
            writerFormat = ArrowParms::CSV;
        }
        else
        {
            mlog(CRITICAL, "Failed to open csv writer: %s", result.status().ToString().c_str());
        }
    }
    else
    {
        mlog(CRITICAL, "Unsupported format: %d", arrowBuilder->getParms()->format);
    }
}

/*----------------------------------------------------------------------------
* buildFieldList
*----------------------------------------------------------------------------*/
bool ArrowBuilderImpl::buildFieldList (const char* rec_type, int offset, int flags)
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
        if(arrowBuilder->getAsGeo())
        {
            /* skip over source columns for geometry as they will be added
             * separately as a part of the dedicated geometry column */
            if(field.flags & (RecordObject::X_COORD | RecordObject::Y_COORD))
            {
                continue;
            }
        }

        /* Add to Schema */
        if(field.elements == 1)
        {
            switch(field.type)
            {
                case RecordObject::INT8:    fieldVector.push_back(arrow::field(field_name, arrow::int8()));       break;
                case RecordObject::INT16:   fieldVector.push_back(arrow::field(field_name, arrow::int16()));      break;
                case RecordObject::INT32:   fieldVector.push_back(arrow::field(field_name, arrow::int32()));      break;
                case RecordObject::INT64:   fieldVector.push_back(arrow::field(field_name, arrow::int64()));      break;
                case RecordObject::UINT8:   fieldVector.push_back(arrow::field(field_name, arrow::uint8()));      break;
                case RecordObject::UINT16:  fieldVector.push_back(arrow::field(field_name, arrow::uint16()));     break;
                case RecordObject::UINT32:  fieldVector.push_back(arrow::field(field_name, arrow::uint32()));     break;
                case RecordObject::UINT64:  fieldVector.push_back(arrow::field(field_name, arrow::uint64()));     break;
                case RecordObject::FLOAT:   fieldVector.push_back(arrow::field(field_name, arrow::float32()));    break;
                case RecordObject::DOUBLE:  fieldVector.push_back(arrow::field(field_name, arrow::float64()));    break;
                case RecordObject::TIME8:   fieldVector.push_back(arrow::field(field_name, arrow::timestamp(arrow::TimeUnit::NANO))); break;
                case RecordObject::STRING:  fieldVector.push_back(arrow::field(field_name, arrow::utf8()));       break;

                case RecordObject::USER:    buildFieldList(field.exttype, field.offset, field.flags);
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
                case RecordObject::INT8:    fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::int8())));      break;
                case RecordObject::INT16:   fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::int16())));     break;
                case RecordObject::INT32:   fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::int32())));     break;
                case RecordObject::INT64:   fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::int64())));     break;
                case RecordObject::UINT8:   fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::uint8())));     break;
                case RecordObject::UINT16:  fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::uint16())));    break;
                case RecordObject::UINT32:  fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::uint32())));    break;
                case RecordObject::UINT64:  fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::uint64())));    break;
                case RecordObject::FLOAT:   fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::float32())));   break;
                case RecordObject::DOUBLE:  fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::float64())));   break;
                case RecordObject::TIME8:   fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::timestamp(arrow::TimeUnit::NANO)))); break;
                case RecordObject::STRING:  fieldVector.push_back(arrow::field(field_name, arrow::list(arrow::utf8())));      break;

                case RecordObject::USER:    if(field.flags & RecordObject::BATCH) buildFieldList(field.exttype, field.offset, field.flags);
                                            else mlog(CRITICAL, "User fields that are arrays must be identified as batches: %s", field.exttype);
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
            fieldList.add(column_field);
        }
    }

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
* appendGeoMetaData
*----------------------------------------------------------------------------*/
void ArrowBuilderImpl::appendGeoMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata)
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
void ArrowBuilderImpl::appendServerMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata)
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
            packagestr += FString("\"%s\"", pkg_list[index]).c_str();
            if(pkg_list[index + 1]) packagestr += ", ";
            delete [] pkg_list[index];
            index++;
        }
    }
    packagestr += "]";
    delete [] pkg_list;

    /* Initialize Meta Data String */
    string metastr(FString(R"json({
        "server":
        {
            "environment":"%s",
            "version":"%s",
            "duration":%s,
            "packages":%s,
            "commit":"%s",
            "launch":"%s"
        },
        "recordinfo":
        {
            "time": "%s",
            "as_geo": %s,
            "x": "%s",
            "y": "%s"
        }
    })json",
        OsApi::getEnvVersion(),
        LIBID,
        durationstr.c_str(),
        packagestr.c_str(),
        BUILDINFO,
        timestr.c_str(),
        arrowBuilder->getTimeKey(),
        arrowBuilder->getAsGeo() ? "true" : "false",
        arrowBuilder->getXKey(),
        arrowBuilder->getYKey()).c_str());

    /* Clean Up JSON String */
    metastr = std::regex_replace(metastr, std::regex("    "), "");
    metastr = std::regex_replace(metastr, std::regex("\n"), " ");

    /* Append Meta String */
    metadata->Append("sliderule", metastr.c_str());
}

/*----------------------------------------------------------------------------
* appendPandasMetaData
*----------------------------------------------------------------------------*/
void ArrowBuilderImpl::appendPandasMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata)
{
    /* Initialize Pandas Meta Data String */
    string pandasstr(R"json({
        "index_columns": ["_INDEX_"],
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
    int column_index = 0;
    int num_columns = schema->fields().size();
    for(const shared_ptr<arrow::Field>& field: schema->fields())
    {
        const string& field_name = field->name();
        const shared_ptr<arrow::DataType>& field_type = field->type();

        /* Initialize Column String */
        string columnstr(R"json({"name": "_NAME_", "field_name": "_NAME_", "pandas_type": "_PTYPE_", "numpy_type": "_NTYPE_", "metadata": null})json");
        const char* pandas_type = "";
        const char* numpy_type = "";
        bool is_last_entry = false;

        if      (field_type->Equals(arrow::float64()))      { pandas_type = "float64";    numpy_type = "float64";           }
        else if (field_type->Equals(arrow::float32()))      { pandas_type = "float32";    numpy_type = "float32";           }
        else if (field_type->Equals(arrow::int8()))         { pandas_type = "int8";       numpy_type = "int8";              }
        else if (field_type->Equals(arrow::int16()))        { pandas_type = "int16";      numpy_type = "int16";             }
        else if (field_type->Equals(arrow::int32()))        { pandas_type = "int32";      numpy_type = "int32";             }
        else if (field_type->Equals(arrow::int64()))        { pandas_type = "int64";      numpy_type = "int64";             }
        else if (field_type->Equals(arrow::uint8()))        { pandas_type = "uint8";      numpy_type = "uint8";             }
        else if (field_type->Equals(arrow::uint16()))       { pandas_type = "uint16";     numpy_type = "uint16";            }
        else if (field_type->Equals(arrow::uint32()))       { pandas_type = "uint32";     numpy_type = "uint32";            }
        else if (field_type->Equals(arrow::uint64()))       { pandas_type = "uint64";     numpy_type = "uint64";            }
        else if (field_type->Equals(arrow::timestamp(arrow::TimeUnit::NANO)))
                                                            { pandas_type = "datetime";   numpy_type = "datetime64[ns]";    }
        else if (field_type->Equals(arrow::utf8()))         { pandas_type = "bytes";      numpy_type = "object";            }
        else if (field_type->Equals(arrow::binary()))       { pandas_type = "bytes";      numpy_type = "object";            }
        else                                                { pandas_type = "bytes";      numpy_type = "object";            }

        /* Mark Last Column */
        if(++column_index == num_columns)
        {
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

    /* Fill In Pandas Meta Data String */
    pandasstr = std::regex_replace(pandasstr, std::regex("    "), "");
    pandasstr = std::regex_replace(pandasstr, std::regex("\n"), " ");
    pandasstr = std::regex_replace(pandasstr, std::regex("_INDEX_"), arrowBuilder->getTimeKey());
    pandasstr = std::regex_replace(pandasstr, std::regex("_COLUMNS_"), columns.c_str());

    /* Append Meta String */
    metadata->Append("pandas", pandasstr.c_str());
}

/*----------------------------------------------------------------------------
* processField
*----------------------------------------------------------------------------*/
void ArrowBuilderImpl::processField (RecordObject::field_t& field, shared_ptr<arrow::Array>* column, batch_list_t& record_batch, int num_rows, int batch_row_size_bits)
{
    switch(field.type)
    {
        case RecordObject::DOUBLE:
        {
            arrow::DoubleBuilder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((double)batch->pri_record->getValueReal(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    double value = (double)batch->pri_record->getValueReal(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::FLOAT:
        {
            arrow::FloatBuilder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((float)batch->pri_record->getValueReal(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    float value = (float)batch->pri_record->getValueReal(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::INT8:
        {
            arrow::Int8Builder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((int8_t)batch->pri_record->getValueInteger(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    int8_t value = (int8_t)batch->pri_record->getValueInteger(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::INT16:
        {
            arrow::Int16Builder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((int16_t)batch->pri_record->getValueInteger(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    int16_t value = (int16_t)batch->pri_record->getValueInteger(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::INT32:
        {
            arrow::Int32Builder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((int32_t)batch->pri_record->getValueInteger(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    int32_t value = (int32_t)batch->pri_record->getValueInteger(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::INT64:
        {
            arrow::Int64Builder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((int64_t)batch->pri_record->getValueInteger(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    int64_t value = (int64_t)batch->pri_record->getValueInteger(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::UINT8:
        {
            arrow::UInt8Builder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((uint8_t)batch->pri_record->getValueInteger(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    uint8_t value = (uint8_t)batch->pri_record->getValueInteger(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::UINT16:
        {
            arrow::UInt16Builder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((uint16_t)batch->pri_record->getValueInteger(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    uint16_t value = (uint16_t)batch->pri_record->getValueInteger(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::UINT32:
        {
            arrow::UInt32Builder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((uint32_t)batch->pri_record->getValueInteger(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    uint32_t value = (uint32_t)batch->pri_record->getValueInteger(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::UINT64:
        {
            arrow::UInt64Builder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((uint64_t)batch->pri_record->getValueInteger(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    uint64_t value = (uint64_t)batch->pri_record->getValueInteger(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::TIME8:
        {
            arrow::TimestampBuilder builder(arrow::timestamp(arrow::TimeUnit::NANO), arrow::default_memory_pool());
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend((int64_t)batch->pri_record->getValueInteger(field));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    int64_t value = (int64_t)batch->pri_record->getValueInteger(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(value);
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        case RecordObject::STRING:
        {
            arrow::StringBuilder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                if(field.flags & RecordObject::BATCH)
                {
                    int32_t starting_offset = field.offset;
                    for(int row = 0; row < batch->rows; row++)
                    {
                        const char* str = batch->pri_record->getValueText(field);
                        builder.UnsafeAppend(str, StringLib::size(str));
                        field.offset += batch_row_size_bits;
                    }
                    field.offset = starting_offset;
                }
                else // non-batch field
                {
                    const char* str = batch->pri_record->getValueText(field);
                    for(int row = 0; row < batch->rows; row++)
                    {
                        builder.UnsafeAppend(str, StringLib::size(str));
                    }
                }
            }
            (void)builder.Finish(column);
            break;
        }

        default:
        {
            break;
        }
    }
}

/*----------------------------------------------------------------------------
* processArray
*----------------------------------------------------------------------------*/
void ArrowBuilderImpl::processArray (RecordObject::field_t& field, shared_ptr<arrow::Array>* column, batch_list_t& record_batch, int batch_row_size_bits)
{
    if(!(field.flags & RecordObject::BATCH))
    {
        batch_row_size_bits = 0;
    }

    switch(field.type)
    {
        case RecordObject::DOUBLE:
        {
            auto builder = make_shared<arrow::DoubleBuilder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((double)batch->pri_record->getValueReal(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::FLOAT:
        {
            auto builder = make_shared<arrow::FloatBuilder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((float)batch->pri_record->getValueReal(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::INT8:
        {
            auto builder = make_shared<arrow::Int8Builder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((int8_t)batch->pri_record->getValueInteger(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::INT16:
        {
            auto builder = make_shared<arrow::Int16Builder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((int16_t)batch->pri_record->getValueInteger(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::INT32:
        {
            auto builder = make_shared<arrow::Int32Builder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((int32_t)batch->pri_record->getValueInteger(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::INT64:
        {
            auto builder = make_shared<arrow::Int64Builder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((int64_t)batch->pri_record->getValueInteger(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::UINT8:
        {
            auto builder = make_shared<arrow::UInt8Builder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((uint8_t)batch->pri_record->getValueInteger(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::UINT16:
        {
            auto builder = make_shared<arrow::UInt16Builder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((uint16_t)batch->pri_record->getValueInteger(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::UINT32:
        {
            auto builder = make_shared<arrow::UInt32Builder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((uint32_t)batch->pri_record->getValueInteger(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::UINT64:
        {
            auto builder = make_shared<arrow::UInt64Builder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((uint64_t)batch->pri_record->getValueInteger(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::TIME8:
        {
            auto builder = make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::NANO), arrow::default_memory_pool());
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        (void)builder->Append((int64_t)batch->pri_record->getValueInteger(field, element));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        case RecordObject::STRING:
        {
            auto builder = make_shared<arrow::StringBuilder>();
            arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ArrowBuilder::batch_t* batch = record_batch[i];
                int32_t starting_offset = field.offset;
                for(int row = 0; row < batch->rows; row++)
                {
                    (void)list_builder.Append();
                    for(int element = 0; element < field.elements; element++)
                    {
                        const char* str = batch->pri_record->getValueText(field, NULL, element);
                        (void)builder->Append(str, StringLib::size(str));
                    }
                    field.offset += batch_row_size_bits;
                }
                field.offset = starting_offset;
            }
            (void)list_builder.Finish(column);
            break;
        }

        default:
        {
            break;
        }
    }
}

/*----------------------------------------------------------------------------
* processGeometry
*----------------------------------------------------------------------------*/
void ArrowBuilderImpl::processGeometry (RecordObject::field_t& x_field, RecordObject::field_t& y_field, shared_ptr<arrow::Array>* column, batch_list_t& record_batch, int num_rows, int batch_row_size_bits)
{
    arrow::BinaryBuilder builder;
    (void)builder.Reserve(num_rows);
    (void)builder.ReserveData(num_rows * sizeof(wkbpoint_t));
    for(int i = 0; i < record_batch.length(); i++)
    {
        ArrowBuilder::batch_t* batch = record_batch[i];
        int32_t starting_x_offset = x_field.offset;
        int32_t starting_y_offset = y_field.offset;
        for(int row = 0; row < batch->rows; row++)
        {
            wkbpoint_t point = {
                #ifdef __be__
                .byteOrder = 0,
                #else
                .byteOrder = 1,
                #endif
                .wkbType = 1,
                .x = batch->pri_record->getValueReal(x_field),
                .y = batch->pri_record->getValueReal(y_field)
            };
            (void)builder.UnsafeAppend((uint8_t*)&point, sizeof(wkbpoint_t));
            if(x_field.flags & RecordObject::BATCH) x_field.offset += batch_row_size_bits;
            if(y_field.flags & RecordObject::BATCH) y_field.offset += batch_row_size_bits;
        }
        x_field.offset = starting_x_offset;
        y_field.offset = starting_y_offset;
    }
    (void)builder.Finish(column);
}

/*----------------------------------------------------------------------------
* processAncillaryFields
*----------------------------------------------------------------------------*/
void ArrowBuilderImpl::processAncillaryFields (vector<shared_ptr<arrow::Array>>& columns, batch_list_t& record_batch)
{
    vector<string>& ancillary_fields = arrowBuilder->getParms()->ancillary_fields;
    Dictionary<vector<AncillaryFields::field_t*>> field_table;
    Dictionary<RecordObject::fieldType_t> field_type_table;

    /* Initialize Field Table */
    for(size_t i = 0; i < ancillary_fields.size(); i++)
    {
        const char* name = ancillary_fields[i].c_str();
        vector<AncillaryFields::field_t*> field_vec;
        field_table.add(name, field_vec);
    }

    /* Populate Field Table */
    for(int i = 0; i < record_batch.length(); i++)
    {
        ArrowBuilder::batch_t* batch = record_batch[i];

        /* Loop through Ancillary Fields */
        for(int j = 0; j < batch->num_anc_recs; j++)
        {
            AncillaryFields::field_array_t* field_array = reinterpret_cast<AncillaryFields::field_array_t*>(batch->anc_records[j]->getRecordData());
            for(uint32_t k = 0; k < field_array->num_fields; k++)
            {
                /* Get Name */
                const char* name = NULL;
                uint8_t field_index = field_array->fields[k].field_index;
                if(field_index < ancillary_fields.size()) name = ancillary_fields[field_index].c_str();
                else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid field index: %d", field_index);

                /* Add to Field Table */
                field_table[name].push_back(&(field_array->fields[k]));
                field_type_table.add(name, static_cast<RecordObject::fieldType_t>(field_array->fields[k].data_type), false);
            }
        }
    }

    /* Loop Through Fields */
    for(size_t i = 0; i < ancillary_fields.size(); i++)
    {
        const char* name = ancillary_fields[i].c_str();
        vector<AncillaryFields::field_t*>& field_vec = field_table[name];
        RecordObject::fieldType_t type = field_type_table[name];
        int num_rows = field_vec.size();

        /* Populate Schema */
        if(firstTime)
        {
            switch(type)
            {
                case RecordObject::INT8:    fieldVector.push_back(arrow::field(name, arrow::int8()));       break;
                case RecordObject::INT16:   fieldVector.push_back(arrow::field(name, arrow::int16()));      break;
                case RecordObject::INT32:   fieldVector.push_back(arrow::field(name, arrow::int32()));      break;
                case RecordObject::INT64:   fieldVector.push_back(arrow::field(name, arrow::int64()));      break;
                case RecordObject::UINT8:   fieldVector.push_back(arrow::field(name, arrow::uint8()));      break;
                case RecordObject::UINT16:  fieldVector.push_back(arrow::field(name, arrow::uint16()));     break;
                case RecordObject::UINT32:  fieldVector.push_back(arrow::field(name, arrow::uint32()));     break;
                case RecordObject::UINT64:  fieldVector.push_back(arrow::field(name, arrow::uint64()));     break;
                case RecordObject::FLOAT:   fieldVector.push_back(arrow::field(name, arrow::float32()));    break;
                case RecordObject::DOUBLE:  fieldVector.push_back(arrow::field(name, arrow::float64()));    break;
                case RecordObject::TIME8:   fieldVector.push_back(arrow::field(name, arrow::timestamp(arrow::TimeUnit::NANO))); break;
                default:                    break;
            }
        }

        /* Populate Column */
        shared_ptr<arrow::Array> column;
        switch(type)
        {
            case RecordObject::DOUBLE:
            {
                arrow::DoubleBuilder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    double* val_ptr = AncillaryFields::getValueAsDouble(field->value);
                    builder.UnsafeAppend((double)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::FLOAT:
            {
                arrow::FloatBuilder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    float* val_ptr = AncillaryFields::getValueAsFloat(field->value);
                    builder.UnsafeAppend((float)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT8:
            {
                arrow::Int8Builder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    int64_t* val_ptr = AncillaryFields::getValueAsInteger(field->value);
                    builder.UnsafeAppend((int8_t)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT16:
            {
                arrow::Int16Builder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    int64_t* val_ptr = AncillaryFields::getValueAsInteger(field->value);
                    builder.UnsafeAppend((int16_t)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT32:
            {
                arrow::Int32Builder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    int64_t* val_ptr = AncillaryFields::getValueAsInteger(field->value);
                    builder.UnsafeAppend((int32_t)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT64:
            {
                arrow::Int64Builder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    int64_t* val_ptr = AncillaryFields::getValueAsInteger(field->value);
                    builder.UnsafeAppend((int64_t)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT8:
            {
                arrow::UInt8Builder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    int64_t* val_ptr = AncillaryFields::getValueAsInteger(field->value);
                    builder.UnsafeAppend((uint8_t)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT16:
            {
                arrow::UInt16Builder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    int64_t* val_ptr = AncillaryFields::getValueAsInteger(field->value);
                    builder.UnsafeAppend((uint16_t)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT32:
            {
                arrow::UInt32Builder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    int64_t* val_ptr = AncillaryFields::getValueAsInteger(field->value);
                    builder.UnsafeAppend((uint32_t)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT64:
            {
                arrow::UInt64Builder builder;
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    int64_t* val_ptr = AncillaryFields::getValueAsInteger(field->value);
                    builder.UnsafeAppend((uint64_t)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::TIME8:
            {
                arrow::TimestampBuilder builder(arrow::timestamp(arrow::TimeUnit::NANO), arrow::default_memory_pool());
                (void)builder.Reserve(num_rows);
                for(int j = 0; j < num_rows; j++)
                {
                    AncillaryFields::field_t* field = field_vec[j];
                    int64_t* val_ptr = AncillaryFields::getValueAsInteger(field->value);
                    builder.UnsafeAppend((int64_t)*val_ptr);
                }
                (void)builder.Finish(&column);
                break;
            }

            default:
            {
                break;
            }
        }

        /* Add to Columns */
        columns.push_back(column);
    }
}

/*----------------------------------------------------------------------------
* processAncillaryElements
*----------------------------------------------------------------------------*/
void ArrowBuilderImpl::processAncillaryElements (vector<shared_ptr<arrow::Array>>& columns, batch_list_t& record_batch)
{
    int num_rows = 0;
    vector<string>& ancillary_fields = arrowBuilder->getParms()->ancillary_fields;
    Dictionary<vector<AncillaryFields::element_array_t*>> element_table;
    Dictionary<RecordObject::fieldType_t> element_type_table;

    /* Initialize Field Table */
    for(size_t i = 0; i < ancillary_fields.size(); i++)
    {
        const char* name = ancillary_fields[i].c_str();
        vector<AncillaryFields::element_array_t*> element_vec;
        element_table.add(name, element_vec);
    }

    /* Populate Field Table */
    for(int i = 0; i < record_batch.length(); i++)
    {
        ArrowBuilder::batch_t* batch = record_batch[i];

        /* Loop through Ancillary Elements */
        for(int j = 0; j < batch->num_anc_recs; j++)
        {
            AncillaryFields::element_array_t* element_array = reinterpret_cast<AncillaryFields::element_array_t*>(batch->anc_records[j]->getRecordData());

            /* Get Name */
            const char* name = NULL;
            if(element_array->field_index < ancillary_fields.size()) name = ancillary_fields[element_array->field_index].c_str();
            else throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid field index: %d", element_array->field_index);

            /* Add to Element Table */
            element_table[name].push_back(element_array);
            element_type_table.add(name, static_cast<RecordObject::fieldType_t>(element_array->data_type), false);
            num_rows += element_array->num_elements;
        }
    }

    /* Loop Through Fields */
    for(size_t i = 0; i < ancillary_fields.size(); i++)
    {
        const char* name = ancillary_fields[i].c_str();
        vector<AncillaryFields::element_array_t*>& element_vec = element_table[name];
        RecordObject::fieldType_t type = element_type_table[name];

        /* Populate Schema */
        if(firstTime)
        {
            switch(type)
            {
                case RecordObject::INT8:    fieldVector.push_back(arrow::field(name, arrow::int8()));       break;
                case RecordObject::INT16:   fieldVector.push_back(arrow::field(name, arrow::int16()));      break;
                case RecordObject::INT32:   fieldVector.push_back(arrow::field(name, arrow::int32()));      break;
                case RecordObject::INT64:   fieldVector.push_back(arrow::field(name, arrow::int64()));      break;
                case RecordObject::UINT8:   fieldVector.push_back(arrow::field(name, arrow::uint8()));      break;
                case RecordObject::UINT16:  fieldVector.push_back(arrow::field(name, arrow::uint16()));     break;
                case RecordObject::UINT32:  fieldVector.push_back(arrow::field(name, arrow::uint32()));     break;
                case RecordObject::UINT64:  fieldVector.push_back(arrow::field(name, arrow::uint64()));     break;
                case RecordObject::FLOAT:   fieldVector.push_back(arrow::field(name, arrow::float32()));    break;
                case RecordObject::DOUBLE:  fieldVector.push_back(arrow::field(name, arrow::float64()));    break;
                case RecordObject::TIME8:   fieldVector.push_back(arrow::field(name, arrow::timestamp(arrow::TimeUnit::NANO))); break;
                default:                    break;
            }
        }

        /* Populate Column */
        shared_ptr<arrow::Array> column;
        switch(type)
        {
            case RecordObject::DOUBLE:
            {
                arrow::DoubleBuilder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    double* src = AncillaryFields::getValueAsDouble(&element_array->data[0]);
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::FLOAT:
            {
                arrow::FloatBuilder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    float* src = AncillaryFields::getValueAsFloat(&element_array->data[0]);
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT8:
            {
                arrow::Int8Builder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    int8_t* src = (int8_t*)&element_array->data[0];
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT16:
            {
                arrow::Int16Builder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    int16_t* src = (int16_t*)&element_array->data[0];
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT32:
            {
                arrow::Int32Builder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    int32_t* src = (int32_t*)&element_array->data[0];
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::INT64:
            {
                arrow::Int64Builder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    int64_t* src = (int64_t*)&element_array->data[0];
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT8:
            {
                arrow::UInt8Builder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    uint8_t* src = (uint8_t*)&element_array->data[0];
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT16:
            {
                arrow::UInt16Builder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    uint16_t* src = (uint16_t*)&element_array->data[0];
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT32:
            {
                arrow::UInt32Builder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    uint32_t* src = (uint32_t*)&element_array->data[0];
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::UINT64:
            {
                arrow::UInt64Builder builder;
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    uint64_t* src = (uint64_t*)&element_array->data[0];
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            case RecordObject::TIME8:
            {
                arrow::TimestampBuilder builder(arrow::timestamp(arrow::TimeUnit::NANO), arrow::default_memory_pool());
                (void)builder.Reserve(num_rows);
                for(size_t j = 0; j < element_vec.size(); j++)
                {
                    AncillaryFields::element_array_t* element_array = element_vec[j];
                    int64_t* src = (int64_t*)&element_array->data[0];
                    for(uint32_t k = 0; k < element_array->num_elements; k++)
                    {
                        builder.UnsafeAppend(src[k]);
                    }
                }
                (void)builder.Finish(&column);
                break;
            }

            default:
            {
                break;
            }
        }

        /* Add to Columns */
        columns.push_back(column);
    }
}