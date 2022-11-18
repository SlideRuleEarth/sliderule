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

#include <arrow/api.h>

#include "core.h"
#include "icesat2.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArrowBuilder::LuaMetaName = "ArrowBuilder";
const struct luaL_Reg ArrowBuilder::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - :arrow(<outq name>)
 *----------------------------------------------------------------------------*/
int ArrowBuilder::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* outq_name = getLuaString(L, 1);

        /* Create ATL06 Dispatch */
        return createLuaObject(L, new ArrowBuilder(L, outq_name));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ArrowBuilder::init (void)
{
    defineTableSchema(Atl06Dispatch::atRecType);
    defineTableSchema(Atl06Dispatch::atCompactRecType);
    defineTableSchema(Atl03Reader::exRecType);
}

/******************************************************************************
 * PRIVATE METOHDS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowBuilder::ArrowBuilder (lua_State* L, const char* outq_name):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    assert(outq_name);

    /* Initialize Publisher */
    outQ = new Publisher(outq_name);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
ArrowBuilder::~ArrowBuilder(void)
{
    delete outQ;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    const char* rectype = record->getRecordType();

         if(StringLib::match(rectype, Atl06Dispatch::atRecType))        return buildAtl06Table((Atl06Dispatch::atl06_t*)record->getRecordData());
    else if(StringLib::match(rectype, Atl06Dispatch::atCompactRecType)) return buildAtl06CompactTable((Atl06Dispatch::atl06_compact_t*)record->getRecordData());
    else if(StringLib::match(rectype, Atl03Reader::exRecType))          return buildAtl03ExtentTable((Atl03Reader::extent_t*)record->getRecordData());
    else
    {
        mlog(CRITICAL, "Unexpected record received: %s", rectype);
    }

    return false;
}

/*----------------------------------------------------------------------------
 * processTimeout
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::processTimeout (void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * processTermination
 *
 *  Note that RecordDispatcher will only call this once
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::processTermination (void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * buildAtl06Table
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::buildAtl06Table (Atl06Dispatch::atl06_t* rec)
{
    return true;
}

/*----------------------------------------------------------------------------
 * buildAtl06CompactTable
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::buildAtl06CompactTable (Atl06Dispatch::atl06_compact_t* rec)
{
    return true;
}

/*----------------------------------------------------------------------------
 * buildAtl03ExtentTable
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::buildAtl03ExtentTable (Atl03Reader::extent_t* rec)
{
    return true;
}

/*----------------------------------------------------------------------------
 * defineTableSchema
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::defineTableSchema (const char* rectype)
{
    std::vector<std::shared_ptr<arrow::Field>> schema_vector;

    /* Loop through fields */
    char** field_names = NULL;
    RecordObject::field_t** fields = NULL;
    int num_fields = RecordObject::getRecordFields(rectype, &fieldnames, &fields);
    for(int i = 0; i < num_fields; i++)
    {
        switch(fields[i]->type)
        {
            case INT8:      schema_vector.push_back(arrow::field(field_names[i], arrow::int8()));       break;
            case INT16:     schema_vector.push_back(arrow::field(field_names[i], arrow::int16()));      break;
            case INT32:     schema_vector.push_back(arrow::field(field_names[i], arrow::int32()));      break;
            case INT64:     schema_vector.push_back(arrow::field(field_names[i], arrow::int64()));      break;
            case UINT8:     schema_vector.push_back(arrow::field(field_names[i], arrow::uint8()));      break;
            case UINT16:    schema_vector.push_back(arrow::field(field_names[i], arrow::uint16()));     break;
            case UINT32:    schema_vector.push_back(arrow::field(field_names[i], arrow::uint32()));     break;
            case UINT64:    schema_vector.push_back(arrow::field(field_names[i], arrow::uint64()));     break;
            case FLOAT:     schema_vector.push_back(arrow::field(field_names[i], arrow::float32()));    break;
            case DOUBLE:    schema_vector.push_back(arrow::field(field_names[i], arrow::float64()));    break;
            case TIME8:     schema_vector.push_back(arrow::field(field_names[i], arrow::date64()));     break;
            case STRING:    schema_vector.push_back(arrow::field(field_names[i], arrow::utf8()));       break;
            default:        break;
        }
    }

    /* Create Schema */
    arrow::Schema* schema = new arrow::Schema(schema_vector);

    /* Clean Up */
    if(fields) delete [] fields;
    if(fieldnames) delete [] fieldnames;

    /* Return Status */
    return num_fields > 0;
}

/*----------------------------------------------------------------------------
 * defineAtl06CompactTable
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::defineAtl06CompactTable (void)
{

}

/*----------------------------------------------------------------------------
 * defineAtl03ExtentTable
 *----------------------------------------------------------------------------*/
bool ArrowBuilder::defineAtl03ExtentTable (void)
{

}
