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

#include "OsApi.h"
#include "H5Coro.h"
#include "H5VarSet.h"
#include <vector>

using std::vector;

/*----------------------------------------------------------------------------
 * Local Helpers
 *----------------------------------------------------------------------------*/
namespace
{
    long compute_row_size(const H5DArray* array)
    {
        long row_size = array->rowSize();
        if(row_size <= 1 && array->h5f)
        {
            const int64_t rows = array->h5f->info.shape[0];
            if(rows > 0 && array->numElements() % rows == 0)
            {
                row_size = array->numElements() / rows;
            }
        }
        return row_size;
    }

    template<typename T>
    FieldColumn<FieldList<T>>* get_list_column(GeoDataFrame* gdf, const char* name)
    {
        FieldColumn<FieldList<T>>* column = dynamic_cast<FieldColumn<FieldList<T>>*>(gdf->getColumn(name, true));
        if(!column)
        {
            column = new FieldColumn<FieldList<T>>();
            if(!gdf->addColumn(name, column, true))
            {
                delete column;
                throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to add list column <%s>", name);
            }
        }
        return column;
    }

    template<typename T>
    void append_list_row(FieldColumn<FieldList<T>>* column, const H5DArray* array, long element, long row_size, bool nodata)
    {
        FieldList<T> values;

        if(!nodata)
        {
            vector<uint8_t> row_buffer(row_size * array->elementSize());
            array->serializeRow(row_buffer.data(), element);
            const T* typed = reinterpret_cast<const T*>(row_buffer.data());
            for(long i = 0; i < row_size; i++)
            {
                values.append(typed[i]);
            }
        }
        else
        {
            for(long i = 0; i < row_size; i++)
            {
                values.append(static_cast<T>(0));
            }
        }

        column->append(values);
    }

    void add_list_column(GeoDataFrame* gdf, const char* name, RecordObject::fieldType_t type)
    {
        switch(type)
        {
            case RecordObject::INT8:    (void)get_list_column<int8_t>(gdf, name);   break;
            case RecordObject::INT16:   (void)get_list_column<int16_t>(gdf, name);  break;
            case RecordObject::INT32:   (void)get_list_column<int32_t>(gdf, name);  break;
            case RecordObject::INT64:   (void)get_list_column<int64_t>(gdf, name);  break;
            case RecordObject::UINT8:   (void)get_list_column<uint8_t>(gdf, name);  break;
            case RecordObject::UINT16:  (void)get_list_column<uint16_t>(gdf, name); break;
            case RecordObject::UINT32:  (void)get_list_column<uint32_t>(gdf, name); break;
            case RecordObject::UINT64:  (void)get_list_column<uint64_t>(gdf, name); break;
            case RecordObject::FLOAT:   (void)get_list_column<float>(gdf, name);    break;
            case RecordObject::DOUBLE:  (void)get_list_column<double>(gdf, name);   break;
            case RecordObject::TIME8:   (void)get_list_column<time8_t>(gdf, name);  break;
            default: throw RunTimeException(CRITICAL, RTE_FAILURE, "unsupported list column type for %s: %d", name, type);
        }
    }

