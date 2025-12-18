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

#include <regex>
#include <fstream>
#include <sstream>
#include <cmath>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "OsApi.h"
#include "GeoDataFrame.h"
#include "RequestFields.h"
#include "MsgQ.h"
#include "Table.h"
#include "EventLib.h"
#include "SystemConfig.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoDataFrame::OBJECT_TYPE = "GeoDataFrame";
const char* GeoDataFrame::GDF = "gdf";
const char* GeoDataFrame::META = "meta";
const char* GeoDataFrame::TERMINATE = "terminate";
const char* GeoDataFrame::SOURCE_ID = "srcid";
const char* GeoDataFrame::SOURCE_TABLE = "srctbl";

const char* GeoDataFrame::LUA_META_NAME = "GeoDataFrame";
const struct luaL_Reg GeoDataFrame::LUA_META_TABLE[] = {
    {NULL,      NULL}
};

const char* GeoDataFrame::FrameColumn::OBJECT_TYPE = "FrameColumn";
const char* GeoDataFrame::FrameColumn::LUA_META_NAME = "FrameColumn";
const struct luaL_Reg GeoDataFrame::FrameColumn::LUA_META_TABLE[] = {
    {"__index", luaGetData},
    {NULL,      NULL}
};

const char* GeoDataFrame::FrameRunner::OBJECT_TYPE = "FrameRunner";

const char* GeoDataFrame::gdfRecType = "geodataframe";
const RecordObject::fieldDef_t GeoDataFrame::gdfRecDef[] = {
    {"type",        RecordObject::UINT32,   offsetof(gdf_rec_t, type),       1,              NULL, NATIVE_FLAGS},
    {"size",        RecordObject::UINT32,   offsetof(gdf_rec_t, size),       1,              NULL, NATIVE_FLAGS},
    {"encoding",    RecordObject::UINT32,   offsetof(gdf_rec_t, encoding),   1,              NULL, NATIVE_FLAGS},
    {"num_rows",    RecordObject::UINT32,   offsetof(gdf_rec_t, num_rows),   1,              NULL, NATIVE_FLAGS},
    {"name",        RecordObject::STRING,   offsetof(gdf_rec_t, name),       MAX_NAME_SIZE,  NULL, NATIVE_FLAGS},
    {"data",        RecordObject::UINT8,    offsetof(gdf_rec_t, data),       0,              NULL, NATIVE_FLAGS},
};

const char* GeoDataFrame::FrameSender::LUA_META_NAME = "FrameSender";
const struct luaL_Reg GeoDataFrame::FrameSender::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

const char* GeoDataFrame::CRS_KEY = "crs";

/******************************************************************************
 * STATIC FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * _addColumn - internal helper
 *----------------------------------------------------------------------------*/
template<class T>
static void _addColumn(GeoDataFrame* dataframe, GeoDataFrame::gdf_rec_t* gdf_rec_data)
{
    // get column from dataframe
    FieldColumn<T>* column = dynamic_cast<FieldColumn<T>*>(dataframe->getColumn(gdf_rec_data->name, true));

    // create new column if not found
    if(!column)
    {
        column = new FieldColumn<T>(gdf_rec_data->encoding, GeoDataFrame::DEFAULT_RECEIVED_COLUMN_CHUNK_SIZE);
        if(!dataframe->addColumn(gdf_rec_data->name, column, true))
        {
            delete column;
            throw RunTimeException(ERROR, RTE_FAILURE, "failed to add column <%s> to dataframe", gdf_rec_data->name);
        }
    }
    else if(column->encoding != gdf_rec_data->encoding)
    {
        throw RunTimeException(ERROR, RTE_FAILURE, "column <%s> had mismatched encoding: %X != %X", gdf_rec_data->name, column->encoding, gdf_rec_data->encoding);
    }

    // append data to column
    if(gdf_rec_data->type == GeoDataFrame::COLUMN_REC)
    {
        dataframe->numRows = column->appendBuffer(gdf_rec_data->data, gdf_rec_data->size);
    }
    else if(gdf_rec_data->type == GeoDataFrame::META_REC)
    {
        if(gdf_rec_data->encoding & GeoDataFrame::META_COLUMN)
        {
            T* value_ptr = reinterpret_cast<T*>(gdf_rec_data->data);
            dataframe->numRows = column->appendValue(*value_ptr, gdf_rec_data->num_rows);
        }
    }
    else
    {
        throw RunTimeException(ERROR, RTE_FAILURE, "failed to add column <%u> with invalid type", gdf_rec_data->type);
    }
}

/*----------------------------------------------------------------------------
 * _addSourceColumn - internal helper
 *----------------------------------------------------------------------------*/
static void _addSourceColumn(GeoDataFrame* dataframe, GeoDataFrame::gdf_rec_t* gdf_rec_data, int32_t source_id)
{
    // get source id column from dataframe
    FieldColumn<int32_t>* column = dynamic_cast<FieldColumn<int32_t>*>(dataframe->getColumn(GeoDataFrame::SOURCE_ID, true));

    // create new source id column if not found
    if(!column)
    {
        const uint32_t encoding_mask = 0;
        column = new FieldColumn<int32_t>(encoding_mask, GeoDataFrame::DEFAULT_RECEIVED_COLUMN_CHUNK_SIZE);
        if(!dataframe->addColumn(GeoDataFrame::SOURCE_ID, column, true))
        {
            delete column;
            throw RunTimeException(ERROR, RTE_FAILURE, "failed to add column <%s> to dataframe", GeoDataFrame::SOURCE_ID);
        }
    }

    // append source_id to column
    dataframe->numRows = column->appendValue(source_id, gdf_rec_data->num_rows);

    // get source table from metadata
    FieldDictionary* dict = dynamic_cast<FieldDictionary*>(dataframe->getMetaData(GeoDataFrame::SOURCE_TABLE, Field::DICTIONARY, true));

    // create new source table if not found
    if(!dict)
    {
        dict = new FieldDictionary();
        if(!dataframe->addMetaData(GeoDataFrame::SOURCE_TABLE, dict, true))
        {
            delete dict;
            throw RunTimeException(ERROR, RTE_FAILURE, "failed to add metadata <%s> to dataframe", GeoDataFrame::SOURCE_TABLE);
        }
    }

    // add source_id to meta data
    const string value(reinterpret_cast<const char*>(gdf_rec_data->data), gdf_rec_data->size);
    FieldElement<string>* source_id_field = new FieldElement<string>(value);
    if(!dict->add(FString("%d", source_id).c_str(), source_id_field, true))
    {
        delete source_id_field;
        throw RunTimeException(ERROR, RTE_FAILURE, "failed to add <%s=%d> to <%s>", GeoDataFrame::SOURCE_ID, source_id, GeoDataFrame::SOURCE_TABLE);
    }
}

/*----------------------------------------------------------------------------
 * _addListColumn - internal helper
 *----------------------------------------------------------------------------*/
template<class T>
static void _addListColumn(GeoDataFrame* dataframe, GeoDataFrame::gdf_rec_t* gdf_rec_data)
{
    // get column from dataframe
    FieldColumn<FieldList<T>>* column = dynamic_cast<FieldColumn<FieldList<T>>*>(dataframe->getColumn(gdf_rec_data->name, true));

    // create new column if not found
    if(!column)
    {
        column = new FieldColumn<FieldList<T>>(gdf_rec_data->encoding & ~Field::NESTED_MASK, GeoDataFrame::DEFAULT_RECEIVED_COLUMN_CHUNK_SIZE);
        if(!dataframe->addColumn(gdf_rec_data->name, column, true))
        {
            delete column;
            throw RunTimeException(ERROR, RTE_FAILURE, "failed to add list column <%s> to dataframe", gdf_rec_data->name);
        }
    }
    else if((column->encoding & ~Field::NESTED_MASK) != (gdf_rec_data->encoding & ~Field::NESTED_MASK))
    {
        throw RunTimeException(ERROR, RTE_FAILURE, "column <%s> had mismatched encoding: %X != %X", gdf_rec_data->name, column->encoding, gdf_rec_data->encoding);
    }

    // append data to column
    if(gdf_rec_data->type == GeoDataFrame::COLUMN_REC)
    {
        uint32_t* sizes_ptr = reinterpret_cast<uint32_t*>(gdf_rec_data->data);
        const long size_of_sizes = sizeof(uint32_t) * gdf_rec_data->num_rows;
        long data_offset = size_of_sizes;
        for(uint32_t j = 0; j < gdf_rec_data->num_rows; j++)
        {
            FieldList<T> field_list;
            field_list.appendBuffer(&gdf_rec_data->data[data_offset], sizes_ptr[j]);
            data_offset += sizes_ptr[j];
            dataframe->numRows = column->append(field_list);
        }
    }
    else
    {
        throw RunTimeException(ERROR, RTE_FAILURE, "failed to add list column <%u> with invalid type", gdf_rec_data->type);
    }
}

