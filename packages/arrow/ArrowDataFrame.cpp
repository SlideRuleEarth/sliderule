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
 * INCLUDE
 ******************************************************************************/

#include <parquet/arrow/schema.h>
#include <parquet/arrow/writer.h>
#include <parquet/properties.h>
#include <arrow/table.h>
#include <arrow/util/key_value_metadata.h>
#include <arrow/io/file.h>
#include <arrow/ipc/api.h>
#include <arrow/builder.h>
#include <parquet/file_writer.h>
#include <arrow/csv/writer.h>
#include <regex>

#include "OsApi.h"
#include "GeoDataFrame.h"
#include "FieldList.h"
#include "FieldArray.h"
#include "FieldColumn.h"
#include "ArrowDataFrame.h"
#include "ArrowFields.h"
#include "ArrowCommon.h"

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
* encode - T: field column type, B: arrow builder type
*----------------------------------------------------------------------------*/
template<class T, class B>
void encode(const FieldColumn<T>* field_column, vector<shared_ptr<arrow::Array>>& columns)
{
    B builder;

    const long num_rows = field_column->length();
    (void)builder.Reserve(num_rows);
    for(int i = 0; i < num_rows; i++)
    {
        builder.UnsafeAppend((*field_column)[i]);
    }

    shared_ptr<arrow::Array> arrow_column;
    (void)builder.Finish(&arrow_column);
    columns.push_back(arrow_column);
}

/*----------------------------------------------------------------------------
* encode - time8_t
*----------------------------------------------------------------------------*/
void encode(const FieldColumn<time8_t>* field_column, vector<shared_ptr<arrow::Array>>& columns)
{
    arrow::TimestampBuilder builder(arrow::timestamp(arrow::TimeUnit::NANO), arrow::default_memory_pool());

    const long num_rows = field_column->length();
    (void)builder.Reserve(num_rows);
    for(int i = 0; i < num_rows; i++)
    {
        builder.UnsafeAppend((*field_column)[i].nanoseconds);
    }

    shared_ptr<arrow::Array> arrow_column;
    (void)builder.Finish(&arrow_column);
    columns.push_back(arrow_column);
}

/*----------------------------------------------------------------------------
* encodeColumn - T: field column type, B: arrow builder type
*----------------------------------------------------------------------------*/
template<class T, class B>
void encodeColumn(const FieldColumn<FieldColumn<T>>* field_column, vector<shared_ptr<arrow::Array>>& columns)
{
    auto builder = make_shared<B>();

    const long num_rows = field_column->length();
    arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
    for(int i = 0; i < num_rows; i++)
    {
        const FieldColumn<T>& field = (*field_column)[i];
        const long num_elements = field.length();
        (void)list_builder.Append();
        for(int element = 0; element < num_elements; element++)
        {
            (void)builder->Append(field[element]);
        }
    }

    shared_ptr<arrow::Array> arrow_column;
    (void)list_builder.Finish(&arrow_column);
    columns.push_back(arrow_column);
}

/*----------------------------------------------------------------------------
* encodeColumn - time8_t
*----------------------------------------------------------------------------*/
void encodeColumn(const FieldColumn<FieldColumn<time8_t>>* field_column, vector<shared_ptr<arrow::Array>>& columns)
{
    auto builder = make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::NANO), arrow::default_memory_pool());

    const long num_rows = field_column->length();
    arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
    for(int i = 0; i < num_rows; i++)
    {
        const FieldColumn<time8_t>& field = (*field_column)[i];
        const long num_elements = field.length();
        (void)list_builder.Append();
        for(int element = 0; element < num_elements; element++)
        {
            (void)builder->Append(field[element].nanoseconds);
        }
    }

    shared_ptr<arrow::Array> arrow_column;
    (void)list_builder.Finish(&arrow_column);
    columns.push_back(arrow_column);
}

