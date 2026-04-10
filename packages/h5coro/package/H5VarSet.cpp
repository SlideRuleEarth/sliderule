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
#include "H5CoroLib.h"
#include "H5VarSet.h"

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/


/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5VarSet::H5VarSet(const FieldList<string>& variable_list, H5Coro::Context* context, const char* group, long col, long startrow, long numrows):
    variables(getDictSize(variable_list.length()))
{
    // handle trailing slash in group name
    const char* separator = "";
    if(group)
    {
        const int str_size = StringLib::size(group);
        if(str_size > 0)
        {
            if(group[str_size - 1] != '/')
            {
                separator = "/";
            }
        }
    }

    // populate variables
    for(int i = 0; i < variable_list.length(); i++)
    {
        const string& field_name = GeoDataFrame::extractColumnName(variable_list[i]);
        const FString dataset_name("%s%s%s", group ? group : "", separator, field_name.c_str());
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
bool H5VarSet::joinToGDF(GeoDataFrame* gdf, int timeout_ms, bool throw_exception)
{
    bool status = true;
    H5DArray* array = NULL;
    const char* dataset_name = variables.first(&array);
    while(dataset_name != NULL)
    {
        array->join(timeout_ms, throw_exception);

        const uint32_t encoding = array->numDimensions() > 1 ?
                                  (Field::NESTED_LIST | static_cast<uint32_t>(array->elementType())) :
                                  static_cast<uint32_t>(array->elementType());

        if(!gdf->addNewColumn(dataset_name, encoding))
        {
            status = false;
            if(throw_exception)
            {
                throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to add column for <%s>", dataset_name);
            }
        }
        dataset_name = variables.next(&array);
    }
    return status;
}

/*----------------------------------------------------------------------------
 * addToGDF
 *----------------------------------------------------------------------------*/
void H5VarSet::addToGDF(GeoDataFrame* gdf, long element) const
{
    Dictionary<H5DArray*>::Iterator iter(variables);
    vector<uint8_t> row_buffer;

    for(int i = 0; i < iter.length; i++)
    {
        const char* dataset_name = iter[i].key;
        H5DArray* array = iter[i].value;
        const bool multidim = array->numDimensions() > 1;
        const uint32_t encoding = multidim ?
                                  (Field::NESTED_LIST | static_cast<uint32_t>(array->elementType())) :
                                  static_cast<uint32_t>(array->elementType());

        const bool nodata = (element == static_cast<int32_t>(INVALID_KEY));

        const uint8_t* data_ptr = NULL;
        long size;

        if(multidim)
        {
            size = static_cast<long>(array->rowSize() * array->elementSize());
            if(!nodata)
            {
                row_buffer.resize(static_cast<size_t>(size));
                array->serializeRow(row_buffer.data(), element);
                data_ptr = row_buffer.data();
            }
        }
        else
        {

            size = array->elementSize();
            if(!nodata) data_ptr = array->referenceElement(element);
        }

        if(gdf->appendFromBuffer(dataset_name, data_ptr, size, encoding, nodata) == 0)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to add data for <%s>", dataset_name);
        }
    }
}

/*----------------------------------------------------------------------------
 * addToGDF
 *----------------------------------------------------------------------------*/
long H5VarSet::addToGDF(GeoDataFrame* gdf) const
{
    long rows = 0;
    Dictionary<H5DArray*>::Iterator iter(variables);
    for(int i = 0; i < iter.length; i++)
    {
        const char* dataset_name = iter[i].key;
        H5DArray* array = iter[i].value;
        const bool multidim = array->numDimensions() > 1;
        const uint32_t encoding = multidim ?
                                  (Field::NESTED_LIST | static_cast<uint32_t>(array->elementType())) :
                                  static_cast<uint32_t>(array->elementType());
        const long dataset_rows = gdf->appendFromBuffer(dataset_name, array->h5f->info.data, array->h5f->info.datasize, encoding, false);
        if(dataset_rows == 0)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to add array data for <%s>", dataset_name);
        }
        else if(rows == 0)
        {
            rows = dataset_rows;
        }
        else if(rows != dataset_rows)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "mismatched number of rows in array for <%s>: expected %ld, got %ld", dataset_name, dataset_rows, rows);
        }
    }
    return rows;
}
