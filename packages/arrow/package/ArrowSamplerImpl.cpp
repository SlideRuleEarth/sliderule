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

#include <parquet/arrow/schema.h>
#include <parquet/arrow/writer.h>
#include <parquet/properties.h>
#include <arrow/util/key_value_metadata.h>
#include <arrow/io/file.h>
#include <arrow/io/api.h>
#include <arrow/ipc/api.h>
#include <arrow/builder.h>
#include <arrow/csv/writer.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructors
 *----------------------------------------------------------------------------*/
ArrowSamplerImpl::ArrowSamplerImpl (ArrowSampler* _sampler):
    arrowSampler(_sampler),
    inputFile(NULL),
    reader(nullptr),
    timeKey(NULL),
    xKey(NULL),
    yKey(NULL),
    asGeo(false)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArrowSamplerImpl::~ArrowSamplerImpl (void)
{
    delete[] timeKey;
    delete[] xKey;
    delete[] yKey;
}

/*----------------------------------------------------------------------------
* processInputFile
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::processInputFile(const char* file_path, std::vector<point_info_t>& points)
{
    try
    {
        /* Open the input file */
        PARQUET_ASSIGN_OR_THROW(inputFile, arrow::io::ReadableFile::Open(file_path, arrow::default_memory_pool()));
        PARQUET_ASSIGN_OR_THROW(reader, parquet::arrow::OpenFile(inputFile, arrow::default_memory_pool()));
    }
    catch(const parquet::ParquetStatusException& e)
    {
        /* Rethrow as a runtime exception */
        throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", e.what());
    }

    getMetadata();
    getPoints(points);
}

/*----------------------------------------------------------------------------
* processSamples
*----------------------------------------------------------------------------*/
bool ArrowSamplerImpl::processSamples(ArrowSampler::batch_sampler_t* sampler)
{
    const OutputFields* parms = arrowSampler->getParms();
    bool  status = false;

    /* Convert samples into new columns */
    try
    {
        if(parms->format == OutputFields::GEOPARQUET || parms->format == OutputFields::PARQUET || parms->format == OutputFields::FEATHER)
        {
            status = makeColumnsWithLists(sampler);
        }
        else if(parms->format == OutputFields::CSV)
        {
            /* Arrow csv writer cannot handle columns with lists of samples */
            status = makeColumnsWithOneSample(sampler);
        }
        else mlog(CRITICAL, "Unsupported file format");
    }
    catch(const parquet::ParquetStatusException& e)
    {
        mlog(CRITICAL, "Error processing samples: %s", e.what());
    }

    if(status)
    {
        /* Create raster file map <id, filename> */
        const std::set<uint64_t> &sampleIds = sampler->robj->fileDictGetSampleIds();
        for(std::set<uint64_t>::const_iterator it = sampleIds.begin(); it != sampleIds.end(); it++)
        {
            const uint64_t fileId = *it;
            const char* name = sampler->robj->fileDictGet(fileId);
            sampler->filemap.emplace_back(fileId, name);
        }
    }
    else
    {
        /* No columns will be added */
        newFields.clear();
        newColumns.clear();
    }

    return status;
}

/*----------------------------------------------------------------------------
* createOutputFiles
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::createOutpuFiles(void)
{
    const OutputFields* parms = arrowSampler->getParms();
    const char* dataFile = arrowSampler->getDataFile();

    try
    {
        auto table = inputFileToTable();
        auto updated_table = addNewColumns(table);
        table = nullptr;

        switch(parms->format.value)
        {
        case OutputFields::GEOPARQUET:
            tableToParquet(updated_table, dataFile);
            break;

        case OutputFields::PARQUET:
            tableToParquet(updated_table, dataFile);
            break;

        case OutputFields::CSV:
            /* Remove geometry column before writing to csv */
            table = removeGeometryColumn(updated_table);
            tableToCsv(table, dataFile);
            break;

        case OutputFields::FEATHER:
            tableToFeather(updated_table, dataFile);
            break;

        default:
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Unsupported file format");
        }

        /* Generate metadata file since csv/feather writers ignore it */
        if(parms->format == OutputFields::CSV || parms->format == OutputFields::FEATHER)
        {
            metadataToJson(updated_table, arrowSampler->getMetadataFile());
        }
    }
    catch(const parquet::ParquetStatusException& e)
    {
        /* Rethrow as a runtime exception */
        throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", e.what());
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
* getMetadata
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::getMetadata(void)
{
    bool foundIt = false;

    const std::shared_ptr<parquet::FileMetaData> file_metadata = reader->parquet_reader()->metadata();

    for(int i = 0; i < file_metadata->key_value_metadata()->size(); i++)
    {
        const std::string key = file_metadata->key_value_metadata()->key(i);
        const std::string value = file_metadata->key_value_metadata()->value(i);

        if(key == "recordinfo")
        {
            rapidjson::Document doc;
            doc.Parse(value.c_str());
            if(doc.HasParseError())
            {
                throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to parse metadata JSON: %s", value.c_str());
            }

            const char* _timeKey = doc["time"].GetString();
            const char* _xKey    = doc["x"].GetString();
            const char* _yKey    = doc["y"].GetString();
            const bool _asGeo    = doc["as_geo"].GetBool();

            /* Make sure the keys are not empty */
            if(_timeKey[0] == '\0' || _xKey[0] == '\0' || _yKey[0] == '\0')
            {
                throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid recordinfo in sliderule metadata.");
            }

            timeKey = StringLib::duplicate(_timeKey);
            xKey    = StringLib::duplicate(_xKey);
            yKey    = StringLib::duplicate(_yKey);
            asGeo   = _asGeo;
            foundIt = true;
            break;
        }
    }

    if(!foundIt)
    {
       foundIt = getMetadataLegacy();
    }

    if(!foundIt)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "No metadata found in parquet file.");
    }
}