/*----------------------------------------------------------------------------
* encodeList - T: field list type, B: arrow builder type
*----------------------------------------------------------------------------*/
template<class T, class B>
void encodeList(const FieldColumn<FieldList<T>>* field_column, vector<shared_ptr<arrow::Array>>& columns)
{
    auto builder = make_shared<B>();

    const long num_rows = field_column->length();
    arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
    for(int i = 0; i < num_rows; i++)
    {
        const FieldList<T>& field = (*field_column)[i];
        const long num_elements = field.length();
        (void)list_builder.Append();
        for(int element = 0; element < num_elements; element++)
        {
            (void)builder->Append(field[element]);
        }
    }

    shared_ptr<arrow::Array> arrow_column;
    (void)list_builder.Finish(&arrow_column);
    columns.push_back(arrow_column);
}

/*----------------------------------------------------------------------------
* encodeList - time8_t
*----------------------------------------------------------------------------*/
void encodeList(const FieldColumn<FieldList<time8_t>>* field_column, vector<shared_ptr<arrow::Array>>& columns)
{
    auto builder = make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::NANO), arrow::default_memory_pool());

    const long num_rows = field_column->length();
    arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
    for(int i = 0; i < num_rows; i++)
    {
        const FieldList<time8_t>& field = (*field_column)[i];
        const long num_elements = field.length();
        (void)list_builder.Append();
        for(int element = 0; element < num_elements; element++)
        {
            (void)builder->Append(field[element].nanoseconds);
        }
    }

    shared_ptr<arrow::Array> arrow_column;
    (void)list_builder.Finish(&arrow_column);
    columns.push_back(arrow_column);
}

/*----------------------------------------------------------------------------
* encodeArray - T: field array type, B: arrow builder type
*----------------------------------------------------------------------------*/
template<class T, class B>
void encodeArray(const Field* field, vector<shared_ptr<arrow::Array>>& columns)
{
    auto builder = make_shared<B>();

    const long num_rows = field->length();
    arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
    for(int i = 0; i < num_rows; i++)
    {
        const FieldUnsafeArray<T>* field_array = dynamic_cast<const FieldUnsafeArray<T>*>(field->get(i));
        const long num_elements = field_array->size;
        (void)list_builder.Append();
        for(int element = 0; element < num_elements; element++)
        {
            (void)builder->Append(field_array->memPtr[element]);
        }
    }

    shared_ptr<arrow::Array> arrow_column;
    (void)list_builder.Finish(&arrow_column);
    columns.push_back(arrow_column);
}

/*----------------------------------------------------------------------------
* encodeArray - time8_t
*----------------------------------------------------------------------------*/
void encodeArrayTime8(const Field* field, vector<shared_ptr<arrow::Array>>& columns)
{
    auto builder = make_shared<arrow::TimestampBuilder>(arrow::timestamp(arrow::TimeUnit::NANO), arrow::default_memory_pool());

    const long num_rows = field->length();
    arrow::ListBuilder list_builder(arrow::default_memory_pool(), builder);
    for(int i = 0; i < num_rows; i++)
    {
        const FieldUnsafeArray<time8_t>* field_array = dynamic_cast<const FieldUnsafeArray<time8_t>*>(field->get(i));
        const long num_elements = field_array->size;
        (void)list_builder.Append();
        for(int element = 0; element < num_elements; element++)
        {
            (void)builder->Append(field_array->memPtr[element].nanoseconds);
        }
    }

    shared_ptr<arrow::Array> arrow_column;
    (void)list_builder.Finish(&arrow_column);
    columns.push_back(arrow_column);
}

/*----------------------------------------------------------------------------
* encodeGeometry
*----------------------------------------------------------------------------*/
void encodeGeometry(const GeoDataFrame& dataframe, vector<shared_ptr<arrow::Array>>& columns)
{
    const long num_rows = dataframe.length();
    const FieldColumn<double>* x = dataframe.getXColumn();
    const FieldColumn<double>* y = dataframe.getYColumn();

    assert(x);
    assert(y);

    if(!x || !y)
    {
        mlog(ERROR, "Attempting to build GeoDataFrame without x and y columns");
        return;
    }

    arrow::BinaryBuilder builder;
    (void)builder.Reserve(num_rows);
    (void)builder.ReserveData(num_rows * sizeof(ArrowCommon::wkbpoint_t));
    for(int i = 0; i < num_rows; i++)
    {
        ArrowCommon::wkbpoint_t point = {
            #ifdef __be__
            .byteOrder = 0,
            #else
            .byteOrder = 1,
            #endif
            .wkbType = 1,
            .x = (*x)[i],
            .y = (*y)[i]
        };
        builder.UnsafeAppend(reinterpret_cast<uint8_t*>(&point), sizeof(ArrowCommon::wkbpoint_t));
    }

    shared_ptr<arrow::Array> geo_column;
    (void)builder.Finish(&geo_column);
    columns.push_back(geo_column);
}