/*----------------------------------------------------------------------------
 * _appendListValues - internal helper
 *----------------------------------------------------------------------------*/
template<typename T>
static long _appendListValues(GeoDataFrame* gdf, const char* name, const void* values, long count, bool nodata)
{
    FieldColumn<FieldList<T>>* column = dynamic_cast<FieldColumn<FieldList<T>>*>(gdf->getColumn(name, true));
    FieldList<T> list;
    if(nodata)
    {
        for(long i = 0; i < count; i++) list.append(static_cast<T>(0));
    }
    else
    {
        const T* typed = reinterpret_cast<const T*>(values);
        for(long i = 0; i < count; i++) list.append(typed[i]);
    }
    return column->append(list);
}

/*----------------------------------------------------------------------------
 * _appendColumnBuffer - internal helper
 *----------------------------------------------------------------------------*/
template<typename T>
static long _appendColumnBuffer(Field* field, const uint8_t* data, int size, bool nodata)
{
    FieldColumn<T>* column = dynamic_cast<FieldColumn<T>*>(field);
    if(nodata)
    {
        const long count = size / sizeof(T);
        const T zero_value{};
        return column->appendValue(zero_value, count);
    }
    else
    {
        return column->appendBuffer(data, size);
    }
}


/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * FrameColumn: Constructor
 *----------------------------------------------------------------------------*/