/*----------------------------------------------------------------------------
* getMetadataLegacy - this supports and older layout of the parquet file
*----------------------------------------------------------------------------*/
bool ArrowSamplerImpl::getMetadataLegacy(void)
{
    const std::shared_ptr<parquet::FileMetaData> file_metadata = reader->parquet_reader()->metadata();

    for(int i = 0; i < file_metadata->key_value_metadata()->size(); i++)
    {
        const std::string key = file_metadata->key_value_metadata()->key(i);
        const std::string value = file_metadata->key_value_metadata()->value(i);

        if(key == "sliderule")
        {
            rapidjson::Document doc;
            doc.Parse(value.c_str());
            if(doc.HasParseError())
            {
                return false;
            }

            if(doc.HasMember("recordinfo"))
            {
                const rapidjson::Value& recordinfo = doc["recordinfo"];

                const char* _timeKey = recordinfo["time"].GetString();
                const char* _xKey    = recordinfo["x"].GetString();
                const char* _yKey    = recordinfo["y"].GetString();
                const bool _asGeo    = recordinfo["as_geo"].GetBool();

                /* Make sure the keys are not empty */
                if(_timeKey[0] == '\0' || _xKey[0] == '\0' || _yKey[0] == '\0')
                {
                    return false;
                }

                timeKey = StringLib::duplicate(_timeKey);
                xKey    = StringLib::duplicate(_xKey);
                yKey    = StringLib::duplicate(_yKey);
                asGeo   = _asGeo;

                /* Found it */
                return true;
            }
        }
    }
    return false;
}

/*----------------------------------------------------------------------------
* getPoints
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::getPoints(std::vector<point_info_t>& points)
{
    if(asGeo)
        getGeoPoints(points);
    else
        getXYPoints(points);

    /* Get point's gps from time column */
    const std::vector<const char*> columnNames = {timeKey};
    const std::shared_ptr<arrow::Table> table = inputFileToTable(columnNames);
    const int time_column_index = table->schema()->GetFieldIndex(timeKey);
    if(time_column_index > -1)
    {
        auto time_column = std::static_pointer_cast<arrow::TimestampArray>(table->column(time_column_index)->chunk(0));
        auto timestamp_type = std::static_pointer_cast<arrow::TimestampType>(table->column(time_column_index)->type());

        if(timestamp_type->unit() != arrow::TimeUnit::NANO)
        {
            mlog(ERROR, "Time column must be in nanoseconds.");
            points.clear();
            return;
        }

        /* Convert unix nanoseconds to gps seconds */
        for(int64_t i = 0; i < time_column->length(); i++)
        {
            const double unix_nsecs = time_column->Value(i);
            points[i].gps = TimeLib::sys2gpstime(unix_nsecs / 1000.0) * 1000.0;
        }
    }
    else mlog(ERROR, "Time column not found.");
}

/*----------------------------------------------------------------------------
* getXYPoints
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::getXYPoints(std::vector<point_info_t>& points)
{
    const std::vector<const char*> columnNames = {xKey, yKey};

    const std::shared_ptr<arrow::Table> table = inputFileToTable(columnNames);
    const int x_column_index = table->schema()->GetFieldIndex(xKey);
    if(x_column_index == -1) throw RunTimeException(ERROR, RTE_FAILURE, "X column not found.");

    const int y_column_index = table->schema()->GetFieldIndex(yKey);
    if(y_column_index == -1) throw RunTimeException(ERROR, RTE_FAILURE, "Y column not found.");

    auto x_column = std::static_pointer_cast<arrow::DoubleArray>(table->column(x_column_index)->chunk(0));
    auto y_column = std::static_pointer_cast<arrow::DoubleArray>(table->column(y_column_index)->chunk(0));

    /* x and y columns are the same length */
    for(int64_t i = 0; i < x_column->length(); i++)
    {
        const double x = x_column->Value(i);
        const double y = y_column->Value(i);

        points.emplace_back(point_info_t({{x, y, 0.0}, 0}));
    }
    mlog(DEBUG, "Read %zu points from file", points.size());
}

