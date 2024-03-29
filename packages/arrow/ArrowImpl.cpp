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
#include "ParquetBuilder.h"
#include "ArrowParms.h"
#include "ArrowImpl.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructors
 *----------------------------------------------------------------------------*/
ArrowImpl::ArrowImpl (ParquetBuilder* _builder):
    parquetBuilder(_builder),
    schema(NULL),
    writerFormat(ArrowParms::UNSUPPORTED),
    fieldList(LIST_BLOCK_SIZE),
    firstTime(true)
{
    if(parquetBuilder)
    {
        /* Build Field List and Iterator */
        buildFieldList(parquetBuilder->getRecType(), 0, 0);
        if(parquetBuilder->getAsGeo()) fieldVector.push_back(arrow::field("geometry", arrow::binary()));
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArrowImpl::~ArrowImpl (void)
{
}

/*----------------------------------------------------------------------------
 * processRecordBatch
 *----------------------------------------------------------------------------*/
bool ArrowImpl::processRecordBatch (batch_list_t& record_batch, int num_rows, int batch_row_size_bits, bool file_finished)
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
    if(parquetBuilder->getAsGeo())
    {
        uint32_t geo_trace_id = start_trace(INFO, trace_id, "geo_column", "%s", "{}");
        shared_ptr<arrow::Array> column;
        processGeometry(parquetBuilder->getXField(), parquetBuilder->getYField(), &column, record_batch, num_rows, batch_row_size_bits);
        columns.push_back(column);
        stop_trace(INFO, geo_trace_id);
    }

    /* Add Ancillary Columns */
    if(parquetBuilder->hasAncFields())      processAncillaryFields(columns, record_batch);
    if(parquetBuilder->hasAncElements())    processAncillaryElements(columns, record_batch);

    /* Create Parquet Writer (on first time) */
    if(firstTime)
    {
        writerFormat = createSchema();
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


/*----------------------------------------------------------------------------
* getpoints
*----------------------------------------------------------------------------*/
void ArrowImpl::getPointsFromFile(const char* file_path, std::vector<OGRPoint>& points)
{
    std::shared_ptr<arrow::Table> table = parquetFileToTable(file_path);
    auto geometry_column_index = table->schema()->GetFieldIndex("geometry");
    if(geometry_column_index == -1)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "Geometry column not found.");
    }

    auto geometry_column = std::static_pointer_cast<arrow::BinaryArray>(table->column(geometry_column_index)->chunk(0));
    mlog(DEBUG, "Geometry column elements: %ld", geometry_column->length());

    /* The geometry column is binary type */
    auto binary_array = std::static_pointer_cast<arrow::BinaryArray>(geometry_column);

    /* Iterate over each item in the geometry column */
    for(int64_t item = 0; item < binary_array->length(); item++)
    {
        auto wkb_data = binary_array->GetString(item);     /* Get WKB data as string (binary data) */
        OGRPoint poi = convertWKBToPoint(wkb_data);
        points.push_back(poi);
    }
}

/*----------------------------------------------------------------------------
* createParquetFile
*----------------------------------------------------------------------------*/
void ArrowImpl::createParquetFile(const char* input_file, const char* output_file, const std::vector<ParquetSampler::sampler_t*>& _samplers)
{
    std::ignore = output_file;
    std::ignore = _samplers;

    /* Read the input file */
    std::shared_ptr<arrow::Table> table = parquetFileToTable(input_file);
    auto pool = arrow::default_memory_pool();

    std::vector<std::shared_ptr<arrow::Field>> fields = table->schema()->fields();
    std::vector<std::shared_ptr<arrow::ChunkedArray>> columns = table->columns();

    for(ParquetSampler::sampler_t* sampler : _samplers)
    {
        RasterObject* robj = sampler->robj;

        /* Create list builders for the new columns */
        arrow::ListBuilder value_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
        auto value_builder = static_cast<arrow::DoubleBuilder*>(value_list_builder.value_builder());

        arrow::ListBuilder time_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
        auto time_builder = static_cast<arrow::DoubleBuilder*>(time_list_builder.value_builder());

        arrow::ListBuilder flags_list_builder(pool, std::make_shared<arrow::UInt32Builder>());
        auto flags_builder = static_cast<arrow::UInt32Builder*>(flags_list_builder.value_builder());

        arrow::ListBuilder fileid_list_builder(pool, std::make_shared<arrow::UInt64Builder>());
        auto fileid_builder = static_cast<arrow::UInt64Builder*>(fileid_list_builder.value_builder());

        /* Create list builders for zonal stats */
        arrow::ListBuilder count_list_builder(pool, std::make_shared<arrow::UInt32Builder>());
        auto count_builder = static_cast<arrow::UInt32Builder*>(count_list_builder.value_builder());

        arrow::ListBuilder min_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
        auto min_builder = static_cast<arrow::DoubleBuilder*>(min_list_builder.value_builder());

        arrow::ListBuilder max_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
        auto max_builder = static_cast<arrow::DoubleBuilder*>(max_list_builder.value_builder());

        arrow::ListBuilder mean_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
        auto mean_builder = static_cast<arrow::DoubleBuilder*>(mean_list_builder.value_builder());

        arrow::ListBuilder median_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
        auto median_builder = static_cast<arrow::DoubleBuilder*>(median_list_builder.value_builder());

        arrow::ListBuilder stdev_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
        auto stdev_builder = static_cast<arrow::DoubleBuilder*>(stdev_list_builder.value_builder());

        arrow::ListBuilder mad_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
        auto mad_builder = static_cast<arrow::DoubleBuilder*>(mad_list_builder.value_builder());

        /* Reserve memory for the new columns */
        PARQUET_THROW_NOT_OK(value_list_builder.Reserve(table->num_rows()));
        PARQUET_THROW_NOT_OK(time_list_builder.Reserve(table->num_rows()));
        if(robj->hasFlags())
        {
            PARQUET_THROW_NOT_OK(flags_list_builder.Reserve(table->num_rows()));
        }
        PARQUET_THROW_NOT_OK(fileid_list_builder.Reserve(table->num_rows()));
        if(robj->hasZonalStats())
        {
            PARQUET_THROW_NOT_OK(count_list_builder.Reserve(table->num_rows()));
            PARQUET_THROW_NOT_OK(min_list_builder.Reserve(table->num_rows()));
            PARQUET_THROW_NOT_OK(max_list_builder.Reserve(table->num_rows()));
            PARQUET_THROW_NOT_OK(mean_list_builder.Reserve(table->num_rows()));
            PARQUET_THROW_NOT_OK(median_list_builder.Reserve(table->num_rows()));
            PARQUET_THROW_NOT_OK(stdev_list_builder.Reserve(table->num_rows()));
            PARQUET_THROW_NOT_OK(mad_list_builder.Reserve(table->num_rows()));
        }

        /* Iterate over each sample in a vector of lists of samples */
        for(ParquetSampler::sample_list_t* slist : sampler->samples)
        {
            /* Start a new lists for each RasterSample */
            PARQUET_THROW_NOT_OK(value_list_builder.Append());
            PARQUET_THROW_NOT_OK(time_list_builder.Append());
            if(robj->hasFlags())
            {
                PARQUET_THROW_NOT_OK(flags_list_builder.Append());
            }
            PARQUET_THROW_NOT_OK(fileid_list_builder.Append());
            if(robj->hasZonalStats())
            {
                PARQUET_THROW_NOT_OK(count_list_builder.Append());
                PARQUET_THROW_NOT_OK(min_list_builder.Append());
                PARQUET_THROW_NOT_OK(max_list_builder.Append());
                PARQUET_THROW_NOT_OK(mean_list_builder.Append());
                PARQUET_THROW_NOT_OK(median_list_builder.Append());
                PARQUET_THROW_NOT_OK(stdev_list_builder.Append());
                PARQUET_THROW_NOT_OK(mad_list_builder.Append());
            }

            /* Iterate over each sample */
            for(RasterSample* sample : *slist)
            {
                /* Append the value to the value list */
                PARQUET_THROW_NOT_OK(value_builder->Append(sample->value));
                PARQUET_THROW_NOT_OK(time_builder->Append(sample->time));
                if(robj->hasFlags())
                {
                    PARQUET_THROW_NOT_OK(flags_builder->Append(sample->flags));
                }
                PARQUET_THROW_NOT_OK(fileid_builder->Append(sample->fileId));
                if(robj->hasZonalStats())
                {
                    PARQUET_THROW_NOT_OK(count_builder->Append(sample->stats.count));
                    PARQUET_THROW_NOT_OK(min_builder->Append(sample->stats.min));
                    PARQUET_THROW_NOT_OK(max_builder->Append(sample->stats.max));
                    PARQUET_THROW_NOT_OK(mean_builder->Append(sample->stats.mean));
                    PARQUET_THROW_NOT_OK(median_builder->Append(sample->stats.median));
                    PARQUET_THROW_NOT_OK(stdev_builder->Append(sample->stats.stdev));
                    PARQUET_THROW_NOT_OK(mad_builder->Append(sample->stats.mad));
                }
            }
        }

        /* Finish the list builders */
        std::shared_ptr<arrow::Array> value_list_array, time_list_array, fileid_list_array, flags_list_array;
        std::shared_ptr<arrow::Array> count_list_array, min_list_array, max_list_array, mean_list_array, median_list_array, stdev_list_array, mad_list_array;

        PARQUET_THROW_NOT_OK(value_list_builder.Finish(&value_list_array));
        PARQUET_THROW_NOT_OK(time_list_builder.Finish(&time_list_array));
        if(robj->hasFlags())
        {
            PARQUET_THROW_NOT_OK(flags_list_builder.Finish(&flags_list_array));
        }
        PARQUET_THROW_NOT_OK(fileid_list_builder.Finish(&fileid_list_array));
        if(robj->hasZonalStats())
        {
            PARQUET_THROW_NOT_OK(count_list_builder.Finish(&count_list_array));
            PARQUET_THROW_NOT_OK(min_list_builder.Finish(&min_list_array));
            PARQUET_THROW_NOT_OK(max_list_builder.Finish(&max_list_array));
            PARQUET_THROW_NOT_OK(mean_list_builder.Finish(&mean_list_array));
            PARQUET_THROW_NOT_OK(median_list_builder.Finish(&median_list_array));
            PARQUET_THROW_NOT_OK(stdev_list_builder.Finish(&stdev_list_array));
            PARQUET_THROW_NOT_OK(mad_list_builder.Finish(&mad_list_array));
        }

        std::string prefix = robj->getAssetName();

        /* Create fields for the new columns */
        auto value_field  = std::make_shared<arrow::Field>(prefix + ".value",  arrow::list(arrow::float64()));
        auto time_field   = std::make_shared<arrow::Field>(prefix + ".time",   arrow::list(arrow::float64()));
        auto flags_field  = std::make_shared<arrow::Field>(prefix + ".flags",  arrow::list(arrow::uint32()));
        auto fileid_field = std::make_shared<arrow::Field>(prefix + ".fileid", arrow::list(arrow::uint64()));

        auto count_field  = std::make_shared<arrow::Field>(prefix + ".stats.count",  arrow::list(arrow::uint32()));
        auto min_field    = std::make_shared<arrow::Field>(prefix + ".stats.min",    arrow::list(arrow::float64()));
        auto max_field    = std::make_shared<arrow::Field>(prefix + ".stats.max",    arrow::list(arrow::float64()));
        auto mean_field   = std::make_shared<arrow::Field>(prefix + ".stats.mean",   arrow::list(arrow::float64()));
        auto median_field = std::make_shared<arrow::Field>(prefix + ".stats.median", arrow::list(arrow::float64()));
        auto stdev_field  = std::make_shared<arrow::Field>(prefix + ".stats.stdev",  arrow::list(arrow::float64()));
        auto mad_field    = std::make_shared<arrow::Field>(prefix + ".stats.mad",    arrow::list(arrow::float64()));

        /* Add new column fields to the schema */
        fields.push_back(value_field);
        fields.push_back(time_field);
        if(robj->hasFlags())
        {
            fields.push_back(flags_field);
        }
        fields.push_back(fileid_field);
        if(robj->hasZonalStats())
        {
            fields.push_back(count_field);
            fields.push_back(min_field);
            fields.push_back(max_field);
            fields.push_back(mean_field);
            fields.push_back(median_field);
            fields.push_back(stdev_field);
            fields.push_back(mad_field);
        }

        /* Add new columns */
        columns.push_back(std::make_shared<arrow::ChunkedArray>(value_list_array));
        columns.push_back(std::make_shared<arrow::ChunkedArray>(time_list_array));
        if(robj->hasFlags())
        {
            columns.push_back(std::make_shared<arrow::ChunkedArray>(flags_list_array));
        }
        columns.push_back(std::make_shared<arrow::ChunkedArray>(fileid_list_array));
        if(robj->hasZonalStats())
        {
            columns.push_back(std::make_shared<arrow::ChunkedArray>(count_list_array));
            columns.push_back(std::make_shared<arrow::ChunkedArray>(min_list_array));
            columns.push_back(std::make_shared<arrow::ChunkedArray>(max_list_array));
            columns.push_back(std::make_shared<arrow::ChunkedArray>(mean_list_array));
            columns.push_back(std::make_shared<arrow::ChunkedArray>(median_list_array));
            columns.push_back(std::make_shared<arrow::ChunkedArray>(stdev_list_array));
            columns.push_back(std::make_shared<arrow::ChunkedArray>(mad_list_array));
        }
    }

    /* Attach metadata to the new schema */
    auto combined_schema = std::make_shared<arrow::Schema>(fields);
    auto metadata = table->schema()->metadata();
    combined_schema = combined_schema->WithMetadata(metadata);

    /* Create a new table with the combined schema and columns */
    auto updated_table = arrow::Table::Make(combined_schema, columns);

    mlog(DEBUG, "Table was %ld rows and %d columns.", table->num_rows(), table->num_columns());
    mlog(DEBUG, "Table is  %ld rows and %d columns.", updated_table->num_rows(), updated_table->num_columns());

    tableToParquetFile(updated_table, output_file);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
* createSchema
*----------------------------------------------------------------------------*/
ArrowParms::format_t ArrowImpl::createSchema (void)
{
    ArrowParms::format_t writer_format = ArrowParms::UNSUPPORTED;

    /* Create Schema */
    schema = make_shared<arrow::Schema>(fieldVector);

    if(parquetBuilder->getParms()->format == ArrowParms::PARQUET)
    {
        /* Set Arrow Output Stream */
        shared_ptr<arrow::io::FileOutputStream> file_output_stream;
        PARQUET_ASSIGN_OR_THROW(file_output_stream, arrow::io::FileOutputStream::Open(parquetBuilder->getFileName()));

        /* Set Writer Properties */
        parquet::WriterProperties::Builder writer_props_builder;
        writer_props_builder.compression(parquet::Compression::SNAPPY);
        writer_props_builder.version(parquet::ParquetVersion::PARQUET_2_6);
        shared_ptr<parquet::WriterProperties> writer_props = writer_props_builder.build();

        /* Set Arrow Writer Properties */
        auto arrow_writer_props = parquet::ArrowWriterProperties::Builder().store_schema()->build();

        /* Set MetaData */
        auto metadata = schema->metadata() ? schema->metadata()->Copy() : std::make_shared<arrow::KeyValueMetadata>();
        if(parquetBuilder->getAsGeo()) appendGeoMetaData(metadata);
        appendServerMetaData(metadata);
        appendPandasMetaData(metadata);
        schema = schema->WithMetadata(metadata);

        /* Create Parquet Writer */
        auto result = parquet::arrow::FileWriter::Open(*schema, ::arrow::default_memory_pool(), file_output_stream, writer_props, arrow_writer_props);
        if(result.ok())
        {
            parquetWriter = std::move(result).ValueOrDie();
            writer_format = ArrowParms::PARQUET;
        }
        else
        {
            mlog(CRITICAL, "Failed to open parquet writer: %s", result.status().ToString().c_str());
        }
    }
    else if(parquetBuilder->getParms()->format == ArrowParms::CSV)
    {
        /* Create CSV Writer */
        auto result = arrow::io::FileOutputStream::Open(parquetBuilder->getFileName());
        if(result.ok())
        {
            csvWriter = result.ValueOrDie();
            writer_format = ArrowParms::CSV;
        }
        else
        {
            mlog(CRITICAL, "Failed to open csv writer: %s", result.status().ToString().c_str());
        }
    }
    else
    {
        mlog(CRITICAL, "Unsupported format: %d", parquetBuilder->getParms()->format);
    }

    /* Return Status */
    return writer_format;
}

/*----------------------------------------------------------------------------
* buildFieldList
*----------------------------------------------------------------------------*/
bool ArrowImpl::buildFieldList (const char* rec_type, int offset, int flags)
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
        if(parquetBuilder->getAsGeo())
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
void ArrowImpl::appendGeoMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata)
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
void ArrowImpl::appendServerMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata)
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
void ArrowImpl::appendPandasMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata)
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

    /* Build Index String */
    const char* time_key = parquetBuilder->getTimeKey();
    if(!time_key) time_key = ""; // replace null with empty string
    string time_str(time_key);
    int max_loops = 10; // upper bound the nesting
    for(int i = 0; i < max_loops; i++)
    {
        /* Remove Parent Fields */
        size_t pos = time_str.find(".");
        if(pos != string::npos) time_str.replace(0, pos+1, "");
        else break;
    }
    FString formatted_time("\"%s\"", time_str.c_str());

    /* Fill In Pandas Meta Data String */
    pandasstr = std::regex_replace(pandasstr, std::regex("    "), "");
    pandasstr = std::regex_replace(pandasstr, std::regex("\n"), " ");
    pandasstr = std::regex_replace(pandasstr, std::regex("_INDEX_"), time_key[0] ? formatted_time.c_str() : "");
    pandasstr = std::regex_replace(pandasstr, std::regex("_COLUMNS_"), columns.c_str());

    /* Append Meta String */
    metadata->Append("pandas", pandasstr.c_str());
}