/*----------------------------------------------------------------------------
* buildFieldList
*----------------------------------------------------------------------------*/
void buildFieldList (vector<shared_ptr<arrow::Field>>& fields, const ArrowFields& parms, const GeoDataFrame& dataframe)
{
    // loop through columns in data frame
    const vector<string> column_names = dataframe.getColumnNames();
    for(const string& name: column_names)
    {
        Field* field = dataframe.getColumn(name.c_str());

        // check for geometry columns
        if(parms.format == ArrowFields::GEOPARQUET)
        {
            // skip over source columns for geometry as they will be added
            // separately as a part of the dedicated geometry column
            if (field->encoding & Field::X_COLUMN ||
                field->encoding & Field::Y_COLUMN)
            {
                continue;
            }
        }

        /* Add to Schema */
        if(field->type == Field::COLUMN)
        {
            const uint32_t field_encoding = field->getValueEncoding();
            const uint32_t type_encoding = field->getEncodedType();
            if(field_encoding & (Field::NESTED_ARRAY | Field::NESTED_LIST | Field::NESTED_COLUMN))
            {
                switch(type_encoding)
                {
                    case Field::BOOL:   fields.push_back(arrow::field(name, arrow::list(arrow::boolean())));    break;
                    case Field::INT8:   fields.push_back(arrow::field(name, arrow::list(arrow::int8())));       break;
                    case Field::INT16:  fields.push_back(arrow::field(name, arrow::list(arrow::int16())));      break;
                    case Field::INT32:  fields.push_back(arrow::field(name, arrow::list(arrow::int32())));      break;
                    case Field::INT64:  fields.push_back(arrow::field(name, arrow::list(arrow::int64())));      break;
                    case Field::UINT8:  fields.push_back(arrow::field(name, arrow::list(arrow::uint8())));      break;
                    case Field::UINT16: fields.push_back(arrow::field(name, arrow::list(arrow::uint16())));     break;
                    case Field::UINT32: fields.push_back(arrow::field(name, arrow::list(arrow::uint32())));     break;
                    case Field::UINT64: fields.push_back(arrow::field(name, arrow::list(arrow::uint64())));     break;
                    case Field::FLOAT:  fields.push_back(arrow::field(name, arrow::list(arrow::float32())));    break;
                    case Field::DOUBLE: fields.push_back(arrow::field(name, arrow::list(arrow::float64())));    break;
                    case Field::TIME8:  fields.push_back(arrow::field(name, arrow::list(arrow::timestamp(arrow::TimeUnit::NANO)))); break;
                    case Field::STRING: fields.push_back(arrow::field(name, arrow::list(arrow::utf8())));       break;
                    default: mlog(WARNING, "Skipping field %s with encoding %d", name.c_str(), field->getValueEncoding());                break;
                }
            }
            else
            {
                switch(type_encoding)
                {
                    case Field::BOOL:   fields.push_back(arrow::field(name, arrow::boolean()));                 break;
                    case Field::INT8:   fields.push_back(arrow::field(name, arrow::int8()));                    break;
                    case Field::INT16:  fields.push_back(arrow::field(name, arrow::int16()));                   break;
                    case Field::INT32:  fields.push_back(arrow::field(name, arrow::int32()));                   break;
                    case Field::INT64:  fields.push_back(arrow::field(name, arrow::int64()));                   break;
                    case Field::UINT8:  fields.push_back(arrow::field(name, arrow::uint8()));                   break;
                    case Field::UINT16: fields.push_back(arrow::field(name, arrow::uint16()));                  break;
                    case Field::UINT32: fields.push_back(arrow::field(name, arrow::uint32()));                  break;
                    case Field::UINT64: fields.push_back(arrow::field(name, arrow::uint64()));                  break;
                    case Field::FLOAT:  fields.push_back(arrow::field(name, arrow::float32()));                 break;
                    case Field::DOUBLE: fields.push_back(arrow::field(name, arrow::float64()));                 break;
                    case Field::TIME8:  fields.push_back(arrow::field(name, arrow::timestamp(arrow::TimeUnit::NANO))); break;
                    case Field::STRING: fields.push_back(arrow::field(name, arrow::utf8()));                    break;
                    default: mlog(WARNING, "Skipping field %s with encoding %d", name.c_str(), field->getValueEncoding());                break;
                }
            }
        }
        else
        {
            mlog(WARNING, "Skipping field %s with type %d", name.c_str(), static_cast<int>(field->type));
        }
    }
}

