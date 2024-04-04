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

#include "ArrowSamplerImpl.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructors
 *----------------------------------------------------------------------------*/
ArrowSamplerImpl::ArrowSamplerImpl (ParquetSampler* _sampler):
    parquetSampler(_sampler)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArrowSamplerImpl::~ArrowSamplerImpl (void)
{
}



/*----------------------------------------------------------------------------
* getpoints
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::getPointsFromFile(const char* file_path, std::vector<ParquetSampler::point_info_t*>& points)
{
    std::shared_ptr<arrow::Table> table = parquetFileToTable(file_path);
    int geometry_column_index = table->schema()->GetFieldIndex("geometry");
    if(geometry_column_index == -1)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "Geometry column not found.");
    }

    auto geometry_column = std::static_pointer_cast<arrow::BinaryArray>(table->column(geometry_column_index)->chunk(0));
    mlog(DEBUG, "Geometry column elements: %ld", geometry_column->length());

    /* The geometry column is binary type */
    auto binary_array = std::static_pointer_cast<arrow::BinaryArray>(geometry_column);

    /* Iterate over each item in the geometry column and extract points */
    for(int64_t i = 0; i < binary_array->length(); i++)
    {
        std::string wkb_data = binary_array->GetString(i);     /* Get WKB data as string (binary data) */
        OGRPoint point = convertWKBToPoint(wkb_data);
        ParquetSampler::point_info_t* pinfo = new ParquetSampler::point_info_t(point);
        points.push_back(pinfo);
    }

    int time_column_index = table->schema()->GetFieldIndex("time");
    if(time_column_index > -1)
    {
        /* If time column exists it will have the same length as the geometry column */
        auto time_column = std::static_pointer_cast<arrow::DoubleArray>(table->column(time_column_index)->chunk(0));
        mlog(DEBUG, "Time column elements: %ld", time_column->length());

        /* Update gps time for each point */
        for(int64_t i = 0; i < time_column->length(); i++)
        {
            points[i]->gps_time = time_column->Value(i);
        }
    }
    else mlog(DEBUG, "Time column not found.");
}

/*----------------------------------------------------------------------------
* createParquetFile
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::createParquetFile(const char* input_file, const char* output_file)
{
    /* Read the input file */
    std::shared_ptr<arrow::Table> table = parquetFileToTable(input_file);

    std::vector<std::shared_ptr<arrow::Field>> fields = table->schema()->fields();
    std::vector<std::shared_ptr<arrow::ChunkedArray>> columns = table->columns();

    /* Append new columns to the table */
    mutex.lock();
    {
        fields.insert(fields.end(), new_fields.begin(), new_fields.end());
        columns.insert(columns.end(), new_columns.begin(), new_columns.end());
    }
    mutex.unlock();

    auto metadata = table->schema()->metadata()->Copy();

    /*
     * Pandas metadata does not contain information about the new columns.
     * Pandas and geopands can use/read the file just fine without pandas custom metadata.
     * Removing it is a lot easier than updating it.
     */
    const std::string pandas_key = "pandas";
    if(metadata->Contains(pandas_key))
    {
        int key_index = metadata->FindKey(pandas_key);
        if(key_index != -1)
        {
            PARQUET_THROW_NOT_OK(metadata->Delete(key_index));
        }
    }

    /* Create a filemap metadata */
    PARQUET_THROW_NOT_OK(metadata->Set("filemap", createFileMap()));

    /* Attach metadata to the new schema */
    auto combined_schema = std::make_shared<arrow::Schema>(fields);
    combined_schema = combined_schema->WithMetadata(metadata);

    /* Create a new table with the combined schema and columns */
    auto updated_table = arrow::Table::Make(combined_schema, columns);

    mlog(DEBUG, "Table was %ld rows and %d columns.", table->num_rows(), table->num_columns());
    mlog(DEBUG, "Table is  %ld rows and %d columns.", updated_table->num_rows(), updated_table->num_columns());

    tableToParquetFile(updated_table, output_file);

    // printParquetMetadata(input_file);
    // printParquetMetadata(output_file);
}