/*----------------------------------------------------------------------------
* processField
*----------------------------------------------------------------------------*/
void ArrowImpl::processField (RecordObject::field_t& field, shared_ptr<arrow::Array>* column, batch_list_t& record_batch, int num_rows, int batch_row_size_bits)
{
    switch(field.type)
    {
        case RecordObject::DOUBLE:
        {
            arrow::DoubleBuilder builder;
            (void)builder.Reserve(num_rows);
            for(int i = 0; i < record_batch.length(); i++)
            {
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
void ArrowImpl::processArray (RecordObject::field_t& field, shared_ptr<arrow::Array>* column, batch_list_t& record_batch, int batch_row_size_bits)
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
                ParquetBuilder::batch_t* batch = record_batch[i];
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
void ArrowImpl::processGeometry (RecordObject::field_t& x_field, RecordObject::field_t& y_field, shared_ptr<arrow::Array>* column, batch_list_t& record_batch, int num_rows, int batch_row_size_bits)
{
    arrow::BinaryBuilder builder;
    (void)builder.Reserve(num_rows);
    (void)builder.ReserveData(num_rows * sizeof(wkbpoint_t));
    for(int i = 0; i < record_batch.length(); i++)
    {
        ParquetBuilder::batch_t* batch = record_batch[i];
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
void ArrowImpl::processAncillaryFields (vector<shared_ptr<arrow::Array>>& columns, batch_list_t& record_batch)
{
    vector<string>& ancillary_fields = parquetBuilder->getParms()->ancillary_fields;
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
        ParquetBuilder::batch_t* batch = record_batch[i];

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
void ArrowImpl::processAncillaryElements (vector<shared_ptr<arrow::Array>>& columns, batch_list_t& record_batch)
{
    int num_rows = 0;
    vector<string>& ancillary_fields = parquetBuilder->getParms()->ancillary_fields;
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
        ParquetBuilder::batch_t* batch = record_batch[i];

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


/*----------------------------------------------------------------------------
* parquetFileToTable
*----------------------------------------------------------------------------*/
std::shared_ptr<arrow::Table> ArrowImpl::parquetFileToTable(const char* file_path)
{
    std::shared_ptr<arrow::io::ReadableFile> infile;
    PARQUET_ASSIGN_OR_THROW(infile, arrow::io::ReadableFile::Open(file_path, arrow::default_memory_pool()));

    std::unique_ptr<parquet::arrow::FileReader> reader;
    PARQUET_THROW_NOT_OK(parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader));

    std::shared_ptr<arrow::Table> table;
    PARQUET_THROW_NOT_OK(reader->ReadTable(&table));

    return table;
}

/*----------------------------------------------------------------------------
* tableToParquetFile
*----------------------------------------------------------------------------*/
void ArrowImpl::tableToParquetFile(std::shared_ptr<arrow::Table> table, const char* file_path)
{
    std::shared_ptr<arrow::io::FileOutputStream> outfile;
    PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(file_path));

    /* Create a Parquet writer properties builder */
    parquet::WriterProperties::Builder writer_props_builder;
    writer_props_builder.compression(parquet::Compression::SNAPPY);
    writer_props_builder.version(parquet::ParquetVersion::PARQUET_2_6);
    shared_ptr<parquet::WriterProperties> writer_properties = writer_props_builder.build();

    /* Create an Arrow writer properties builder to specify that we want to store Arrow schema */
    auto arrow_properties = parquet::ArrowWriterProperties::Builder().store_schema()->build();

    PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, table->num_rows(), writer_properties, arrow_properties));
}


/*----------------------------------------------------------------------------
* swap_double - local utility fucntion
*----------------------------------------------------------------------------*/
static double swap_double(const double value)
{
    union {
        uint64_t i;
        double   d;
    } conv;

    conv.d = value;
    conv.i = __bswap_64(conv.i);
    return conv.d;
}


/*----------------------------------------------------------------------------
* convertWKBToPoint
*----------------------------------------------------------------------------*/
OGRPoint ArrowImpl::convertWKBToPoint(const std::string& wkb_data)
{
    wkbpoint_t point;

    if(wkb_data.size() < sizeof(wkbpoint_t))
    {
        throw std::runtime_error("Invalid WKB data size.");
    }

    // Byte order is the first byte
    size_t offset = 0;
    std::memcpy(&point.byteOrder, wkb_data.data() + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);

    // Next four bytes are wkbType
    std::memcpy(&point.wkbType, wkb_data.data() + offset, sizeof(uint32_t));

    // Convert to host byte order if necessary
    if(point.byteOrder == 0)
    {
        // Big endian
        point.wkbType = be32toh(point.wkbType);
    }
    else if(point.byteOrder == 1)
    {
        // Little endian
        point.wkbType = le32toh(point.wkbType);
    }
    else
    {
        throw std::runtime_error("Unknown byte order.");
    }
    offset += sizeof(uint32_t);

    // Next eight bytes are x coordinate
    std::memcpy(&point.x, wkb_data.data() + offset, sizeof(double));
    if((point.byteOrder == 0 && __BYTE_ORDER != __BIG_ENDIAN) ||
        (point.byteOrder == 1 && __BYTE_ORDER != __LITTLE_ENDIAN))
    {
        point.x = swap_double(point.x);
    }
    offset += sizeof(double);

    // Next eight bytes are y coordinate
    std::memcpy(&point.y, wkb_data.data() + offset, sizeof(double));
    if((point.byteOrder == 0 && __BYTE_ORDER != __BIG_ENDIAN) ||
        (point.byteOrder == 1 && __BYTE_ORDER != __LITTLE_ENDIAN))
    {
        point.y = swap_double(point.y);
    }

    OGRPoint poi(point.x, point.y, 0);
    return poi;
}