/*----------------------------------------------------------------------------
* appendGeoMetaData
*----------------------------------------------------------------------------*/
void appendGeoMetaData (const std::shared_ptr<arrow::KeyValueMetadata>& metadata)
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
    metadata->Append("geo", geostr);
}

/*----------------------------------------------------------------------------
* appendPandasMetaData
*----------------------------------------------------------------------------*/
void appendPandasMetaData (const char* index_column_name, const shared_ptr<arrow::KeyValueMetadata>& metadata, const shared_ptr<arrow::Schema>& schema)
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
    const int num_columns = schema->fields().size();
    for(const shared_ptr<arrow::Field>& field: schema->fields())
    {
        const string& field_name = field->name();
        const shared_ptr<arrow::DataType>& field_type = field->type();

        /* Initialize Column String */
        string columnstr(R"json({"name": "_NAME_", "field_name": "_NAME_", "pandas_type": "_PTYPE_", "numpy_type": "_NTYPE_", "metadata": null})json");
        const char* pandas_type = NULL;
        const char* numpy_type = NULL;
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
    pandasstr = std::regex_replace(pandasstr, std::regex("_INDEX_"), index_column_name);
    pandasstr = std::regex_replace(pandasstr, std::regex("_COLUMNS_"), columns.c_str());

    /* Append Meta String */
    metadata->Append("pandas", pandasstr);
}

