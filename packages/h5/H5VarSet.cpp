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
        const string& field_name = variable_list[i];
        const FString dataset_name("%s%s%s", group ? group : "", group ? "/" : "", field_name.c_str());
        H5DArray* array = new H5DArray(context, dataset_name.c_str(), col, startrow, numrows);
        const bool status = variables.add(field_name.c_str(), array);
        if(!status)
        {
            delete array;
            throw RunTimeException(CRITICAL, RTE_ERROR, "failed to add dataset <%s>", dataset_name.c_str());
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
        if(!gdf->addColumn(dataset_name, array->elementType()))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "failed to join array for <%s>", dataset_name);
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
        if(element != static_cast<int32_t>(INVALID_KEY))
        {
            gdf->appendFromBuffer(dataset_name, array->referenceElement(element), array->elementSize());
        }
        else
        {
            const uint8_t nodata_buf[8] = {0,0,0,0,0,0,0,0};
            gdf->appendFromBuffer(dataset_name, nodata_buf, 8);
        }
    }
}