    void append_list_column(GeoDataFrame* gdf, const char* name, const H5DArray* array, long element, long row_size, bool nodata)
    {
        switch(array->elementType())
        {
            case RecordObject::INT8:    append_list_row<int8_t>   (get_list_column<int8_t>(gdf, name),    array, element, row_size, nodata);  break;
            case RecordObject::INT16:   append_list_row<int16_t>  (get_list_column<int16_t>(gdf, name),   array, element, row_size, nodata); break;
            case RecordObject::INT32:   append_list_row<int32_t>  (get_list_column<int32_t>(gdf, name),   array, element, row_size, nodata); break;
            case RecordObject::INT64:   append_list_row<int64_t>  (get_list_column<int64_t>(gdf, name),   array, element, row_size, nodata); break;
            case RecordObject::UINT8:   append_list_row<uint8_t>  (get_list_column<uint8_t>(gdf, name),   array, element, row_size, nodata); break;
            case RecordObject::UINT16:  append_list_row<uint16_t> (get_list_column<uint16_t>(gdf, name),  array, element, row_size, nodata); break;
            case RecordObject::UINT32:  append_list_row<uint32_t> (get_list_column<uint32_t>(gdf, name),  array, element, row_size, nodata); break;
            case RecordObject::UINT64:  append_list_row<uint64_t> (get_list_column<uint64_t>(gdf, name),  array, element, row_size, nodata); break;
            case RecordObject::FLOAT:   append_list_row<float>    (get_list_column<float>(gdf, name),     array, element, row_size, nodata); break;
            case RecordObject::DOUBLE:  append_list_row<double>   (get_list_column<double>(gdf, name),    array, element, row_size, nodata); break;
            case RecordObject::TIME8:   append_list_row<time8_t>  (get_list_column<time8_t>(gdf, name),   array, element, row_size, nodata); break;
            default: throw RunTimeException(CRITICAL, RTE_FAILURE, "unsupported element type for list append on %s: %d", name, array->elementType());
        }
    }
}

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/


/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5VarSet::H5VarSet(const FieldList<string>& variable_list, H5Coro::Context* context, const char* group, long col, long startrow, long numrows):
    variables(getDictSize(variable_list.length()))
{
    for(int i = 0; i < variable_list.length(); i++)
    {
        const string& field_name = GeoDataFrame::extractColumnName(variable_list[i]);
        const FString dataset_name("%s%s%s", group ? group : "", group ? "/" : "", field_name.c_str());
        H5DArray* array = new H5DArray(context, dataset_name.c_str(), col, startrow, numrows);
        const bool status = variables.add(field_name.c_str(), array);
        if(!status)
        {
            delete array;
            throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to add dataset <%s>", dataset_name.c_str());
        }
    }
}

/*----------------------------------------------------------------------------
 * joinToGDF
 *----------------------------------------------------------------------------*/
void H5VarSet::joinToGDF(GeoDataFrame* gdf, int timeout_ms, bool throw_exception)
{
    H5DArray* array = NULL;
    const char* dataset_name = variables.first(&array);
    while(dataset_name != NULL)
    {
        array->join(timeout_ms, throw_exception);
        const long row_size = compute_row_size(array);
        if(row_size > 1)
        {
            add_list_column(gdf, dataset_name, array->elementType());
        }
        else
        {
            if(!gdf->addNewColumn(dataset_name, array->elementType()))
            {
                throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to join array for <%s>", dataset_name);
            }
        }
        dataset_name = variables.next(&array);
    }
}

/*----------------------------------------------------------------------------
 * addToGDF
 *----------------------------------------------------------------------------*/
void H5VarSet::addToGDF(GeoDataFrame* gdf, long element) const
{
    Dictionary<H5DArray*>::Iterator iter(variables);
    for(int i = 0; i < iter.length; i++)
    {
        const char* dataset_name = iter[i].key;
        H5DArray* array = iter[i].value;
        const long row_size = compute_row_size(array);
        const bool multidim = (row_size > 1);
        if(element != static_cast<int32_t>(INVALID_KEY))
        {
            if(multidim)
            {
                append_list_column(gdf, dataset_name, array, element, row_size, false);
            }
            else
            {
                gdf->appendFromBuffer(dataset_name, array->referenceElement(element), array->elementSize());
            }
        }
        else
        {
            if(multidim)
            {
                append_list_column(gdf, dataset_name, array, element, row_size, true);
            }
            else
            {
                const uint8_t nodata_buf[8] = {0,0,0,0,0,0,0,0};
                gdf->appendFromBuffer(dataset_name, nodata_buf, 8);
            }
        }
    }
}