/*----------------------------------------------------------------------------
* processDataFrame
*----------------------------------------------------------------------------*/
void processDataFrame (vector<shared_ptr<arrow::Array>>& columns, const ArrowFields& parms, const GeoDataFrame& dataframe, const uint32_t trace_id)
{
    // build columns
    Dictionary<FieldDictionary::entry_t>::Iterator iter(dataframe.getColumns());
    for(int f = 0; f < iter.length; f++)
    {
        const char* name = iter[f].key;
        const Field* field = iter[f].value.field;

        // check for geoparquet format
        if( (parms.format == ArrowFields::GEOPARQUET) &&
            (field->encoding & Field::X_COLUMN ||
             field->encoding & Field::Y_COLUMN) )
        {
            // geometry columns will be encoded below
            continue;
        }

        // encode field to arrow
        const uint32_t field_trace_id = start_trace(INFO, trace_id, "encodeFields", "{\"field\": %s}", name);
        if(field->type == Field::COLUMN)
        {
            switch(field->getValueEncoding())
            {
                case Field::INT8:                           encode<int8_t,   arrow::Int8Builder>   (dynamic_cast<const FieldColumn<int8_t>*>(field), columns);   break;
                case Field::INT16:                          encode<int16_t,  arrow::Int16Builder>  (dynamic_cast<const FieldColumn<int16_t>*>(field), columns);  break;
                case Field::INT32:                          encode<int32_t,  arrow::Int32Builder>  (dynamic_cast<const FieldColumn<int32_t>*>(field), columns);  break;
                case Field::INT64:                          encode<int64_t,  arrow::Int64Builder>  (dynamic_cast<const FieldColumn<int64_t>*>(field), columns);  break;
                case Field::UINT8:                          encode<uint8_t,  arrow::UInt8Builder>  (dynamic_cast<const FieldColumn<uint8_t>*>(field), columns);  break;
                case Field::UINT16:                         encode<uint16_t, arrow::UInt16Builder> (dynamic_cast<const FieldColumn<uint16_t>*>(field), columns); break;
                case Field::UINT32:                         encode<uint32_t, arrow::UInt32Builder> (dynamic_cast<const FieldColumn<uint32_t>*>(field), columns); break;
                case Field::UINT64:                         encode<uint64_t, arrow::UInt64Builder> (dynamic_cast<const FieldColumn<uint64_t>*>(field), columns); break;
                case Field::FLOAT:                          encode<float,    arrow::FloatBuilder>  (dynamic_cast<const FieldColumn<float>*>(field), columns);    break;
                case Field::DOUBLE:                         encode<double,   arrow::DoubleBuilder> (dynamic_cast<const FieldColumn<double>*>(field), columns);   break;
                case Field::TIME8:                          encode(dynamic_cast<const FieldColumn<time8_t>*>(field), columns); break;
                case Field::STRING:                         encode<string,   arrow::StringBuilder> (dynamic_cast<const FieldColumn<string>*>(field), columns);   break;

                case Field::NESTED_ARRAY | Field::INT8:     encodeArray<int8_t,   arrow::Int8Builder>   (field, columns); break;
                case Field::NESTED_ARRAY | Field::INT16:    encodeArray<int16_t,  arrow::Int16Builder>  (field, columns); break;
                case Field::NESTED_ARRAY | Field::INT32:    encodeArray<int32_t,  arrow::Int32Builder>  (field, columns); break;
                case Field::NESTED_ARRAY | Field::INT64:    encodeArray<int64_t,  arrow::Int64Builder>  (field, columns); break;
                case Field::NESTED_ARRAY | Field::UINT8:    encodeArray<uint8_t,  arrow::UInt8Builder>  (field, columns); break;
                case Field::NESTED_ARRAY | Field::UINT16:   encodeArray<uint16_t, arrow::UInt16Builder> (field, columns); break;
                case Field::NESTED_ARRAY | Field::UINT32:   encodeArray<uint32_t, arrow::UInt32Builder> (field, columns); break;
                case Field::NESTED_ARRAY | Field::UINT64:   encodeArray<uint64_t, arrow::UInt64Builder> (field, columns); break;
                case Field::NESTED_ARRAY | Field::FLOAT:    encodeArray<float,    arrow::FloatBuilder>  (field, columns); break;
                case Field::NESTED_ARRAY | Field::DOUBLE:   encodeArray<double,   arrow::DoubleBuilder> (field, columns); break;
                case Field::NESTED_ARRAY | Field::TIME8:    encodeArrayTime8(field, columns); break;
                case Field::NESTED_ARRAY | Field::STRING:   encodeArray<string,   arrow::StringBuilder> (field, columns); break;

                case Field::NESTED_LIST | Field::INT8:      encodeList<int8_t,   arrow::Int8Builder>   (dynamic_cast<const FieldColumn<FieldList<int8_t>>*>(field), columns);   break;
                case Field::NESTED_LIST | Field::INT16:     encodeList<int16_t,  arrow::Int16Builder>  (dynamic_cast<const FieldColumn<FieldList<int16_t>>*>(field), columns);  break;
                case Field::NESTED_LIST | Field::INT32:     encodeList<int32_t,  arrow::Int32Builder>  (dynamic_cast<const FieldColumn<FieldList<int32_t>>*>(field), columns);  break;
                case Field::NESTED_LIST | Field::INT64:     encodeList<int64_t,  arrow::Int64Builder>  (dynamic_cast<const FieldColumn<FieldList<int64_t>>*>(field), columns);  break;
                case Field::NESTED_LIST | Field::UINT8:     encodeList<uint8_t,  arrow::UInt8Builder>  (dynamic_cast<const FieldColumn<FieldList<uint8_t>>*>(field), columns);  break;
                case Field::NESTED_LIST | Field::UINT16:    encodeList<uint16_t, arrow::UInt16Builder> (dynamic_cast<const FieldColumn<FieldList<uint16_t>>*>(field), columns); break;
                case Field::NESTED_LIST | Field::UINT32:    encodeList<uint32_t, arrow::UInt32Builder> (dynamic_cast<const FieldColumn<FieldList<uint32_t>>*>(field), columns); break;
                case Field::NESTED_LIST | Field::UINT64:    encodeList<uint64_t, arrow::UInt64Builder> (dynamic_cast<const FieldColumn<FieldList<uint64_t>>*>(field), columns); break;
                case Field::NESTED_LIST | Field::FLOAT:     encodeList<float,    arrow::FloatBuilder>  (dynamic_cast<const FieldColumn<FieldList<float>>*>(field), columns);    break;
                case Field::NESTED_LIST | Field::DOUBLE:    encodeList<double,   arrow::DoubleBuilder> (dynamic_cast<const FieldColumn<FieldList<double>>*>(field), columns);   break;
                case Field::NESTED_LIST | Field::TIME8:     encodeList(dynamic_cast<const FieldColumn<FieldList<time8_t>>*>(field), columns); break;
                case Field::NESTED_LIST | Field::STRING:    encodeList<string,   arrow::StringBuilder> (dynamic_cast<const FieldColumn<FieldList<string>>*>(field), columns);   break;

                case Field::NESTED_COLUMN | Field::INT8:    encodeColumn<int8_t,   arrow::Int8Builder>   (dynamic_cast<const FieldColumn<FieldColumn<int8_t>>*>(field), columns);   break;
                case Field::NESTED_COLUMN | Field::INT16:   encodeColumn<int16_t,  arrow::Int16Builder>  (dynamic_cast<const FieldColumn<FieldColumn<int16_t>>*>(field), columns);  break;
                case Field::NESTED_COLUMN | Field::INT32:   encodeColumn<int32_t,  arrow::Int32Builder>  (dynamic_cast<const FieldColumn<FieldColumn<int32_t>>*>(field), columns);  break;
                case Field::NESTED_COLUMN | Field::INT64:   encodeColumn<int64_t,  arrow::Int64Builder>  (dynamic_cast<const FieldColumn<FieldColumn<int64_t>>*>(field), columns);  break;
                case Field::NESTED_COLUMN | Field::UINT8:   encodeColumn<uint8_t,  arrow::UInt8Builder>  (dynamic_cast<const FieldColumn<FieldColumn<uint8_t>>*>(field), columns);  break;
                case Field::NESTED_COLUMN | Field::UINT16:  encodeColumn<uint16_t, arrow::UInt16Builder> (dynamic_cast<const FieldColumn<FieldColumn<uint16_t>>*>(field), columns); break;
                case Field::NESTED_COLUMN | Field::UINT32:  encodeColumn<uint32_t, arrow::UInt32Builder> (dynamic_cast<const FieldColumn<FieldColumn<uint32_t>>*>(field), columns); break;
                case Field::NESTED_COLUMN | Field::UINT64:  encodeColumn<uint64_t, arrow::UInt64Builder> (dynamic_cast<const FieldColumn<FieldColumn<uint64_t>>*>(field), columns); break;
                case Field::NESTED_COLUMN | Field::FLOAT:   encodeColumn<float,    arrow::FloatBuilder>  (dynamic_cast<const FieldColumn<FieldColumn<float>>*>(field), columns);    break;
                case Field::NESTED_COLUMN | Field::DOUBLE:  encodeColumn<double,   arrow::DoubleBuilder> (dynamic_cast<const FieldColumn<FieldColumn<double>>*>(field), columns);   break;
                case Field::NESTED_COLUMN | Field::TIME8:   encodeColumn(dynamic_cast<const FieldColumn<FieldColumn<time8_t>>*>(field), columns); break;
                case Field::NESTED_COLUMN | Field::STRING:  encodeColumn<string,   arrow::StringBuilder> (dynamic_cast<const FieldColumn<FieldColumn<string>>*>(field), columns);   break;

                default: mlog(WARNING, "Skipping column %s with encoding %d", name, static_cast<int>(field->encoding)); break;
            }
        }
        else
        {
            mlog(WARNING, "Skipping field %s of type %d", name, static_cast<int>(field->type));
        }
        stop_trace(INFO, field_trace_id);
    }

    // build geo columns
    if(parms.format == ArrowFields::GEOPARQUET)
    {
        const uint32_t geo_trace_id = start_trace(INFO, trace_id, "encodeGeometry", "%s", "{}");
        encodeGeometry(dataframe, columns);
        stop_trace(INFO, geo_trace_id);
    }
}