/*----------------------------------------------------------------------------
* getGeoPoints
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::getGeoPoints(std::vector<point_info_t>& points)
{
    const char* geocol  = "geometry";
    const std::vector<const char*> columnNames = {geocol};

    const std::shared_ptr<arrow::Table> table = inputFileToTable(columnNames);
    const int geometry_column_index = table->schema()->GetFieldIndex(geocol);
    if(geometry_column_index == -1) throw RunTimeException(ERROR, RTE_FAILURE, "Geometry column not found.");

    auto geometry_column = std::static_pointer_cast<arrow::BinaryArray>(table->column(geometry_column_index)->chunk(0));
    mlog(DEBUG, "Geometry column elements: %ld", geometry_column->length());

    /* The geometry column is binary type */
    auto binary_array = std::static_pointer_cast<arrow::BinaryArray>(geometry_column);

    /* Iterate over each item in the geometry column and extract points */
    for(int64_t i = 0; i < binary_array->length(); i++)
    {
        const std::string wkb_data = binary_array->GetString(i);     /* Get WKB data as string (binary data) */
        const OutputLib::wkbpoint_t point = convertWKBToPoint(wkb_data);

        points.emplace_back(point_info_t({{point.x, point.y, 0.0}, 0}));
    }
    mlog(INFO, "Read %zu geo points from file", points.size());
}

/*----------------------------------------------------------------------------
* inputFileToTable
*----------------------------------------------------------------------------*/
std::shared_ptr<arrow::Table> ArrowSamplerImpl::inputFileToTable(const std::vector<const char*>& columnNames)
{
    /* If columnNames is empty, read all columns */
    if(columnNames.empty())
    {
        std::shared_ptr<arrow::Table> table;
        PARQUET_THROW_NOT_OK(reader->ReadTable(&table));
        return table;
    }

    /* Read only the specified columns */
    std::shared_ptr<arrow::Schema> schema;
    PARQUET_THROW_NOT_OK(reader->GetSchema(&schema));
    std::vector<int> columnIndices;
    for(const auto& columnName : columnNames)
    {
        auto index = schema->GetFieldIndex(columnName);
        if(index != -1)
        {
            columnIndices.push_back(index);
        }
        else mlog(DEBUG, "Column %s not found in parquet file.", columnName);
    }

    std::shared_ptr<arrow::Table> table;
    PARQUET_THROW_NOT_OK(reader->ReadTable(columnIndices, &table));
    return table;
}