GeoDataFrame::FrameColumn::FrameColumn(lua_State* L, const Field& _column):
    LuaObject (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    column(_column)
{
}

/*----------------------------------------------------------------------------
 * FrameColumn: luaGetData - [<index>]
 *----------------------------------------------------------------------------*/
int GeoDataFrame::FrameColumn::luaGetData (lua_State* L)
{
    try
    {
        GeoDataFrame::FrameColumn* lua_obj = dynamic_cast<GeoDataFrame::FrameColumn*>(getLuaSelf(L, 1));
        const long index = getLuaInteger(L, 2) - 1; // lua indexing starts at 1, convert to c indexing that starts at 0

        // check index
        if(index < 0) throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid index: %ld", index + 1);

        // check the metatable for the key (to support functions)
        luaL_getmetatable(L, lua_obj->LuaMetaName);
        lua_pushinteger(L, index);
        lua_rawget(L, -2);
        if(!lua_isnil(L, -1)) return 1;
        lua_pop(L, 1);

        // handle field access
        return lua_obj->column.toLua(L, index);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error indexing frame column %s: %s", OBJECT_TYPE, e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * FrameRunner: Constructor -
 *----------------------------------------------------------------------------*/
GeoDataFrame::FrameRunner::FrameRunner(lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table),
    runtime(0.0)
{
    LuaEngine::setAttrFunc(L, "runtime", luaGetRunTime);
}

/*----------------------------------------------------------------------------
 * FrameRunner: luaGetRunTime -
 *----------------------------------------------------------------------------*/
int GeoDataFrame::FrameRunner::luaGetRunTime (lua_State* L)
{
    try
    {
        FrameRunner* lua_obj = dynamic_cast<FrameRunner*>(getLuaSelf(L, 1));
        lua_pushnumber(L, lua_obj->runtime);
    }
    catch(const RunTimeException& e)
    {
        lua_pushnumber(L, 0.0);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * FrameRunner: updateRunTime -
 *----------------------------------------------------------------------------*/
void GeoDataFrame::FrameRunner::updateRunTime(double duration)
{
    m.lock();
    {
        runtime += duration;
    }
    m.unlock();
}

/*----------------------------------------------------------------------------
 * FrameSender: luaCreate -
 *----------------------------------------------------------------------------*/
int GeoDataFrame::FrameSender::luaCreate(lua_State* L)
{
    try
    {
        const char*     rspq        = getLuaString(L, 1);
        const uint64_t  key_space   = getLuaInteger(L, 2, true, RequestFields::DEFAULT_KEY_SPACE);
        const int       timeout     = getLuaInteger(L, 3, true, SYS_TIMEOUT);

        return createLuaObject(L, new FrameSender(L, rspq, key_space, timeout));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
    }

    return returnLuaStatus(L, false);
}

/*----------------------------------------------------------------------------
 * FrameSender: Constructor -
 *----------------------------------------------------------------------------*/
GeoDataFrame::FrameSender::FrameSender(lua_State* L, const char* _rspq, uint64_t _key_space, int _timeout):
    FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    rspq(StringLib::duplicate(_rspq)),
    key_space(_key_space),
    timeout(_timeout)
{
}

/*----------------------------------------------------------------------------
 * FrameSender: Destructor -
 *----------------------------------------------------------------------------*/
GeoDataFrame::FrameSender::~FrameSender(void)
{
    delete [] rspq;
}

/*----------------------------------------------------------------------------
 * FrameSender: run -
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::FrameSender::run(GeoDataFrame* dataframe)
{
    /* Latch Start Time */
    const double start = TimeLib::latchtime();
    const uint64_t key = (dataframe->getKey() << 32) | key_space;

    try
    {
        /* Send DataFrame */
        dataframe->sendDataframe(rspq, key, timeout);
    }
    catch (const RunTimeException& e)
    {
        Publisher pubq(rspq);
        alert(ERROR, RTE_FAILURE, &pubq, &dataframe->active, "request <%s> failed to send dataframe: %s", rspq, e.what());
    }

    /* Update Run Time */
    updateRunTime(TimeLib::latchtime() - start);

    /* Success */
    return true;
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void GeoDataFrame::init(void)
{
    RECDEF(gdfRecType, gdfRecDef, sizeof(gdf_rec_t), NULL);
}

/*----------------------------------------------------------------------------
 * luaCreate - dataframe([<column table>], [<meta table>]), [<crs>]
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaCreate (lua_State* L)
{
    bool status = true;
    GeoDataFrame* dataframe = NULL;

    try
    {
        // parameter indices
        const int column_table_index = 1;
        const int meta_table_index   = 2;
        const int crs_index          = 3;
        const int nargs = lua_gettop(L);

        // optional CRS
        const char* crs = NULL;
        if(nargs >= crs_index && lua_isstring(L, crs_index))
        {
            crs = lua_tostring(L, crs_index);
        }

        // create initial dataframe
        dataframe = new GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE, {}, {}, crs);

        // column table
        if(lua_istable(L, column_table_index))
        {
            // build columns of dataframe
            lua_pushnil(L); // prime the stack for the next key
            while(lua_next(L, column_table_index) != 0)
            {
                if(lua_isstring(L, -2))
                {
                    // create column
                    const char* name = lua_tostring(L, -2);
                    FieldColumn<double>* column = new FieldColumn<double>();

                    // add values to column
                    const int num_elements = lua_rawlen(L, -1);
                    for(int i = 0; i < num_elements; i++)
                    {
                        double value;
                        lua_rawgeti(L, -1, i + 1);
                        convertFromLua(L, -1, value);
                        lua_pop(L, 1);
                        column->append(value);
                    }

                    // add column to dataframe
                    dataframe->columnFields.add(name, column, true);
                    mlog(DEBUG, "Adding column %s of length %ld", name, column->length());
                }
                lua_pop(L, 1); // remove the key
            }

            // check dataframe columns and set number of rows
            const vector<string>& column_names = dataframe->getColumnNames();
            for(const string& name: column_names)
            {
                const Field* field = dataframe->getColumn(name.c_str());
                if(dataframe->numRows == 0)
                {
                    dataframe->numRows = field->length();
                }
                else if(dataframe->numRows != field->length())
                {
                    throw RunTimeException(CRITICAL, RTE_FAILURE, "number of rows must match for all columns, %ld != %ld", dataframe->numRows, field->length());
                }
            }
        }

        //  meta table
        if(lua_istable(L, meta_table_index))
        {
            // build keys of metadata
            lua_pushnil(L); // prime the stack for the next key
            while(lua_next(L, meta_table_index) != 0)
            {
                if(lua_isstring(L, -2))
                {
                    const char* key = lua_tostring(L, -2);

                    if(lua_isnumber(L, -1)) // treat as a double
                    {
                        FieldElement<double>* element = new FieldElement<double>();
                        element->setEncodingFlags(META_COLUMN);
                        dataframe->metaFields.add(key, element, true);
                    }
                    else if(lua_isstring(L, -1))
                    {
                        FieldElement<string>* element = new FieldElement<string>();
                        element->setEncodingFlags(META_COLUMN);
                        dataframe->metaFields.add(key, element, true);
                    }
                    mlog(DEBUG, "Adding metadata %s", key);
                }
                lua_pop(L, 1); // remove the key
            }

            // import metadata elements from lua
            dataframe->metaFields.fromLua(L, meta_table_index);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error importing %s: %s", OBJECT_TYPE, e.what());
        status = false;
    }

    // return dataframe LuaObject
    if(!status)
    {
        delete dataframe;
        return returnLuaStatus(L, false);
    }
    return createLuaObject(L, dataframe);
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
void GeoDataFrame::clear(void)
{
    columnFields.clear();
    numRows = 0;
}

/*----------------------------------------------------------------------------
 * length
 *----------------------------------------------------------------------------*/
long GeoDataFrame::length(void) const
{
    return numRows;
}

/*----------------------------------------------------------------------------
 * addRow
 *----------------------------------------------------------------------------*/
long GeoDataFrame::addRow(void)
{
    numRows++;
    return numRows;
}

/*----------------------------------------------------------------------------
 * appendFromBuffer
 *----------------------------------------------------------------------------*/
long GeoDataFrame::appendFromBuffer(const char* name, const uint8_t* buffer, int size, uint32_t column_encoding, bool nodata)
{
    const uint32_t nested_encoding = column_encoding & Field::NESTED_MASK;
    long elements = 0;

    if(nested_encoding == Field::NESTED_LIST || nested_encoding == Field::NESTED_ARRAY)
    {
        const uint32_t value_encoding = column_encoding & Field::TYPE_MASK;
        switch(value_encoding)
        {
            case RecordObject::INT8:    elements = _appendListValues<int8_t>  (this, name, buffer, size, nodata); break;
            case RecordObject::INT16:   elements = _appendListValues<int16_t> (this, name, buffer, size, nodata); break;
            case RecordObject::INT32:   elements = _appendListValues<int32_t> (this, name, buffer, size, nodata); break;
            case RecordObject::INT64:   elements = _appendListValues<int64_t> (this, name, buffer, size, nodata); break;
            case RecordObject::UINT8:   elements = _appendListValues<uint8_t> (this, name, buffer, size, nodata); break;
            case RecordObject::UINT16:  elements = _appendListValues<uint16_t>(this, name, buffer, size, nodata); break;
            case RecordObject::UINT32:  elements = _appendListValues<uint32_t>(this, name, buffer, size, nodata); break;
            case RecordObject::UINT64:  elements = _appendListValues<uint64_t>(this, name, buffer, size, nodata); break;
            case RecordObject::FLOAT:   elements = _appendListValues<float>   (this, name, buffer, size, nodata); break;
            case RecordObject::DOUBLE:  elements = _appendListValues<double>  (this, name, buffer, size, nodata); break;
            case RecordObject::TIME8:   elements = _appendListValues<time8_t> (this, name, buffer, size, nodata); break;
            default:
            {
                mlog(ERROR, "Cannot append to list column <%s> value of type %d", name, static_cast<int>(value_encoding));
            }
        }
    }
    else
    {
        Field* field = getColumn(name);
        switch(field->getValueEncoding())
        {
            case Field::BOOL:   elements = _appendColumnBuffer<bool>    (field, buffer, size, nodata); break;
            case Field::INT8:   elements = _appendColumnBuffer<int8_t>  (field, buffer, size, nodata); break;
            case Field::INT16:  elements = _appendColumnBuffer<int16_t> (field, buffer, size, nodata); break;
            case Field::INT32:  elements = _appendColumnBuffer<int32_t> (field, buffer, size, nodata); break;
            case Field::INT64:  elements = _appendColumnBuffer<int64_t> (field, buffer, size, nodata); break;
            case Field::UINT8:  elements = _appendColumnBuffer<uint8_t> (field, buffer, size, nodata); break;
            case Field::UINT16: elements = _appendColumnBuffer<uint16_t>(field, buffer, size, nodata); break;
            case Field::UINT32: elements = _appendColumnBuffer<uint32_t>(field, buffer, size, nodata); break;
            case Field::UINT64: elements = _appendColumnBuffer<uint64_t>(field, buffer, size, nodata); break;
            case Field::FLOAT:  elements = _appendColumnBuffer<float>   (field, buffer, size, nodata); break;
            case Field::DOUBLE: elements = _appendColumnBuffer<double>  (field, buffer, size, nodata); break;
            case Field::STRING: elements = _appendColumnBuffer<string>  (field, buffer, size, nodata); break;
            case Field::TIME8:  elements = _appendColumnBuffer<time8_t> (field, buffer, size, nodata); break;
            default:
            {
                mlog(ERROR, "Cannot add column <%s> of type %d", name, field->getValueEncoding());
            }
        }
    }
    return elements;
}

/*----------------------------------------------------------------------------
 * getColumnNames
 *----------------------------------------------------------------------------*/
vector<string> GeoDataFrame::getColumnNames(void) const
{
    vector<string> names;
    char** keys = NULL;
    const int num_keys = columnFields.fields.getKeys(&keys);
    for(int i = 0; i < num_keys; i++)
    {
        names.emplace_back(keys[i]);
        delete [] keys[i];
    }
    delete [] keys;
    return names;
}

/*----------------------------------------------------------------------------
 * addColumn - assumes memory is properly allocated already
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::addColumn (const char* name, FieldUntypedColumn* column, bool free_on_delete)
{
    return columnFields.add(name, column, free_on_delete);
}

/*----------------------------------------------------------------------------
 * addNewColumn - allocates name and field
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::addNewColumn (const char* name, uint32_t column_encoding)
{
    FieldUntypedColumn* column = NULL;
    const uint32_t nested_encoding = column_encoding & Field::NESTED_MASK;
    const uint32_t value_encoding  = column_encoding & Field::TYPE_MASK;
    const uint32_t encoding_mask   = column_encoding & ~Field::VALUE_MASK; /* column flags without value bits */

    /* NESTED_LIST and NESTED_ARRAY both go through the list path */
    if(nested_encoding == Field::NESTED_LIST || nested_encoding == Field::NESTED_ARRAY)
    {
        switch(value_encoding)
        {
            case RecordObject::INT8:    column = new FieldColumn<FieldList<int8_t>>  (encoding_mask); break;
            case RecordObject::INT16:   column = new FieldColumn<FieldList<int16_t>> (encoding_mask); break;
            case RecordObject::INT32:   column = new FieldColumn<FieldList<int32_t>> (encoding_mask); break;
            case RecordObject::INT64:   column = new FieldColumn<FieldList<int64_t>> (encoding_mask); break;
            case RecordObject::UINT8:   column = new FieldColumn<FieldList<uint8_t>> (encoding_mask); break;
            case RecordObject::UINT16:  column = new FieldColumn<FieldList<uint16_t>>(encoding_mask); break;
            case RecordObject::UINT32:  column = new FieldColumn<FieldList<uint32_t>>(encoding_mask); break;
            case RecordObject::UINT64:  column = new FieldColumn<FieldList<uint64_t>>(encoding_mask); break;
            case RecordObject::FLOAT:   column = new FieldColumn<FieldList<float>>   (encoding_mask); break;
            case RecordObject::DOUBLE:  column = new FieldColumn<FieldList<double>>  (encoding_mask); break;
            case RecordObject::TIME8:   column = new FieldColumn<FieldList<time8_t>> (encoding_mask); break;
            default:
            {
                mlog(ERROR, "Cannot add nested column <%s> of type %d", name, value_encoding);
                return false;
            }
        }
    }
    else
    {
        switch(value_encoding)
        {
            case Field::BOOL:   column = new FieldColumn<bool>   (encoding_mask); break;
            case Field::INT8:   column = new FieldColumn<int8_t> (encoding_mask); break;
            case Field::INT16:  column = new FieldColumn<int16_t>(encoding_mask); break;
            case Field::INT32:  column = new FieldColumn<int32_t>(encoding_mask); break;
            case Field::INT64:  column = new FieldColumn<int64_t>(encoding_mask); break;
            case Field::UINT8:  column = new FieldColumn<uint8_t>(encoding_mask); break;
            case Field::UINT16: column = new FieldColumn<uint16_t>(encoding_mask); break;
            case Field::UINT32: column = new FieldColumn<uint32_t>(encoding_mask); break;
            case Field::UINT64: column = new FieldColumn<uint64_t>(encoding_mask); break;
            case Field::FLOAT:  column = new FieldColumn<float>  (encoding_mask); break;
            case Field::DOUBLE: column = new FieldColumn<double> (encoding_mask); break;
            case Field::STRING: column = new FieldColumn<string> (encoding_mask); break;
            case Field::TIME8:  column = new FieldColumn<time8_t>(encoding_mask); break;
            default:
            {
                mlog(ERROR, "Cannot add column <%s> of type %d", name, value_encoding);
                return false;
            }
        }
    }

    if(!addColumn(name, column, true))
    {
        mlog(ERROR, "Failed to add column <%s>", name);
        delete column;
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * addExistingColumn - only allocates name
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::addExistingColumn (const char* name, FieldUntypedColumn* column)
{
    if(addColumn(name, column, true))
    {
        // set number of rows if unset
        if(numRows == 0)
        {
            numRows = column->length();
        }
    }
    else
    {
        // log error and clean up
        mlog(ERROR, "Failed to add column <%s>", name);
        delete column;
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * getColumn
 *----------------------------------------------------------------------------*/
FieldUntypedColumn* GeoDataFrame::getColumn (const char* name, bool no_throw) const
{
    if(!no_throw)
    {
        const FieldMap<FieldUntypedColumn>::entry_t entry = columnFields.fields[name];
        if(!entry.field) throw RunTimeException(CRITICAL, RTE_FAILURE, "%s field is null", name);
        return entry.field;
    }
    else
    {
        FieldMap<FieldUntypedColumn>::entry_t entry;
        if(columnFields.fields.find(name, &entry))
        {
            return entry.field;
        }
        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * addMetaData
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::addMetaData (const char* name, Field* meta, bool free_on_delete)
{
    return metaFields.add(name, meta, free_on_delete);
}

/*----------------------------------------------------------------------------
 * getMetaData
 *----------------------------------------------------------------------------*/
Field* GeoDataFrame::getMetaData (const char* name, Field::type_t _type, bool no_throw) const
{
    if(!no_throw)
    {
        const meta_entry_t entry = metaFields.fields[name];
        if(!entry.field) throw RunTimeException(CRITICAL, RTE_FAILURE, "%s field is null", name);
        if(_type != Field::FIELD && _type != entry.field->type) throw RunTimeException(CRITICAL, RTE_FAILURE, "%s is incorrect type: %d", name, static_cast<int>(entry.field->type));
        return entry.field;
    }
    else
    {
        meta_entry_t entry;
        if(metaFields.fields.find(name, &entry))
        {
            if(_type == Field::FIELD || _type == entry.field->type)
            {
                return entry.field;
            }
        }
        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * deleteColumn
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::deleteColumn (const char* name)
{
    if(name)
    {
        return columnFields.fields.remove(name);
    }
    return false;
}

/*----------------------------------------------------------------------------
 * populateGeoColumn
 *----------------------------------------------------------------------------*/
void GeoDataFrame::populateDataframe (void)
{
    // populate geo columns
    Dictionary<FieldMap<FieldUntypedColumn>::entry_t>::Iterator iter(columnFields.fields);
    for(int f = 0; f < iter.length; f++)
    {
        const char* name = iter[f].key;
        const Field* field = iter[f].value.field;

        if(field->encoding & TIME_COLUMN)
        {
            assert(field->type == COLUMN);
            assert(field->getValueEncoding() == TIME8);
            timeColumn = dynamic_cast<const FieldColumn<time8_t>*>(field);
            timeColumnName = name;
        }

        if(field->encoding & X_COLUMN)
        {
            assert(field->type == COLUMN);
            assert(field->getValueEncoding() == DOUBLE);
            xColumn = dynamic_cast<const FieldColumn<double>*>(field);
            xColumnName = name;
        }

        if(field->encoding & Y_COLUMN)
        {
            assert(field->type == COLUMN);
            assert(field->getValueEncoding() == DOUBLE);
            yColumn = dynamic_cast<const FieldColumn<double>*>(field);
            yColumnName = name;
        }

        if(field->encoding & Z_COLUMN)
        {
            assert(field->type == COLUMN);
            assert(field->getValueEncoding() == FLOAT);
            zColumn = dynamic_cast<const FieldColumn<float>*>(field);
            zColumnName = name;
        }
    }
}

/*----------------------------------------------------------------------------
 * operator[]
 *----------------------------------------------------------------------------*/
const FieldUntypedColumn& GeoDataFrame::operator[](const char* key) const
{
    return *getColumn(key, true);
}

/*----------------------------------------------------------------------------
 * getKey
 *----------------------------------------------------------------------------*/
okey_t GeoDataFrame::getKey (void) const
{
    return 0;
}

/*----------------------------------------------------------------------------
 * getTimeColumn
 *----------------------------------------------------------------------------*/
const FieldColumn<time8_t>* GeoDataFrame::getTimeColumn (void) const
{
    return timeColumn;
}

/*----------------------------------------------------------------------------
 * getXColumn
 *----------------------------------------------------------------------------*/
const FieldColumn<double>* GeoDataFrame::getXColumn (void) const
{
    return xColumn;
}

/*----------------------------------------------------------------------------
 * getYColumn
 *----------------------------------------------------------------------------*/
const FieldColumn<double>* GeoDataFrame::getYColumn (void) const
{
    return yColumn;
}

/*----------------------------------------------------------------------------
 * getZColumn
 *----------------------------------------------------------------------------*/
const FieldColumn<float>* GeoDataFrame::getZColumn (void) const
{
    return zColumn;
}

/*----------------------------------------------------------------------------
 * getTimeColumnName
 *----------------------------------------------------------------------------*/
const string& GeoDataFrame::getTimeColumnName (void) const
{
    return timeColumnName;
}

/*----------------------------------------------------------------------------
 * getXColumnName
 *----------------------------------------------------------------------------*/
const string& GeoDataFrame::getXColumnName (void) const
{
    return xColumnName;
}

/*----------------------------------------------------------------------------
 * getYColumnName
 *----------------------------------------------------------------------------*/
const string& GeoDataFrame::getYColumnName (void) const
{
    return yColumnName;
}

/*----------------------------------------------------------------------------
 * getZColumnName
 *----------------------------------------------------------------------------*/
const string& GeoDataFrame::getZColumnName (void) const
{
    return zColumnName;
}

/*----------------------------------------------------------------------------
 * getInfoAsJson
 *----------------------------------------------------------------------------*/
string GeoDataFrame::getInfoAsJson (void) const
{
    return string(FString("{\"time\":\"%s\",\"x\":\"%s\",\"y\":\"%s\",\"z\":\"%s\"}",
                    timeColumnName.c_str(),
                    xColumnName.c_str(),
                    yColumnName.c_str(),
                    zColumnName.c_str()).c_str());
}

/*----------------------------------------------------------------------------
 * waitRunComplete
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::waitRunComplete (int timeout)
{
    bool status = false;
    runSignal.lock();
    {
        if(!runComplete)
        {
            runSignal.wait(SIGNAL_COMPLETE, timeout);
        }
        status = runComplete;
    }
    runSignal.unlock();
    return status;
}

/*----------------------------------------------------------------------------
 * signalRunComplete
 *----------------------------------------------------------------------------*/
void GeoDataFrame::signalRunComplete (void)
{
    runSignal.lock();
    {
        if(!runComplete)
        {
            runSignal.signal(SIGNAL_COMPLETE);
        }
        runComplete = true;
    }
    runSignal.unlock();
}

/*----------------------------------------------------------------------------
 * getColumns
 *----------------------------------------------------------------------------*/
const Dictionary<GeoDataFrame::column_entry_t>& GeoDataFrame::getColumns(void) const
{
    return columnFields.fields;
}

/*----------------------------------------------------------------------------
 * getMeta
 *----------------------------------------------------------------------------*/
const Dictionary<GeoDataFrame::meta_entry_t>& GeoDataFrame::getMeta(void) const
{
    return metaFields.fields;
}

/*----------------------------------------------------------------------------
 * loadCRSFile
 *----------------------------------------------------------------------------*/
string GeoDataFrame::loadCRSFile(const char* crsFile)
{
    // build the full path to the crs file
    std::stringstream crsPath;
    crsPath << CONFDIR << PATH_DELIMETER << crsFile;

    // read the contents of the file
    const std::ifstream f(crsPath.str());
    assert(f.is_open());
    std::ostringstream contents;
    contents << f.rdbuf();

    // serialize into a compact JSON string
    rapidjson::Document doc;
    doc.Parse(contents.str().c_str());
    assert(!doc.HasParseError());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    // return compact crs
    mlog(INFO, "Loaded CRS file: %s", crsFile);
    return buffer.GetString();
}

/*----------------------------------------------------------------------------
 * extractColumnName
 *----------------------------------------------------------------------------*/
string GeoDataFrame::extractColumnName (const string& column_description)
{
    const std::regex pattern(R"((\w+)\((\w+)\))");
    std::smatch matches;
    if(std::regex_match(column_description, matches, pattern))
    {
        return matches[2];
    }
    return column_description;
}

/*----------------------------------------------------------------------------
 * extractColumnOperation
 *----------------------------------------------------------------------------*/
GeoDataFrame::column_op_t GeoDataFrame::extractColumnOperation (const string& column_description)
{
    const std::regex pattern(R"((\w+)\((\w+)\))");
    std::smatch matches;
    if(std::regex_match(column_description, matches, pattern))
    {
        const string function = matches[1];
//      const string field = matches[2];
        if(function == "mean") return OP_MEAN;
        if(function == "median") return OP_MEDIAN;
        if(function == "mode") return OP_MODE;
        if(function == "sum") return OP_SUM;
    }
    return OP_NONE;
}

/*----------------------------------------------------------------------------
 * createAncillaryColumns
 *----------------------------------------------------------------------------*/
void GeoDataFrame::createAncillaryColumns (Dictionary<ancillary_t>** ancillary_columns, const FieldList<string>& ancillary_fields)
{
    // allocate field dictionary of ancillary columns
    // if needed and not already created
    if((*ancillary_columns == NULL) && (ancillary_fields.length() > 0))
    {
        *ancillary_columns = new Dictionary<ancillary_t>;
    }

    // add columns to field dictionary
    for(long i = 0; i < ancillary_fields.length(); i++)
    {
        const ancillary_t ancillary = {
            .column = new FieldColumn<double>(),
            .op = GeoDataFrame::extractColumnOperation(ancillary_fields[i])
        };
        const string name = GeoDataFrame::extractColumnName(ancillary_fields[i]);
        const bool status = (*ancillary_columns)->add(name.c_str(), ancillary); // NOLINT(clang-analyzer-core.CallAndMessage)
        if(!status)
        {
            delete ancillary.column;
            mlog(CRITICAL, "failed to add column <%s> to ancillary columns", ancillary_fields[i].c_str());
        }
    }
}

/*----------------------------------------------------------------------------
 * populateAncillaryColumns
 *----------------------------------------------------------------------------*/
void GeoDataFrame::populateAncillaryColumns(Dictionary<ancillary_t>* ancillary_columns, const GeoDataFrame& df, int32_t start_index, int32_t num_elements)
{
    if(ancillary_columns)
    {
        ancillary_t entry;
        const char* name = ancillary_columns->first(&entry);
        while(name)
        {
            double value;
            if(df[name].encoding & Field::NESTED_MASK)
            {
                /* Multidimensional ancillary field: no scalar aggregation */
                value = NAN;
            }
            else
            {
                switch(entry.op)
                {
                    case GeoDataFrame::OP_NONE:     value = df[name].mean(start_index, num_elements);   break;
                    case GeoDataFrame::OP_MEAN:     value = df[name].mean(start_index, num_elements);   break;
                    case GeoDataFrame::OP_MEDIAN:   value = df[name].median(start_index, num_elements); break;
                    case GeoDataFrame::OP_MODE:     value = df[name].mode(start_index, num_elements);   break;
                    case GeoDataFrame::OP_SUM:      value = df[name].sum(start_index, num_elements);    break;
                    default:                        value = 0.0;                                        break;
                }
            }
            entry.column->append(value);
            name = ancillary_columns->next(&entry);
        }
    }
}

/*----------------------------------------------------------------------------
 * addAncillaryColumns
 *----------------------------------------------------------------------------*/
void GeoDataFrame::addAncillaryColumns (Dictionary<ancillary_t>* ancillary_columns, GeoDataFrame* dataframe)
{
    if(ancillary_columns)
    {
        ancillary_t entry;
        const char* name = ancillary_columns->first(&entry);
        while(name)
        {
            dataframe->addExistingColumn(name, entry.column);
            name = ancillary_columns->next(&entry);
        }
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoDataFrame::GeoDataFrame( lua_State* L,
                            const char* meta_name,
                            const struct luaL_Reg meta_table[],
                            const std::initializer_list<FieldMap<FieldUntypedColumn>::init_entry_t>& column_list,
                            const std::initializer_list<FieldDictionary::init_entry_t>& meta_list,
                            const char* _crs):
    LuaObject (L, OBJECT_TYPE, meta_name, meta_table),
    Field(DATAFRAME, 0),
    inError(false),
    numRows(0),
    columnFields(column_list),
    metaFields(meta_list),
    timeColumn(NULL),
    xColumn(NULL),
    yColumn(NULL),
    zColumn(NULL),
    crs(_crs == NULL ? "" : _crs),
    active(true),
    receivePid(NULL),
    runPid(NULL),
    pubRunQ(NULL),
    subRunQ(pubRunQ),
    runComplete(false)
{
    // set lua functions
    LuaEngine::setAttrFunc(L, "inerror",    luaInError);
    LuaEngine::setAttrFunc(L, "numrows",    luaNumRows);
    LuaEngine::setAttrFunc(L, "numcols",    luaNumColumns);
    LuaEngine::setAttrFunc(L, "export",     luaExport);
    LuaEngine::setAttrFunc(L, "send",       luaSend);
    LuaEngine::setAttrFunc(L, "receive",    luaReceive);
    LuaEngine::setAttrFunc(L, "row",        luaGetRowData);
    LuaEngine::setAttrFunc(L, "__index",    luaGetColumnData);
    LuaEngine::setAttrFunc(L, "meta",       luaGetMetaData);
    LuaEngine::setAttrFunc(L, "crs",        luaGetCRS);
    LuaEngine::setAttrFunc(L, "run",        luaRun);
    LuaEngine::setAttrFunc(L, "finished",   luaRunComplete);

    // start runner
    runPid = new Thread(runThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoDataFrame::~GeoDataFrame(void)
{
    active.store(false);
    delete receivePid;
    delete runPid;

    // release pending frame runners
    int recv_status = MsgQ::STATE_OKAY;
    while(recv_status > 0)
    {
        GeoDataFrame::FrameRunner* runner;
        recv_status = subRunQ.receiveCopy(&runner, sizeof(runner), IO_CHECK);
        if(recv_status > 0 && runner) runner->releaseLuaObject();
    }
}

/*----------------------------------------------------------------------------
 * appendDataframe
 *----------------------------------------------------------------------------*/
void GeoDataFrame::appendDataframe(GeoDataFrame::gdf_rec_t* gdf_rec_data, int32_t source_id)
{
    const uint32_t value_encoding = gdf_rec_data->encoding & Field::VALUE_MASK;
    const uint32_t encoded_type = gdf_rec_data->encoding & Field::TYPE_MASK;

    if((gdf_rec_data->type == GeoDataFrame::META_REC) &&
       (gdf_rec_data->encoding & GeoDataFrame::META_SOURCE_ID))
    {
        _addSourceColumn(this, gdf_rec_data, source_id);
    }
    else if(value_encoding & (Field::NESTED_LIST | Field::NESTED_ARRAY | Field::NESTED_COLUMN))
    {
        switch(encoded_type)
        {
            // case BOOL:      add_status = _addListColumn<bool>    (this, gdf_rec_data); break;
            case INT8:      _addListColumn<int8_t>  (this, gdf_rec_data); break;
            case INT16:     _addListColumn<int16_t> (this, gdf_rec_data); break;
            case INT32:     _addListColumn<int32_t> (this, gdf_rec_data); break;
            case INT64:     _addListColumn<int64_t> (this, gdf_rec_data); break;
            case UINT8:     _addListColumn<uint8_t> (this, gdf_rec_data); break;
            case UINT16:    _addListColumn<uint16_t>(this, gdf_rec_data); break;
            case UINT32:    _addListColumn<uint32_t>(this, gdf_rec_data); break;
            case UINT64:    _addListColumn<uint64_t>(this, gdf_rec_data); break;
            case FLOAT:     _addListColumn<float>   (this, gdf_rec_data); break;
            case DOUBLE:    _addListColumn<double>  (this, gdf_rec_data); break;
            case TIME8:     _addListColumn<time8_t> (this, gdf_rec_data); break;
            default:        throw RunTimeException(ERROR, RTE_FAILURE, "failed to add nested column <%s> with unsupported encoding %X", gdf_rec_data->name, gdf_rec_data->encoding); break;
        }
    }
    else
    {
        switch(encoded_type)
        {
            case BOOL:      _addColumn<bool>    (this, gdf_rec_data); break;
            case INT8:      _addColumn<int8_t>  (this, gdf_rec_data); break;
            case INT16:     _addColumn<int16_t> (this, gdf_rec_data); break;
            case INT32:     _addColumn<int32_t> (this, gdf_rec_data); break;
            case INT64:     _addColumn<int64_t> (this, gdf_rec_data); break;
            case UINT8:     _addColumn<uint8_t> (this, gdf_rec_data); break;
            case UINT16:    _addColumn<uint16_t>(this, gdf_rec_data); break;
            case UINT32:    _addColumn<uint32_t>(this, gdf_rec_data); break;
            case UINT64:    _addColumn<uint64_t>(this, gdf_rec_data); break;
            case FLOAT:     _addColumn<float>   (this, gdf_rec_data); break;
            case DOUBLE:    _addColumn<double>  (this, gdf_rec_data); break;
            case TIME8:     _addColumn<time8_t> (this, gdf_rec_data); break;
            default:        throw RunTimeException(ERROR, RTE_FAILURE, "failed to add column <%s> with unsupported encoding %X", gdf_rec_data->name, gdf_rec_data->encoding); break;
        }
    }
}

/*----------------------------------------------------------------------------
 * sendDataframe
 *----------------------------------------------------------------------------*/
void GeoDataFrame::sendDataframe (const char* rspq, uint64_t key_space, int timeout) const
{
    // check if dataframe is in error
    if(inError) throw RunTimeException(ERROR, RTE_FAILURE, "invalid dataframe");

    // create publisher
    Publisher pub(rspq);

    // massage key_space
    if(key_space == INVALID_KEY) key_space = 0;

    // create and send column records
    Dictionary<column_entry_t>::Iterator column_iter(columnFields.fields);
    for(int i = 0; i < column_iter.length; i++)
    {
        const Dictionary<column_entry_t>::kv_t kv = column_iter[i];

        // get encodings
        const uint32_t value_encoding = kv.value.field->getValueEncoding();
        const uint32_t encoded_type = kv.value.field->getEncodedType();
        if(encoded_type >= RecordObject::NUM_FIELD_TYPES) throw RunTimeException(ERROR, RTE_FAILURE, "unsupported value encoding: %X", encoded_type);

        if(value_encoding == encoded_type)
        {
            // determine size of column
            const long column_size = kv.value.field->length() * RecordObject::FIELD_TYPE_BYTES[encoded_type];
            const long rec_size = offsetof(gdf_rec_t, data) + column_size;

            // create column record
            RecordObject gdf_rec(gdfRecType, rec_size);
            gdf_rec_t* gdf_rec_data = reinterpret_cast<gdf_rec_t*>(gdf_rec.getRecordData());
            gdf_rec_data->key = key_space;
            gdf_rec_data->type = COLUMN_REC;
            gdf_rec_data->size = column_size;
            gdf_rec_data->encoding = kv.value.field->encoding;
            gdf_rec_data->num_rows = kv.value.field->length();
            StringLib::copy(gdf_rec_data->name, kv.key, MAX_NAME_SIZE);

            // serialize column data into record
            const long bytes_serialized = kv.value.field->serialize(gdf_rec_data->data, column_size);
            if(bytes_serialized != column_size) throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to serialize column %s: %ld != %ld", gdf_rec_data->name, bytes_serialized, column_size);

            // send column record
            gdf_rec.post(&pub, 0, NULL, true, timeout);
        }
        else if(value_encoding & (Field::NESTED_LIST | Field::NESTED_ARRAY | Field::NESTED_COLUMN))
        {
            // for nested list columns the data[] field
            // is populated with the following structure:
            //   uint32_t   size_of_row_list_in_bytes [num_rows]
            //   T          row_list [num_rows][elements_in_list]

            // determine size of sizes (bytes)
            const long size_of_sizes = sizeof(uint32_t) * kv.value.field->length();

            // determine size of column
            long column_size = 0;
            for(long j = 0; j < kv.value.field->length(); j++)
            {
                const Field* field_list = kv.value.field->get(j);
                column_size += field_list->length();
            }
            column_size *= RecordObject::FIELD_TYPE_BYTES[encoded_type];

            // determine record size
            const long data_size = size_of_sizes + column_size;
            const long rec_size = offsetof(gdf_rec_t, data) + data_size;

            // create column record
            RecordObject gdf_rec(gdfRecType, rec_size);
            gdf_rec_t* gdf_rec_data = reinterpret_cast<gdf_rec_t*>(gdf_rec.getRecordData());
            gdf_rec_data->key = key_space;
            gdf_rec_data->type = COLUMN_REC;
            gdf_rec_data->size = column_size;
            gdf_rec_data->encoding = kv.value.field->encoding;
            gdf_rec_data->num_rows = kv.value.field->length();
            StringLib::copy(gdf_rec_data->name, kv.key, MAX_NAME_SIZE);

            // serialize column data into record
            long data_offset = size_of_sizes;
            uint32_t* sizes_ptr = reinterpret_cast<uint32_t*>(gdf_rec_data->data);
            for(long j = 0; j < kv.value.field->length(); j++)
            {
                const Field* field_list = kv.value.field->get(j);
                sizes_ptr[j] = static_cast<uint32_t>(field_list->length()) * RecordObject::FIELD_TYPE_BYTES[encoded_type];
                data_offset += field_list->serialize(&(gdf_rec_data->data[data_offset]), data_size - data_offset);
            }

            // send column record
            gdf_rec.post(&pub, 0, NULL, true, timeout);
        }
    }

    // create and send meta records
    Dictionary<meta_entry_t>::Iterator meta_iter(metaFields.fields);
    for(int i = 0; i < meta_iter.length; i++)
    {
        const Dictionary<meta_entry_t>::kv_t kv = meta_iter[i];

        // determine size of element
        const uint32_t value_encoding = kv.value.field->getValueEncoding();
        if(value_encoding >= RecordObject::NUM_FIELD_TYPES) continue;
        const long element_size = kv.value.field->length() * RecordObject::FIELD_TYPE_BYTES[value_encoding];
        const long rec_size = offsetof(gdf_rec_t, data) + element_size;

        // create meta record
        RecordObject gdf_rec(gdfRecType, rec_size);
        gdf_rec_t* gdf_rec_data = reinterpret_cast<gdf_rec_t*>(gdf_rec.getRecordData());
        gdf_rec_data->key = key_space;
        gdf_rec_data->type = META_REC;
        gdf_rec_data->size = element_size;
        gdf_rec_data->encoding = kv.value.field->encoding;
        gdf_rec_data->num_rows = length();
        StringLib::copy(gdf_rec_data->name, kv.key, MAX_NAME_SIZE);

        // serialize metadata element into record
        const long bytes_serialized = kv.value.field->serialize(gdf_rec_data->data, element_size);
        if(bytes_serialized != element_size) throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to serialize metadata %s: %ld", gdf_rec_data->name, bytes_serialized);

        // send meta record
        gdf_rec.post(&pub, 0, NULL, true, timeout);
    }

    // create and send CRS record
    if (!crs.empty())
    {
        const long crs_size = crs.size();
        const long rec_size = offsetof(gdf_rec_t, data) + crs_size;

        RecordObject gdf_rec(gdfRecType, rec_size);
        gdf_rec_t* gdf_rec_data = reinterpret_cast<gdf_rec_t*>(gdf_rec.getRecordData());
        gdf_rec_data->key       = key_space;
        gdf_rec_data->type      = CRS_REC;
        gdf_rec_data->size      = crs_size;
        gdf_rec_data->encoding  = Field::STRING;
        gdf_rec_data->num_rows  = 1;
        StringLib::copy(gdf_rec_data->name, CRS_KEY, MAX_NAME_SIZE);
        memcpy(gdf_rec_data->data, crs.data(), crs_size);
        gdf_rec.post(&pub, 0, NULL, true, timeout);
    }

    // create and send eof record
    {
        const long rec_size = offsetof(gdf_rec_t, data) + sizeof(uint32_t); // for number of columns
        RecordObject gdf_rec(gdfRecType, rec_size);
        gdf_rec_t* gdf_rec_data = reinterpret_cast<gdf_rec_t*>(gdf_rec.getRecordData());
        gdf_rec_data->key = key_space;
        gdf_rec_data->type = EOF_REC;
        gdf_rec_data->num_rows = length();
        eof_subrec_t eof_subrec = {.num_columns = static_cast<uint32_t>(columnFields.length())};
        memcpy(gdf_rec_data->data, &eof_subrec, sizeof(eof_subrec_t));
        gdf_rec.post(&pub, 0, NULL, true, timeout);
    }
}

/*----------------------------------------------------------------------------
 * receiveThread
 *----------------------------------------------------------------------------*/
void* GeoDataFrame::receiveThread (void* parm)
{
    // cast parameter
    receive_info_t* info = static_cast<receive_info_t*>(parm);

    // initialize processing variables
    Subscriber inq(info->inq_name);
    Publisher outq(info->outq_name);
    const double timelimit = TimeLib::latchtime() + (static_cast<double>(info->timeout) / 1000.0);
    bool complete = false;
    int32_t source_id = 0;
    Subscriber::msgRef_t ref;

    // indicate thread is ready to receive
    info->ready_signal.lock();
    {
        info->ready = true;
        info->ready_signal.signal();
    }
    info->ready_signal.unlock();

    // create table of dataframe records
    typedef Table<vector<rec_ref_t>*> df_table_t;
    df_table_t df_table(info->num_channels);

    try
    {
        // while receiving messages
        while(info->dataframe->active.load() && !complete)
        {
            const int recv_status = inq.receiveRef(ref, SYS_TIMEOUT);

            if(recv_status == MsgQ::STATE_TIMEOUT)
            {
                if(TimeLib::latchtime() > timelimit)
                {
                    // timeout has occurred
                    throw RunTimeException(CRITICAL, RTE_FAILURE, "timeout occurred receiving dataframe");
                }
            }
            else if(recv_status < 0)
            {
                // error in receiving
                inq.dereference(ref);
                throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to receive records from queue <%s>: %d", inq.getName(), recv_status);
            }
            else if(ref.size < 0)
            {
                // error in data
                inq.dereference(ref);
                throw RunTimeException(CRITICAL, RTE_FAILURE, "received record of invalid size from queue <%s>: %d", inq.getName(), ref.size);
            }
            else if(ref.size > 0)
            {
                // process record
                const RecordInterface rec(reinterpret_cast<unsigned char*>(ref.data), ref.size);
                if(StringLib::match(rec.getRecordType(), gdfRecType))
                {
                    // get dataframe key
                    gdf_rec_t* rec_data = reinterpret_cast<gdf_rec_t*>(rec.getRecordData());
                    const uint64_t key = rec_data->key;

                    // handle CRS record
                    if(rec_data->type == CRS_REC)
                    {
                        const string crs(reinterpret_cast<const char*>(rec_data->data), rec_data->size);
                        if(info->dataframe->getCRS().empty())
                        {
                            info->dataframe->setCRS(crs);
                        }
                        else
                        {
                            assert(info->dataframe->getCRS() == crs);
                        }

                        inq.dereference(ref);
                        continue;
                    }

                    // get reference to list of key'ed dataframe
                    vector<rec_ref_t>* df_rec_list = NULL;
                    if(!df_table.find(key, df_table_t::MATCH_EXACTLY, &df_rec_list))
                    {
                        df_rec_list = new vector<rec_ref_t>;
                        if(!df_table.add(key, df_rec_list, true))
                        {
                            delete df_rec_list;
                            inq.dereference(ref);
                            throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to add record list to table");
                        }
                    }

                    // assemble on eof
                    if(rec_data->type == EOF_REC)
                    {
                        // pull out data from eof rec
                        const eof_subrec_t eof_subrec = *reinterpret_cast<const eof_subrec_t*>(rec_data->data);
                        inq.dereference(ref); // dereference eof rec since it is not added to table of dataframe refs

                        // append columns and metadata to dataframe
                        for(rec_ref_t& entry: *df_rec_list)
                        {
                            info->dataframe->appendDataframe(entry.rec, source_id);
                            inq.dereference(entry.ref);
                        }

                        // check number of columns
                        if(info->dataframe->columnFields.length() < eof_subrec.num_columns)
                        {
                            throw RunTimeException(CRITICAL, RTE_FAILURE, "incomplete number of columns received: %ld < %u", info->dataframe->length(), eof_subrec.num_columns);
                        }

                        // bump source id now that dataframe is full appended
                        source_id++;

                        // remove key'ed dataframe list from table
                        df_table.remove(key);
                    }
                    else
                    {
                        // add reference to list of key'ed dataframe
                        const rec_ref_t rec_ref = {
                            .ref = ref,
                            .rec = rec_data
                        };
                        df_rec_list->push_back(rec_ref);
                    }
                }
                else
                {
                    // record of non-targeted type - pass through
                    outq.postCopy(ref.data, ref.size);
                    inq.dereference(ref);
                }
            }
            else if(ref.size == 0)
            {
                // terminator indicates dataframe is complete
                inq.dereference(ref);

                // complete dataframe
                info->dataframe->populateDataframe();
                complete = true;
            }
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), RTE_FAILURE, &outq, NULL, "Error receiving dataframe: %s", e.what());
        info->dataframe->inError = true;

        // deference any outstanding messages in any of the lists
        uint64_t key = df_table.first(NULL);
        while(key != INVALID_KEY)
        {
            for(rec_ref_t& entry: *df_table[key])
            {
                inq.dereference(entry.ref);
            }
            df_table.remove(key);
            key = df_table.next(NULL);
        }
    }

    // mark complete and clean up
    info->dataframe->signalComplete();
    delete info;
    return NULL;
}

/*----------------------------------------------------------------------------
 * runThread
 *----------------------------------------------------------------------------*/
void* GeoDataFrame::runThread (void* parm)
{
    assert(parm);
    GeoDataFrame* dataframe = static_cast<GeoDataFrame*>(parm);
    bool complete = false;
    while(dataframe->active.load())
    {
        if(!complete)
        {
            complete = dataframe->waitComplete(SYS_TIMEOUT);
        }
        else
        {
            GeoDataFrame::FrameRunner* runner = NULL;
            const int recv_status = dataframe->subRunQ.receiveCopy(&runner, sizeof(runner), SYS_TIMEOUT);
            if(recv_status > 0)
            {
                if(runner)
                {
                    // execute frame runner
                    if(!runner->run(dataframe))
                    {
                        // exit loop on error
                        mlog(CRITICAL, "error encountered in frame runner: %s", runner->getType());
                        dataframe->active.store(false);
                    }

                    // release frame runner
                    runner->releaseLuaObject();
                }
                else
                {
                    // exit loop on termination
                    dataframe->active.store(false);
                }
            }
        }
    }
    dataframe->signalRunComplete();
    return NULL;
}

/*----------------------------------------------------------------------------
 * toJson
 *----------------------------------------------------------------------------*/
string GeoDataFrame::toJson (void) const
{
    return FString("{\"meta\":%s,\"gdf\":%s}", metaFields.toJson().c_str(), columnFields.toJson().c_str()).c_str();
}

/*----------------------------------------------------------------------------
 * toLua
 *----------------------------------------------------------------------------*/
int GeoDataFrame::toLua (lua_State* L) const
{
    lua_newtable(L);

    lua_pushstring(L, META);
    metaFields.toLua(L);
    lua_settable(L, -3);

    lua_pushstring(L, GDF);
    columnFields.toLua(L);
    lua_settable(L, -3);

    return 1;
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void GeoDataFrame::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        lua_getfield(L, index, META);
        metaFields.fromLua(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, GDF);
        columnFields.fromLua(L, -1);
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * luaInError - inerror() --> true | false
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaInError (lua_State* L)
{
    try
    {
        const GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        lua_pushboolean(L, lua_obj->inError);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error determining state of dataframe: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaNumRows - numrows()
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaNumRows (lua_State* L)
{
    try
    {
        const GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        lua_pushinteger(L, lua_obj->numRows);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error determining number of rows in dataframe: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaNumColumns - numcols()
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaNumColumns (lua_State* L)
{
    try
    {
        const GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        lua_pushinteger(L, lua_obj->columnFields.length());
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error determining number of columns in dataframe: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaExport - export() --> lua table
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaExport (lua_State* L)
{
    try
    {
        const GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        return lua_obj->toLua(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error exporting %s: %s", OBJECT_TYPE, e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaSend - :send(<rspq>, [<key_space>], [<timeout>])
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaSend(lua_State* L)
{
    bool status = true;

    try
    {
        // get parameters
        GeoDataFrame*   dataframe   = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        const char*     rspq        = getLuaString(L, 2);
        const uint64_t  key_space   = getLuaInteger(L, 3, true, RequestFields::DEFAULT_KEY_SPACE);
        const int       timeout     = getLuaInteger(L, 4, true, SYS_TIMEOUT);

        // send dataframe
        dataframe->sendDataframe(rspq, key_space, timeout);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error sending dataframe: %s", e.what());
        status = false;
    }

    // return completion status
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaReceive - :receive(<input q>, <outq>, [<number of resources>], [<timeout>])
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaReceive(lua_State* L)
{
    bool status = false;
    receive_info_t* info = NULL;

    try
    {
        // parameters
        GeoDataFrame* dataframe = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        const char* inq_name = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        const int num_channels = getLuaInteger(L, 4, true, 1);
        const int timeout = getLuaInteger(L, 5, true, SystemConfig::settings().requestTimeoutSec.value * 1000);

        // check if already received
        if(dataframe->receivePid)
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "dataframe already received");
        }

        // kick off receive thread
        info = new receive_info_t(dataframe, inq_name, outq_name, num_channels, timeout);
        dataframe->receivePid = new Thread(receiveThread, info);

        // wait for ready signal
        // this guarantees the receive thread is ready to receive data
        // specifically, the subscriber to the message queue needs to be created so
        // that subsequent posts to the message queue are not dropped
        info->ready_signal.lock();
        {
            if(!info->ready)
            {
                if(info->ready_signal.wait(0, timeout))
                {
                    status = info->ready;          // thread signalled us; success only if it set ready=true
                }
            }
            else
            {
                status = true;                     // thread set ready before we acquired the lock
            }
        }
        info->ready_signal.unlock();
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error receiving dataframe: %s", e.what());
        status = false;
        delete info;
    }

    // return status
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaGetRowData - [<row index>]
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaGetRowData(lua_State* L)
{
    try
    {
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        const long index = getLuaInteger(L, 2) - 1; // indexing in lua starts at 1

        // check index
        if(index < 0) throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid index: %ld", index + 1);

        // create table and populate with column values
        lua_newtable(L);
        column_entry_t entry;
        const char* key = lua_obj->columnFields.fields.first(&entry);
        while(key != NULL)
        {
            lua_pushstring(L, key);
            entry.field->toLua(L, index);
            lua_settable(L, -3);
            key = lua_obj->columnFields.fields.next(&entry);
        }

        // return table
        return 1;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", FrameColumn::LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaGetColumnData - [<column name>]
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaGetColumnData(lua_State* L)
{
    try
    {
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        const char* name = getLuaString(L, 2);

        // check the metatable for the key (to support functions)
        luaL_getmetatable(L, lua_obj->LuaMetaName);
        lua_pushstring(L, name);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1))  return 1;
        else lua_pop(L, 1);

        // handle column access
        const Field& column_field = lua_obj->columnFields[name];
        return createLuaObject(L, new FrameColumn(L, column_field));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", FrameColumn::LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaGetMetaData - meta(<field name>)
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaGetMetaData (lua_State* L)
{
    try
    {
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        const char* field_name = getLuaString(L, 2);
        return lua_obj->metaFields[field_name].toLua(L);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting metadata: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaGetCRS - crs()
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaGetCRS (lua_State* L)
{
    try
    {
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));

        const string& crs = lua_obj->getCRS();
        if(crs.empty()) lua_pushnil(L);
        else            lua_pushstring(L, crs.c_str());
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting metadata: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaRun
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaRun  (lua_State* L)
{
    bool status = false;
    GeoDataFrame::FrameRunner* runner = NULL;

    try
    {
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        bool pass_to_runner = false;

        try
        {
            // try to get a FrameRunner object
            runner = dynamic_cast<GeoDataFrame::FrameRunner*>(getLuaObject(L, 2, GeoDataFrame::FrameRunner::OBJECT_TYPE));
            pass_to_runner = true;
        }
        catch(const RunTimeException& e)
        {
            (void)e;

            // try to get the termination string
            const char* termination_string = getLuaString(L, 2, true, NULL);
            if(termination_string && StringLib::match(termination_string, TERMINATE))
            {
                pass_to_runner = true;
            }
        }

        if(pass_to_runner)
        {
            const int post_state = lua_obj->pubRunQ.postCopy(&runner, sizeof(runner));
            if(post_state != MsgQ::STATE_OKAY)
            {
                throw RunTimeException(CRITICAL, RTE_FAILURE, "run queue post failed: %d", post_state);
            }
            status = true;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error attaching runner: %s", e.what());
        if(runner) runner->releaseLuaObject();
    }

    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaRunComplete - :finished([<timeout is milliseconds>])
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaRunComplete(lua_State* L)
{
    bool status = false;

    try
    {
        // get self
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));

        // get parameters
        const int timeout = getLuaInteger(L, 2, true, IO_PEND);
        const char* rspq = getLuaString(L, 3, true, NULL);
        int interval = getLuaInteger(L, 4, true, DEFAULT_WAIT_INTERVAL);

        // wait on signal
        if(rspq && timeout > 0)
        {
            Publisher pub(rspq);
            int duration = 0;
            interval = MIN(interval, timeout);
            while(!status)
            {
                status = lua_obj->waitRunComplete(interval);
                if(!status)
                {
                    if(pub.getSubCnt() <= 0)
                    {
                        alert(ERROR, RTE_TIMEOUT, &pub, NULL, "request <%s> terminated while waiting", rspq);
                        break;
                    }
                    else if(duration >= timeout)
                    {
                        alert(ERROR, RTE_TIMEOUT, &pub, NULL, "request <%s> timed-out after %d seconds", rspq, timeout);
                        break;
                    }
                    else
                    {
                        duration += interval;
                        alert(INFO, RTE_TIMEOUT, &pub, NULL, "request <%s> ... running %d of %d seconds", rspq, duration / 1000, timeout / 1000);
                    }
                }
            }
        }
        else
        {
            status = lua_obj->waitRunComplete(timeout);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error waiting for run completion: %s", e.what());
    }

    // return completion status
    return returnLuaStatus(L, status);
}