/******************************************************************************
 * CLASS DATA
 ******************************************************************************/

const char* ArrowDataFrame::OBJECT_TYPE = "ArrowDataFrame";
const char* ArrowDataFrame::LUA_META_NAME = "ArrowDataFrame";
const struct luaL_Reg ArrowDataFrame::LUA_META_TABLE[] = {
    {"export",  luaExport},
    {"import",  luaImport}, // TODO
    {NULL,      NULL}
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(...)
 *----------------------------------------------------------------------------*/
int ArrowDataFrame::luaCreate (lua_State* L)
{
    RequestFields* _parms = NULL;
    GeoDataFrame* _dataframe = NULL;
    try
    {
        _parms = dynamic_cast<RequestFields*>(getLuaObject(L, 1, RequestFields::OBJECT_TYPE));
        _dataframe = dynamic_cast<GeoDataFrame*>(getLuaObject(L, 2, GeoDataFrame::OBJECT_TYPE));
        return createLuaObject(L, new ArrowDataFrame(L, _parms, _dataframe));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_dataframe) _dataframe->releaseLuaObject();
        mlog(e.level(), "Error creating ArrowDataFrame: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaExport - export()
 *----------------------------------------------------------------------------*/
int ArrowDataFrame::luaExport (lua_State* L)
{
    bool status = false;
    const char* unique_filename = ArrowCommon::getUniqueFileName(NULL);

    try
    {
        // get lua parameters
        ArrowDataFrame* lua_obj = dynamic_cast<ArrowDataFrame*>(getLuaSelf(L, 1));
        const char* filename = getLuaString(L, 2, true, unique_filename);
        const ArrowFields::format_t format = static_cast<ArrowFields::format_t>(getLuaInteger(L, 3, true, lua_obj->parms->output.format.value));

        // get references
        const RequestFields& parms = *lua_obj->parms;
        const GeoDataFrame& dataframe = *lua_obj->dataframe;
        const ArrowFields& arrow_parms = parms.output;

        // start trace
        const uint32_t parent_trace_id = EventLib::grabId();
        const uint32_t trace_id = start_trace(INFO, parent_trace_id, "ArrowDataFrame", "{\"num_rows\": %ld}", dataframe.length());

        // process dataframe to arrow array
        vector<shared_ptr<arrow::Array>> columns;
        processDataFrame(columns, arrow_parms, dataframe, trace_id);

        // create schema
        vector<shared_ptr<arrow::Field>> field_list;
        buildFieldList(field_list, arrow_parms, dataframe);
        shared_ptr<arrow::Schema> schema = make_shared<arrow::Schema>(field_list);

        // write out table
        const uint32_t write_trace_id = start_trace(INFO, trace_id, "write_table", "%s", "{}");
        if(format == ArrowFields::GEOPARQUET || format == ArrowFields::PARQUET)
        {
            // set arrow output stream
            shared_ptr<arrow::io::FileOutputStream> file_output_stream;
            auto _result = arrow::io::FileOutputStream::Open(filename);
            if(_result.ok())
            {
                file_output_stream = _result.ValueOrDie();

                // set writer properties
                parquet::WriterProperties::Builder writer_props_builder;
                writer_props_builder.compression(parquet::Compression::SNAPPY);
                writer_props_builder.version(parquet::ParquetVersion::PARQUET_2_6);
                const shared_ptr<parquet::WriterProperties> writer_props = writer_props_builder.build();

                // set arrow writer properties
                auto arrow_writer_props = parquet::ArrowWriterProperties::Builder().store_schema()->build();

                // set metadata
                auto metadata = schema->metadata() ? schema->metadata()->Copy() : std::make_shared<arrow::KeyValueMetadata>();
                if(parms.output.format == ArrowFields::GEOPARQUET) appendGeoMetaData(metadata);
                appendPandasMetaData(dataframe.getTimeColumnName().c_str(), metadata, schema);
                metadata->Append("sliderule", parms.toJson());
                metadata->Append("meta", dataframe.metaFields.toJson());
                schema = schema->WithMetadata(metadata);

                // create parquet writer
                auto result = parquet::arrow::FileWriter::Open(*schema, ::arrow::default_memory_pool(), file_output_stream, writer_props, arrow_writer_props);
                if(result.ok())
                {
                    unique_ptr<parquet::arrow::FileWriter> parquet_writer = std::move(result).ValueOrDie();
                    const shared_ptr<arrow::Table> table = arrow::Table::Make(schema, columns);

                    // validate table
                    if(arrow_parms.withValidation)
                    {
                        const arrow::Status validation_status = table->ValidateFull();
                        if (!validation_status.ok()) {
                            mlog(CRITICAL, "Parquet table validation failed: %s\n", validation_status.ToString().c_str());
                        }
                    }

                    // write table
                    const arrow::Status s = parquet_writer->WriteTable(*table);
                    (void)parquet_writer->Close();
                    if(s.ok())
                    {
                        status = true;
                    }
                    else
                    {
                        mlog(CRITICAL, "Failed to write parquet table: %s", s.CodeAsString().c_str());
                    }
                }
                else
                {
                    mlog(CRITICAL, "Failed to open parquet writer: %s", result.status().ToString().c_str());
                }
            }
            else
            {
                mlog(CRITICAL, "Failed to open file output stream: %s", _result.status().ToString().c_str());
            }
        }
        else if(format == ArrowFields::FEATHER)
        {
            // create feather writer
            auto result = arrow::io::FileOutputStream::Open(filename);
            if(result.ok())
            {
                // write table
                const shared_ptr<arrow::io::FileOutputStream> feather_writer = result.ValueOrDie();
                const shared_ptr<arrow::Table> table = arrow::Table::Make(schema, columns);
                const arrow::Status s = arrow::ipc::feather::WriteTable(*table, feather_writer.get());
                (void)feather_writer->Close();
                if(s.ok())
                {
                    status = true;
                }
                else
                {
                    mlog(CRITICAL, "Failed to write feather table: %s", s.CodeAsString().c_str());
                }
            }
            else
            {
                mlog(CRITICAL, "Failed to open feather writer: %s", result.status().ToString().c_str());
            }
        }
        else if(format == ArrowFields::CSV)
        {
            // create csv writer
            auto result = arrow::io::FileOutputStream::Open(filename);
            if(result.ok())
            {
                // write table
                const shared_ptr<arrow::io::FileOutputStream> csv_writer = result.ValueOrDie();
                const shared_ptr<arrow::Table> table = arrow::Table::Make(schema, columns);
                const arrow::Status s = arrow::csv::WriteCSV(*table, arrow::csv::WriteOptions::Defaults(), csv_writer.get());
                (void)csv_writer->Close();
                if(s.ok())
                {
                    status = true;
                }
                else
                {
                    mlog(CRITICAL, "Failed to write CSV table: %s", s.CodeAsString().c_str());
                }
            }
            else
            {
                mlog(CRITICAL, "Failed to open csv writer: %s", result.status().ToString().c_str());
            }
        }
        else
        {
            mlog(CRITICAL, "Unsupported format: %d", format);
        }
        stop_trace(INFO, write_trace_id);

        // stop trace
        stop_trace(INFO, trace_id);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error exporting %s: %s", OBJECT_TYPE, e.what());
    }

    // clean up
    delete [] unique_filename;

    // return status
    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * luaImport - import()
 *----------------------------------------------------------------------------*/
int ArrowDataFrame::luaImport (lua_State* L)
{
    bool status = true;
    try
    {
        ArrowDataFrame* lua_obj = dynamic_cast<ArrowDataFrame*>(getLuaSelf(L, 1));
        (void)lua_obj;

        throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported");
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error importing %s: %s", OBJECT_TYPE, e.what());
        status = false;
    }

    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowDataFrame::ArrowDataFrame(lua_State* L, RequestFields* _parms, GeoDataFrame* _dataframe):
    LuaObject (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    parms(_parms),
    dataframe(_dataframe)
{
    assert(parms);
    assert(dataframe);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArrowDataFrame::~ArrowDataFrame(void)
{
    parms->releaseLuaObject();
    dataframe->releaseLuaObject();
}