/*----------------------------------------------------------------------------
* addNewColumns
*----------------------------------------------------------------------------*/
std::shared_ptr<arrow::Table> ArrowSamplerImpl::addNewColumns(const std::shared_ptr<arrow::Table>& table)
{
    std::vector<std::shared_ptr<arrow::Field>> fields = table->schema()->fields();
    std::vector<std::shared_ptr<arrow::ChunkedArray>> columns = table->columns();

    /* Append new columns to the table */
    mutex.lock();
    {
        fields.insert(fields.end(), newFields.begin(), newFields.end());
        columns.insert(columns.end(), newColumns.begin(), newColumns.end());
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
        const int key_index = metadata->FindKey(pandas_key);
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

    return updated_table;
}

/*----------------------------------------------------------------------------
* makeColumnsWithLists
*----------------------------------------------------------------------------*/
bool ArrowSamplerImpl::makeColumnsWithLists(ArrowSampler::batch_sampler_t* sampler)
{
    auto* pool = arrow::default_memory_pool();
    const RasterObject* robj = sampler->robj;

    /* Create list builders for the new columns */
    arrow::ListBuilder band_list_builder(pool, std::make_shared<arrow::StringBuilder>());
    auto* band_builder = dynamic_cast<arrow::StringBuilder*>(band_list_builder.value_builder());

    arrow::ListBuilder value_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* value_builder = dynamic_cast<arrow::DoubleBuilder*>(value_list_builder.value_builder());

    arrow::ListBuilder time_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* time_builder = dynamic_cast<arrow::DoubleBuilder*>(time_list_builder.value_builder());

    arrow::ListBuilder flags_list_builder(pool, std::make_shared<arrow::UInt32Builder>());
    auto* flags_builder = dynamic_cast<arrow::UInt32Builder*>(flags_list_builder.value_builder());

    arrow::ListBuilder fileid_list_builder(pool, std::make_shared<arrow::UInt64Builder>());
    auto* fileid_builder = dynamic_cast<arrow::UInt64Builder*>(fileid_list_builder.value_builder());

    /* Create list builders for zonal stats */
    arrow::ListBuilder count_list_builder(pool, std::make_shared<arrow::UInt32Builder>());
    auto* count_builder = dynamic_cast<arrow::UInt32Builder*>(count_list_builder.value_builder());

    arrow::ListBuilder min_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* min_builder = dynamic_cast<arrow::DoubleBuilder*>(min_list_builder.value_builder());

    arrow::ListBuilder max_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* max_builder = dynamic_cast<arrow::DoubleBuilder*>(max_list_builder.value_builder());

    arrow::ListBuilder mean_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* mean_builder = dynamic_cast<arrow::DoubleBuilder*>(mean_list_builder.value_builder());

    arrow::ListBuilder median_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* median_builder = dynamic_cast<arrow::DoubleBuilder*>(median_list_builder.value_builder());

    arrow::ListBuilder stdev_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* stdev_builder = dynamic_cast<arrow::DoubleBuilder*>(stdev_list_builder.value_builder());

    arrow::ListBuilder mad_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* mad_builder = dynamic_cast<arrow::DoubleBuilder*>(mad_list_builder.value_builder());

    arrow::ListBuilder slope_count_list_builder(pool, std::make_shared<arrow::UInt32Builder>());
    auto* slope_count_builder = dynamic_cast<arrow::UInt32Builder*>(slope_count_list_builder.value_builder());

    arrow::ListBuilder slope_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* slope_builder = dynamic_cast<arrow::DoubleBuilder*>(slope_list_builder.value_builder());

    arrow::ListBuilder aspect_list_builder(pool, std::make_shared<arrow::DoubleBuilder>());
    auto* aspect_builder = dynamic_cast<arrow::DoubleBuilder*>(aspect_list_builder.value_builder());

    std::shared_ptr<arrow::Array> band_list_array, value_list_array, time_list_array, fileid_list_array, flags_list_array;
    std::shared_ptr<arrow::Array> count_list_array, min_list_array, max_list_array, mean_list_array, median_list_array, stdev_list_array, mad_list_array;
    std::shared_ptr<arrow::Array> slope_count_list_array, slope_list_array, aspect_list_array;

    /* Iterate over each sample in a vector of lists of samples */
    for(int i = 0; i < sampler->samples.length(); i++)
    {
        sample_list_t* slist = sampler->samples[i];

        /* Start new lists */
        if(robj->hasBands())
        {
            PARQUET_THROW_NOT_OK(band_list_builder.Append());
        }
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
        if(robj->hasSpatialDerivs())
        {
            PARQUET_THROW_NOT_OK(slope_count_list_builder.Append());
            PARQUET_THROW_NOT_OK(slope_list_builder.Append());
            PARQUET_THROW_NOT_OK(aspect_list_builder.Append());
        }

        /* Iterate over each sample and add it to the list
         * If slist is empty the column will contain an empty list
         * to keep the number of rows consistent with the other columns
         */
        for(int j = 0; j < slist->length(); j++)
        {
            const RasterSample* sample = slist->get(j);

            if(robj->hasBands())
            {
                PARQUET_THROW_NOT_OK(band_builder->Append(sample->bandName));
            }
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
            if(robj->hasSpatialDerivs())
            {
                PARQUET_THROW_NOT_OK(slope_count_builder->Append(sample->derivs.count));
                PARQUET_THROW_NOT_OK(slope_builder->Append(sample->derivs.slopeDeg));
                PARQUET_THROW_NOT_OK(aspect_builder->Append(sample->derivs.aspectDeg));
            }
        }
    }

    /* Finish the list builders */
    if(robj->hasBands())
    {
        PARQUET_THROW_NOT_OK(band_list_builder.Finish(&band_list_array));
    }
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
    if(robj->hasSpatialDerivs())
    {
        PARQUET_THROW_NOT_OK(slope_count_list_builder.Finish(&slope_count_list_array));
        PARQUET_THROW_NOT_OK(slope_list_builder.Finish(&slope_list_array));
        PARQUET_THROW_NOT_OK(aspect_list_builder.Finish(&aspect_list_array));
    }

    const std::string prefix = sampler->rkey;

    /* Create fields for the new columns */
    auto band_field = std::make_shared<arrow::Field>(prefix + ".band", arrow::list(arrow::utf8()));
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

    auto slope_count_field = std::make_shared<arrow::Field>(prefix + ".derivs.count", arrow::list(arrow::uint32()));
    auto slope_field = std::make_shared<arrow::Field>(prefix + ".derivs.slope", arrow::list(arrow::float64()));
    auto aspect_field = std::make_shared<arrow::Field>(prefix + ".derivs.aspect", arrow::list(arrow::float64()));

    /* Multiple threads may be updating the new fields and columns
     * No throwing exceptions here, since the mutex is locked
     */
    mutex.lock();
    {
        /* Add new columns fields */
        if(robj->hasBands())
        {
            newFields.push_back(band_field);
        }
        newFields.push_back(value_field);
        newFields.push_back(time_field);
        if(robj->hasFlags())
        {
            newFields.push_back(flags_field);
        }
        newFields.push_back(fileid_field);
        if(robj->hasZonalStats())
        {
            newFields.push_back(count_field);
            newFields.push_back(min_field);
            newFields.push_back(max_field);
            newFields.push_back(mean_field);
            newFields.push_back(median_field);
            newFields.push_back(stdev_field);
            newFields.push_back(mad_field);
        }
        if(robj->hasSpatialDerivs())
        {
            newFields.push_back(slope_count_field);
            newFields.push_back(slope_field);
            newFields.push_back(aspect_field);
        }

        /* Add new columns */
        if(robj->hasBands())
        {
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(band_list_array));
        }
        newColumns.push_back(std::make_shared<arrow::ChunkedArray>(value_list_array));
        newColumns.push_back(std::make_shared<arrow::ChunkedArray>(time_list_array));
        if(robj->hasFlags())
        {
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(flags_list_array));
        }
        newColumns.push_back(std::make_shared<arrow::ChunkedArray>(fileid_list_array));
        if(robj->hasZonalStats())
        {
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(count_list_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(min_list_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(max_list_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(mean_list_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(median_list_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(stdev_list_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(mad_list_array));
        }
        if(robj->hasSpatialDerivs())
        {
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(slope_count_list_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(slope_list_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(aspect_list_array));
        }
    }
    mutex.unlock();

    return true;
}

/*----------------------------------------------------------------------------
* makeColumnsWithOneSample
*----------------------------------------------------------------------------*/
bool ArrowSamplerImpl::makeColumnsWithOneSample(ArrowSampler::batch_sampler_t* sampler)
{
    auto* pool = arrow::default_memory_pool();
    const RasterObject* robj = sampler->robj;

    /* Create builders for the new columns */
    arrow::StringBuilder band_builder(pool);
    arrow::DoubleBuilder value_builder(pool);
    arrow::DoubleBuilder time_builder(pool);
    arrow::UInt32Builder flags_builder(pool);
    arrow::UInt64Builder fileid_builder(pool);

    /* Create builders for zonal stats */
    arrow::UInt32Builder count_builder(pool);
    arrow::DoubleBuilder min_builder(pool);
    arrow::DoubleBuilder max_builder(pool);
    arrow::DoubleBuilder mean_builder(pool);
    arrow::DoubleBuilder median_builder(pool);
    arrow::DoubleBuilder stdev_builder(pool);
    arrow::DoubleBuilder mad_builder(pool);

    /* Create builder for slope/aspect */
    arrow::UInt32Builder slope_count_builder(pool);
    arrow::DoubleBuilder slope_builder(pool);
    arrow::DoubleBuilder aspect_builder(pool);

    std::shared_ptr<arrow::Array> band_array, value_array, time_array, fileid_array, flags_array;
    std::shared_ptr<arrow::Array> count_array, min_array, max_array, mean_array, median_array, stdev_array, mad_array;

    std::shared_ptr<arrow::Array> slope_count_array, slope_array, aspect_array;

    RasterSample fakeSample(0.0, 0);
    fakeSample.value = std::nan("");

    for(int i = 0; i < sampler->samples.length(); i++)
    {
        sample_list_t* slist = sampler->samples[i];
        RasterSample* sample;

        if(!slist->empty())
        {
            sample = getFirstValidSample(slist);
        }
        else
        {
            /* List is empty, no samples but we must have a fake sample
             * to keep the number of rows consistent with the other columns
             */
            sample = &fakeSample;
        }
        if(robj->hasBands())
        {
            PARQUET_THROW_NOT_OK(band_builder.Append(sample->bandName));
        }
        PARQUET_THROW_NOT_OK(value_builder.Append(sample->value));
        PARQUET_THROW_NOT_OK(time_builder.Append(sample->time));
        if(robj->hasFlags())
        {
            PARQUET_THROW_NOT_OK(flags_builder.Append(sample->flags));
        }
        PARQUET_THROW_NOT_OK(fileid_builder.Append(sample->fileId));
        if(robj->hasZonalStats())
        {
            PARQUET_THROW_NOT_OK(count_builder.Append(sample->stats.count));
            PARQUET_THROW_NOT_OK(min_builder.Append(sample->stats.min));
            PARQUET_THROW_NOT_OK(max_builder.Append(sample->stats.max));
            PARQUET_THROW_NOT_OK(mean_builder.Append(sample->stats.mean));
            PARQUET_THROW_NOT_OK(median_builder.Append(sample->stats.median));
            PARQUET_THROW_NOT_OK(stdev_builder.Append(sample->stats.stdev));
            PARQUET_THROW_NOT_OK(mad_builder.Append(sample->stats.mad));
        }
        if(robj->hasSpatialDerivs())
        {
            PARQUET_THROW_NOT_OK(slope_count_builder.Append(sample->derivs.count));
            PARQUET_THROW_NOT_OK(slope_builder.Append(sample->derivs.slopeDeg));
            PARQUET_THROW_NOT_OK(aspect_builder.Append(sample->derivs.aspectDeg));
        }
    }

    /* Finish the builders */
    if(robj->hasBands())
    {
        PARQUET_THROW_NOT_OK(band_builder.Finish(&band_array));
    }
    PARQUET_THROW_NOT_OK(value_builder.Finish(&value_array));
    PARQUET_THROW_NOT_OK(time_builder.Finish(&time_array));
    if(robj->hasFlags())
    {
        PARQUET_THROW_NOT_OK(flags_builder.Finish(&flags_array));
    }
    PARQUET_THROW_NOT_OK(fileid_builder.Finish(&fileid_array));
    if(robj->hasZonalStats())
    {
        PARQUET_THROW_NOT_OK(count_builder.Finish(&count_array));
        PARQUET_THROW_NOT_OK(min_builder.Finish(&min_array));
        PARQUET_THROW_NOT_OK(max_builder.Finish(&max_array));
        PARQUET_THROW_NOT_OK(mean_builder.Finish(&mean_array));
        PARQUET_THROW_NOT_OK(median_builder.Finish(&median_array));
        PARQUET_THROW_NOT_OK(stdev_builder.Finish(&stdev_array));
        PARQUET_THROW_NOT_OK(mad_builder.Finish(&mad_array));
    }
    if(robj->hasSpatialDerivs())
    {
        PARQUET_THROW_NOT_OK(slope_count_builder.Finish(&slope_count_array));
        PARQUET_THROW_NOT_OK(slope_builder.Finish(&slope_array));
        PARQUET_THROW_NOT_OK(aspect_builder.Finish(&aspect_array));
    }

    const std::string prefix = sampler->rkey;

    /* Create fields for the new columns */
    auto band_field = std::make_shared<arrow::Field>(prefix + ".band", arrow::utf8());
    auto value_field = std::make_shared<arrow::Field>(prefix + ".value", arrow::float64());
    auto time_field = std::make_shared<arrow::Field>(prefix + ".time", arrow::float64());
    auto flags_field = std::make_shared<arrow::Field>(prefix + ".flags", arrow::uint32());
    auto fileid_field = std::make_shared<arrow::Field>(prefix + ".fileid", arrow::uint64());

    auto count_field = std::make_shared<arrow::Field>(prefix + ".stats.count", arrow::uint32());
    auto min_field = std::make_shared<arrow::Field>(prefix + ".stats.min", arrow::float64());
    auto max_field = std::make_shared<arrow::Field>(prefix + ".stats.max", arrow::float64());
    auto mean_field = std::make_shared<arrow::Field>(prefix + ".stats.mean", arrow::float64());
    auto median_field = std::make_shared<arrow::Field>(prefix + ".stats.median", arrow::float64());
    auto stdev_field = std::make_shared<arrow::Field>(prefix + ".stats.stdev", arrow::float64());
    auto mad_field = std::make_shared<arrow::Field>(prefix + ".stats.mad", arrow::float64());

    auto slope_count_field = std::make_shared<arrow::Field>(prefix + ".derivs.count", arrow::uint32());
    auto slope_field = std::make_shared<arrow::Field>(prefix + ".derivs.slope", arrow::float64());
    auto aspect_field = std::make_shared<arrow::Field>(prefix + ".derivs.aspect", arrow::float64());

    /* Multiple threads may be updating the new fields and columns
     * No throwing exceptions here, since the mutex is locked
     */
    mutex.lock();
    {
        /* Add new columns fields */
        if(robj->hasBands())
        {
            newFields.push_back(band_field);
        }
        newFields.push_back(value_field);
        newFields.push_back(time_field);
        if(robj->hasFlags())
        {
            newFields.push_back(flags_field);
        }
        newFields.push_back(fileid_field);
        if(robj->hasZonalStats())
        {
            newFields.push_back(count_field);
            newFields.push_back(min_field);
            newFields.push_back(max_field);
            newFields.push_back(mean_field);
            newFields.push_back(median_field);
            newFields.push_back(stdev_field);
            newFields.push_back(mad_field);
        }

        /* Add new columns */
        if(robj->hasBands())
        {
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(band_array));
        }
        newColumns.push_back(std::make_shared<arrow::ChunkedArray>(value_array));
        newColumns.push_back(std::make_shared<arrow::ChunkedArray>(time_array));
        if(robj->hasFlags())
        {
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(flags_array));
        }
        newColumns.push_back(std::make_shared<arrow::ChunkedArray>(fileid_array));
        if(robj->hasZonalStats())
        {
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(count_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(min_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(max_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(mean_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(median_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(stdev_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(mad_array));
        }
        if(robj->hasSpatialDerivs())
        {
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(slope_count_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(slope_array));
            newColumns.push_back(std::make_shared<arrow::ChunkedArray>(aspect_array));
        }
    }
    mutex.unlock();

    return true;
}

/*----------------------------------------------------------------------------
* getFirstValidSample
*----------------------------------------------------------------------------*/
RasterSample* ArrowSamplerImpl::getFirstValidSample(sample_list_t* slist)
{
    for(int i = 0; i < slist->length(); i++)
    {
        RasterSample* sample = slist->get(i);
        if(!std::isnan(sample->value))
            return sample;
    }

    /* Return the first sample if no valid samples are found */
    return slist->get(0);
}

/*----------------------------------------------------------------------------
* tableToParquet
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::tableToParquet(const std::shared_ptr<arrow::Table>& table, const char* file_path)
{
    std::shared_ptr<arrow::io::FileOutputStream> outfile;
    PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(file_path));

    /* Create a Parquet writer properties builder */
    parquet::WriterProperties::Builder writer_props_builder;
    writer_props_builder.compression(parquet::Compression::SNAPPY);
    writer_props_builder.version(parquet::ParquetVersion::PARQUET_2_6);
    const shared_ptr<parquet::WriterProperties> writer_properties = writer_props_builder.build();

    /* Create an Arrow writer properties builder to specify that we want to store Arrow schema */
    auto arrow_properties = parquet::ArrowWriterProperties::Builder().store_schema()->build();
    PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, table->num_rows(), writer_properties, arrow_properties));

    /* Close the output file */
    PARQUET_THROW_NOT_OK(outfile->Close());
}

/*----------------------------------------------------------------------------
* tableToCsv
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::tableToCsv(const std::shared_ptr<arrow::Table>& table, const char* file_path)
{
    std::shared_ptr<arrow::io::FileOutputStream> outfile;
    PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(file_path));

    /* Create a CSV writer */
    const arrow::csv::WriteOptions write_options = arrow::csv::WriteOptions::Defaults();

    /* Write the table to the CSV file */
    PARQUET_THROW_NOT_OK(arrow::csv::WriteCSV(*table, write_options, outfile.get()));

    /* Close the output file */
    PARQUET_THROW_NOT_OK(outfile->Close());
}

/*----------------------------------------------------------------------------
* tableToFeather
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::tableToFeather(const std::shared_ptr<arrow::Table>& table, const char* file_path)
{
    std::shared_ptr<arrow::io::FileOutputStream> outfile;
    PARQUET_ASSIGN_OR_THROW(outfile, arrow::io::FileOutputStream::Open(file_path));

    /* Write the table to the Feather file */
    PARQUET_THROW_NOT_OK(arrow::ipc::feather::WriteTable(*table, outfile.get()));

    /* Close the output file */
    PARQUET_THROW_NOT_OK(outfile->Close());
}

/*----------------------------------------------------------------------------
* removeGeometryColumn
*----------------------------------------------------------------------------*/
std::shared_ptr<arrow::Table> ArrowSamplerImpl::removeGeometryColumn(std::shared_ptr<arrow::Table>& table)
{
    const int column_index = table->schema()->GetFieldIndex("geometry");

    if(column_index == -1)
        return table;

    arrow::Result<std::shared_ptr<arrow::Table>> result = table->RemoveColumn(column_index);
    return result.ValueOrDie();
}

/*----------------------------------------------------------------------------
* convertWKBToPoint
*----------------------------------------------------------------------------*/
OutputLib::wkbpoint_t ArrowSamplerImpl::convertWKBToPoint(const std::string& wkb_data)
{
    OutputLib::wkbpoint_t point;

    if(wkb_data.size() < sizeof(OutputLib::wkbpoint_t))
    {
        throw std::runtime_error("Invalid WKB data size.");
    }

    // Byte order is the first byte
    size_t offset = 0;
    std::memcpy(&point.byteOrder, wkb_data.data() + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);

    // Next four bytes are wkbType
    std::memcpy(&point.wkbType, wkb_data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

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
    else throw std::runtime_error("Unknown byte order.");

    // Next eight bytes are x coordinate
    std::memcpy(&point.x, wkb_data.data() + offset, sizeof(double));
    offset += sizeof(double);

    if((point.byteOrder == 0 && __BYTE_ORDER != __BIG_ENDIAN) ||
       (point.byteOrder == 1 && __BYTE_ORDER != __LITTLE_ENDIAN))
    {
        point.x = OsApi::swaplf(point.x);
    }

    // Next eight bytes are y coordinate
    std::memcpy(&point.y, wkb_data.data() + offset, sizeof(double));
    if((point.byteOrder == 0 && __BYTE_ORDER != __BIG_ENDIAN) ||
       (point.byteOrder == 1 && __BYTE_ORDER != __LITTLE_ENDIAN))
    {
        point.y = OsApi::swaplf(point.y);
    }

    return point;
}

/*----------------------------------------------------------------------------
* printParquetMetadata
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::printParquetMetadata(const char* file_path)
{
    std::shared_ptr<arrow::io::ReadableFile> infile;
    PARQUET_ASSIGN_OR_THROW(infile, arrow::io::ReadableFile::Open(file_path, arrow::default_memory_pool()));

    std::unique_ptr<parquet::arrow::FileReader> _reader;
    PARQUET_ASSIGN_OR_THROW(_reader, parquet::arrow::OpenFile(infile, arrow::default_memory_pool()));

    const std::shared_ptr<parquet::FileMetaData> file_metadata = _reader->parquet_reader()->metadata();
    print2term("***********************************************************\n");
    print2term("***********************************************************\n");
    print2term("***********************************************************\n");
    print2term("File Metadata:\n");
    print2term("  Version: %d\n", file_metadata->version());
    print2term("  Num Row Groups: %d\n", file_metadata->num_row_groups());
    print2term("  Num Columns: %d\n", file_metadata->num_columns());
    print2term("  Num Row Groups: %d\n", file_metadata->num_row_groups());
    print2term("  Num Rows: %ld\n", file_metadata->num_rows());
    print2term("  Created By: %s\n", file_metadata->created_by().c_str());
    print2term("  Key Value Metadata:\n");
    for(int i = 0; i < file_metadata->key_value_metadata()->size(); i++)
    {
        const std::string key = file_metadata->key_value_metadata()->key(i);
        const std::string value = file_metadata->key_value_metadata()->value(i);

        if(key == "ARROW:schema") continue;
        print2term("    %s:  %s\n", key.c_str(), value.c_str());
    }

    print2term("  Schema:\n");
    for(int i = 0; i < file_metadata->num_columns(); i++)
    {
        const std::shared_ptr<parquet::schema::ColumnPath> path = file_metadata->schema()->Column(i)->path();
        print2term("    %s\n", path->ToDotString().c_str());
    }
}

/*----------------------------------------------------------------------------
* createFileMap
*----------------------------------------------------------------------------*/
std::string ArrowSamplerImpl::createFileMap(void)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    /* Serialize into JSON */
    const std::vector<ArrowSampler::batch_sampler_t*>& samplers = arrowSampler->getBatchSamplers();
    for(ArrowSampler::batch_sampler_t* sampler : samplers)
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

/*----------------------------------------------------------------------------
* metadataToJson
*----------------------------------------------------------------------------*/
void ArrowSamplerImpl::metadataToJson(const std::shared_ptr<arrow::Table>& table, const char* file_path)
{
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    std::vector<const char*> keys2include = {"sliderule", "filemap"};

    const auto& metadata = table->schema()->metadata();
    for (int i = 0; i < metadata->size(); ++i)
    {
        // print2term("Key: %s, Value: %s\n", metadata->key(i).c_str(), metadata->value(i).c_str());
        if(std::find(keys2include.begin(), keys2include.end(), metadata->key(i)) == keys2include.end())
            continue;

        rapidjson::Value key(metadata->key(i).c_str(), allocator);
        rapidjson::Value value(metadata->value(i).c_str(), allocator);
        doc.AddMember(key, value, allocator);
    }

    /* Serialize the JSON document to string */
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    /* Write the JSON string to a file */
    FILE* jsonFile = fopen(file_path, "w");
    if(jsonFile)
    {
        fwrite(buffer.GetString(), 1, buffer.GetSize(), jsonFile);
        fclose(jsonFile);
    }
    else throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to open metadata file: %s", file_path);
}