/*----------------------------------------------------------------------------
* processSamples
*----------------------------------------------------------------------------*/
bool ArrowSamplerImpl::processSamples(ParquetSampler::sampler_t* sampler)
{
    auto pool = arrow::default_memory_pool();
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

    std::shared_ptr<arrow::Array> value_list_array, time_list_array, fileid_list_array, flags_list_array;
    std::shared_ptr<arrow::Array> count_list_array, min_list_array, max_list_array, mean_list_array, median_list_array, stdev_list_array, mad_list_array;

    try
    {
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

                /* Collect all fileIds used by samples - duplicates are ignored */
                sampler->file_ids.insert(sample->fileId);
            }
        }

        /* Finish the list builders */
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
    }
    catch(const RunTimeException& e)
    {
        /* No columns will be added */
        mlog(e.level(), "Error processing samples: %s", e.what());
        return false;
    }

    const std::string prefix = sampler->rkey;

    /* Create fields for the new columns */
    auto value_field = std::make_shared<arrow::Field>(prefix + ".value", arrow::list(arrow::float64()));
    auto time_field = std::make_shared<arrow::Field>(prefix + ".time", arrow::list(arrow::float64()));
    auto flags_field = std::make_shared<arrow::Field>(prefix + ".flags", arrow::list(arrow::uint32()));
    auto fileid_field = std::make_shared<arrow::Field>(prefix + ".fileid", arrow::list(arrow::uint64()));

    auto count_field = std::make_shared<arrow::Field>(prefix + ".stats.count", arrow::list(arrow::uint32()));
    auto min_field = std::make_shared<arrow::Field>(prefix + ".stats.min", arrow::list(arrow::float64()));
    auto max_field = std::make_shared<arrow::Field>(prefix + ".stats.max", arrow::list(arrow::float64()));
    auto mean_field = std::make_shared<arrow::Field>(prefix + ".stats.mean", arrow::list(arrow::float64()));
    auto median_field = std::make_shared<arrow::Field>(prefix + ".stats.median", arrow::list(arrow::float64()));
    auto stdev_field = std::make_shared<arrow::Field>(prefix + ".stats.stdev", arrow::list(arrow::float64()));
    auto mad_field = std::make_shared<arrow::Field>(prefix + ".stats.mad", arrow::list(arrow::float64()));

    /* Multiple threads may be updating the new fields and columns */
    mutex.lock();
    {
        /* Add new columns fields */
        new_fields.push_back(value_field);
        new_fields.push_back(time_field);
        if(robj->hasFlags())
        {
            new_fields.push_back(flags_field);
        }
        new_fields.push_back(fileid_field);
        if(robj->hasZonalStats())
        {
            new_fields.push_back(count_field);
            new_fields.push_back(min_field);
            new_fields.push_back(max_field);
            new_fields.push_back(mean_field);
            new_fields.push_back(median_field);
            new_fields.push_back(stdev_field);
            new_fields.push_back(mad_field);
        }

        /* Add new columns data */
        new_columns.push_back(std::make_shared<arrow::ChunkedArray>(value_list_array));
        new_columns.push_back(std::make_shared<arrow::ChunkedArray>(time_list_array));
        if(robj->hasFlags())
        {
            new_columns.push_back(std::make_shared<arrow::ChunkedArray>(flags_list_array));
        }
        new_columns.push_back(std::make_shared<arrow::ChunkedArray>(fileid_list_array));
        if(robj->hasZonalStats())
        {
            new_columns.push_back(std::make_shared<arrow::ChunkedArray>(count_list_array));
            new_columns.push_back(std::make_shared<arrow::ChunkedArray>(min_list_array));
            new_columns.push_back(std::make_shared<arrow::ChunkedArray>(max_list_array));
            new_columns.push_back(std::make_shared<arrow::ChunkedArray>(mean_list_array));
            new_columns.push_back(std::make_shared<arrow::ChunkedArray>(median_list_array));
            new_columns.push_back(std::make_shared<arrow::ChunkedArray>(stdev_list_array));
            new_columns.push_back(std::make_shared<arrow::ChunkedArray>(mad_list_array));
        }
    }
    mutex.unlock();

    return true;
}


/*----------------------------------------------------------------------------
* clearColumns
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::clearColumns(void)
{
    mutex.lock();
    {
        new_fields.clear();
        new_columns.clear();
    }
    mutex.unlock();
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/


/*----------------------------------------------------------------------------
* createFileMap
*----------------------------------------------------------------------------*/
std::string ArrowSamplerImpl::createFileMap(void)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    /* Serialize into JSON */
    const std::vector<ParquetSampler::sampler_t*>& samplers = parquetSampler->getSamplers();
    for(ParquetSampler::sampler_t* sampler : samplers)
    {
        rapidjson::Value asset_list_json(rapidjson::kArrayType);

        for(const auto& pair : sampler->filemap)
        {
            rapidjson::Value asset_json(rapidjson::kObjectType);
            asset_json.AddMember("file_id", pair.first, allocator);
            asset_json.AddMember("file_name", rapidjson::Value(pair.second, allocator), allocator);
            asset_list_json.PushBack(asset_json, allocator);
        }
        document.AddMember(rapidjson::Value(sampler->rkey, allocator), asset_list_json, allocator);
    }

    /* Convert JSON document to string */
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    std::string serialized_json = buffer.GetString();
    return serialized_json;
}