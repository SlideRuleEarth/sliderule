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

#include "OsApi.h"
#include "GeoDataFrame.h"
#include "RequestFields.h"
#include "LuaLibraryMsg.h"
#include "MsgQ.h"
#include "Table.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoDataFrame::OBJECT_TYPE = "GeoDataFrame";
const char* GeoDataFrame::GDF = "gdf";
const char* GeoDataFrame::META = "meta";
const char* GeoDataFrame::TERMINATE = "terminate";

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
        if(index < 0) throw RunTimeException(CRITICAL, RTE_ERROR, "invalid index: %ld", index + 1);

        // check the metatable for the key (to support functions)
        luaL_getmetatable(L, lua_obj->LuaMetaName);
        lua_pushinteger(L, index);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1))  return 1;
        else lua_pop(L, 1);

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
 * FrameColumn: Constructor -
 *----------------------------------------------------------------------------*/
GeoDataFrame::FrameRunner::FrameRunner(lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table),
    runtime(0.0)
{
    LuaEngine::setAttrFunc(L, "runtime", luaGetRunTime);
}

/*----------------------------------------------------------------------------
 * FrameColumn: luaGetRunTime -
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
 * luaGetRunTime -
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
 * init
 *----------------------------------------------------------------------------*/
void GeoDataFrame::init(void)
{
    RECDEF(gdfRecType, gdfRecDef, sizeof(gdf_rec_t), NULL);
}

/*----------------------------------------------------------------------------
 * luaCreate - dataframe([<column table>], [<meta table>])
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaCreate (lua_State* L)
{
    bool status = true;
    GeoDataFrame* dataframe = NULL;

    try
    {
        // parameter indices
        const int column_table_index = 1;
        const int meta_table_index = 2;

        // create initial dataframe
        dataframe = new GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE, {}, {}, true);
        lua_pop(L, 2);

        // column table
        if(lua_gettop(L) >= column_table_index)
        {
            // build columns of dataframe
            lua_pushnil(L); // prime the stack for the next key
            while(lua_next(L, column_table_index) != 0)
            {
                if(lua_isstring(L, -2))
                {
                    const char* name = StringLib::duplicate(lua_tostring(L, -2));
                    FieldColumn<double>* column = new FieldColumn<double>();
                    const FieldDictionary::entry_t entry = {name, column};
                    dataframe->columnFields.add(entry);
                    mlog(INFO, "Adding column %ld: %s", dataframe->columnFields.length(), name);
                }
                lua_pop(L, 1); // remove the key
            }

            // import dataframe columns from lua
            dataframe->columnFields.fromLua(L, column_table_index);

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
                    throw RunTimeException(CRITICAL, RTE_ERROR, "number of rows must match for all columns, %ld != %ld", dataframe->numRows, field->length());
                }
            }
        }

        //  meta table
        if(lua_gettop(L) >= meta_table_index)
        {
            // build keys of metadata
            lua_pushnil(L); // prime the stack for the next key
            while(lua_next(L, meta_table_index) != 0)
            {
                if(lua_isstring(L, -2))
                {
                    const char* name = StringLib::duplicate(lua_tostring(L, -2));
                    FieldElement<double>* element = new FieldElement<double>();
                    element->setEncodingFlags(META_COLUMN);
                    const FieldDictionary::entry_t entry = {name, element};
                    dataframe->metaFields.add(entry);
                    mlog(INFO, "Adding metadata %s", name);
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
long GeoDataFrame::appendFromBuffer(const char* name, const uint8_t* buffer, int size) const
{
    Field* field = getColumn(name);
    switch(field->getValueEncoding())
    {
        case Field::BOOL:
        {
            FieldColumn<bool>* column = dynamic_cast<FieldColumn<bool>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::INT8:
        {
            FieldColumn<int8_t>* column = dynamic_cast<FieldColumn<int8_t>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::INT16:
        {
            FieldColumn<int16_t>* column = dynamic_cast<FieldColumn<int16_t>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::INT32:
        {
            FieldColumn<int32_t>* column = dynamic_cast<FieldColumn<int32_t>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::INT64:
        {
            FieldColumn<int64_t>* column = dynamic_cast<FieldColumn<int64_t>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::UINT8:
        {
            FieldColumn<uint8_t>* column = dynamic_cast<FieldColumn<uint8_t>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::UINT16:
        {
            FieldColumn<uint16_t>* column = dynamic_cast<FieldColumn<uint16_t>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::UINT32:
        {
            FieldColumn<uint32_t>* column = dynamic_cast<FieldColumn<uint32_t>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::UINT64:
        {
            FieldColumn<uint64_t>* column = dynamic_cast<FieldColumn<uint64_t>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::FLOAT:
        {
            FieldColumn<float>* column = dynamic_cast<FieldColumn<float>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::DOUBLE:
        {
            FieldColumn<double>* column = dynamic_cast<FieldColumn<double>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::STRING:
        {
            FieldColumn<string>* column = dynamic_cast<FieldColumn<string>*>(field);
            return column->appendBuffer(buffer, size);
        }
        case Field::TIME8:
        {
            FieldColumn<time8_t>* column = dynamic_cast<FieldColumn<time8_t>*>(field);
            return column->appendBuffer(buffer, size);
        }
        default:
        {
            mlog(ERROR, "Cannot add column <%s> of type %d", name, field->getValueEncoding());
            return 0;
        }
    }
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
 * addColumn
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::addColumn (const char* name, Field* column)
{
    const FieldDictionary::entry_t entry = {name, column};
    columnFields.add(entry);
    return true;
}

/*----------------------------------------------------------------------------
 * addColumn
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::addColumn (const char* name, uint32_t _type)
{
    Field* column = NULL;

    const char* _name = StringLib::duplicate(name);
    switch(_type)
    {
        case Field::BOOL:   column = new FieldColumn<bool>();       break;
        case Field::INT8:   column = new FieldColumn<int8_t>();     break;
        case Field::INT16:  column = new FieldColumn<int16_t>();    break;
        case Field::INT32:  column = new FieldColumn<int32_t>();    break;
        case Field::INT64:  column = new FieldColumn<int64_t>();    break;
        case Field::UINT8:  column = new FieldColumn<uint8_t>();    break;
        case Field::UINT16: column = new FieldColumn<uint16_t>();   break;
        case Field::UINT32: column = new FieldColumn<uint32_t>();   break;
        case Field::UINT64: column = new FieldColumn<uint64_t>();   break;
        case Field::FLOAT:  column = new FieldColumn<float>();      break;
        case Field::DOUBLE: column = new FieldColumn<double>();     break;
        case Field::STRING: column = new FieldColumn<string>();     break;
        case Field::TIME8:  column = new FieldColumn<time8_t>();    break;
        default:
        {
            mlog(ERROR, "Cannot add column <%s> of type %d", _name, _type);
            return false;
        }
    }

    if(!addColumn(_name, column))
    {
        delete [] _name;
        delete column;
        mlog(ERROR, "Failed to add column <%s>", _name);
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * getColumn
 *----------------------------------------------------------------------------*/
Field* GeoDataFrame::getColumn (const char* name, Field::type_t _type, bool no_throw) const
{
    if(!no_throw)
    {
        const FieldDictionary::entry_t entry = columnFields.fields[name];
        if(!entry.field) throw RunTimeException(CRITICAL, RTE_ERROR, "%s field is null", name);
        if(_type != Field::FIELD && _type != entry.field->type) throw RunTimeException(CRITICAL, RTE_ERROR, "%s is incorrect type: %d", name, static_cast<int>(entry.field->type));
        return entry.field;
    }
    else
    {
        FieldDictionary::entry_t entry;
        if(columnFields.fields.find(name, &entry))
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
 * addMetaData
 *----------------------------------------------------------------------------*/
void GeoDataFrame::addMetaData (const char* name, Field* column)
{
    const FieldDictionary::entry_t entry = {name, column};
    metaFields.add(entry);
}

/*----------------------------------------------------------------------------
 * getMetaData
 *----------------------------------------------------------------------------*/
Field* GeoDataFrame::getMetaData (const char* name, Field::type_t _type, bool no_throw) const
{
    if(!no_throw)
    {
        const FieldDictionary::entry_t entry = metaFields.fields[name];
        if(!entry.field) throw RunTimeException(CRITICAL, RTE_ERROR, "%s field is null", name);
        if(_type != entry.field->type) throw RunTimeException(CRITICAL, RTE_ERROR, "%s is incorrect type: %d", name, static_cast<int>(entry.field->type));
        return entry.field;
    }
    else
    {
        FieldDictionary::entry_t entry;
        if(metaFields.fields.find(name, &entry))
        {
            if(_type == entry.field->type)
            {
                return entry.field;
            }
        }
        return NULL;
    }
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
const FieldColumn<double>* GeoDataFrame::getZColumn (void) const
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
const Dictionary<FieldDictionary::entry_t>& GeoDataFrame::getColumns(void) const
{
    return columnFields.fields;
}

/*----------------------------------------------------------------------------
 * getMeta
 *----------------------------------------------------------------------------*/
const Dictionary<FieldDictionary::entry_t>& GeoDataFrame::getMeta(void) const
{
    return metaFields.fields;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoDataFrame::GeoDataFrame( lua_State* L,
                            const char* meta_name,
                            const struct luaL_Reg meta_table[],
                            const std::initializer_list<FieldDictionary::entry_t>& column_list,
                            const std::initializer_list<FieldDictionary::entry_t>& meta_list,
                            bool free_on_delete):
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
    active(true),
    receivePid(NULL),
    runPid(NULL),
    pubRunQ(NULL),
    subRunQ(pubRunQ),
    runComplete(false),
    freeOnDelete(free_on_delete)
{
    // set lua functions
    LuaEngine::setAttrFunc(L, "inerror",    luaInError);
    LuaEngine::setAttrFunc(L, "numrows",    luaNumRows);
    LuaEngine::setAttrFunc(L, "numcols",    luaNumColumns);
    LuaEngine::setAttrFunc(L, "export",     luaExport);
    LuaEngine::setAttrFunc(L, "send",       luaSend);
    LuaEngine::setAttrFunc(L, "receive",    luaReceive);
    LuaEngine::setAttrFunc(L, "__index",    luaGetColumnData);
    LuaEngine::setAttrFunc(L, "meta",       luaGetMetaData);
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
    active = false;
    delete receivePid;
    delete runPid;

    // release pending frame runners
    int recv_status = MsgQ::STATE_OKAY;
    while(recv_status > 0)
    {
        GeoDataFrame::FrameRunner* runner;
        recv_status = subRunQ.receiveCopy(&runner, sizeof(runner), SYS_TIMEOUT);
        if(recv_status > 0 && runner) runner->releaseLuaObject();
    }

    // free entries in FieldDictionaries
    if(freeOnDelete)
    {
        // column dictionary
        {
            FieldDictionary::entry_t entry;
            const char* key = columnFields.fields.first(&entry);
            while(key != NULL)
            {
                delete [] entry.name;
                delete entry.field;
                key = columnFields.fields.next(&entry);
            }
        }

        // meta dictionary
        {
            FieldDictionary::entry_t entry;
            const char* key = metaFields.fields.first(&entry);
            while(key != NULL)
            {
                delete [] entry.name;
                delete entry.field;
                key = metaFields.fields.next(&entry);
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * add_column
 *----------------------------------------------------------------------------*/
template<class T>
bool add_column(GeoDataFrame* dataframe, GeoDataFrame::gdf_rec_t* gdf_rec_data)
{
    // get column from dataframe
    FieldColumn<T>* column = dynamic_cast<FieldColumn<T>*>(dataframe->getColumn(gdf_rec_data->name, Field::COLUMN, true));

    // create new column if not found
    if(!column)
    {
        column = new FieldColumn<T>(gdf_rec_data->encoding, GeoDataFrame::DEFAULT_RECEIVED_COLUMN_CHUNK_SIZE);
        const char* _name = StringLib::duplicate(gdf_rec_data->name);
        if(!dataframe->addColumn(_name, column))
        {
            delete column;
            delete [] _name;
            return false;
        }
    }
    else if(column->encoding != gdf_rec_data->encoding)
    {
        return false;
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
        return false;
    }

    // success
    return true;
}

/*----------------------------------------------------------------------------
 * appendDataframe
 *----------------------------------------------------------------------------*/
void GeoDataFrame::appendDataframe(GeoDataFrame::gdf_rec_t* gdf_rec_data)
{
    const rec_type_t _type = static_cast<rec_type_t>(gdf_rec_data->type);
    if((_type == GeoDataFrame::COLUMN_REC) || (_type == GeoDataFrame::META_REC))
    {
        const uint32_t value_encoding = gdf_rec_data->encoding & Field::VALUE_MASK;
        bool add_status = true;

        // add to column
        switch(value_encoding)
        {
            case BOOL:      add_status = add_column<bool>    (this, gdf_rec_data); break;
            case INT8:      add_status = add_column<int8_t>  (this, gdf_rec_data); break;
            case INT16:     add_status = add_column<int16_t> (this, gdf_rec_data); break;
            case INT32:     add_status = add_column<int32_t> (this, gdf_rec_data); break;
            case INT64:     add_status = add_column<int64_t> (this, gdf_rec_data); break;
            case UINT8:     add_status = add_column<uint8_t> (this, gdf_rec_data); break;
            case UINT16:    add_status = add_column<uint16_t>(this, gdf_rec_data); break;
            case UINT32:    add_status = add_column<uint32_t>(this, gdf_rec_data); break;
            case UINT64:    add_status = add_column<uint64_t>(this, gdf_rec_data); break;
            case FLOAT:     add_status = add_column<float>   (this, gdf_rec_data); break;
            case DOUBLE:    add_status = add_column<double>  (this, gdf_rec_data); break;
            case TIME8:     add_status = add_column<time8_t> (this, gdf_rec_data); break;
            default:        add_status = false; break;
        }

        // check add status
        if(!add_status)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "failed to add metadata %s of size %u with encoding %u", gdf_rec_data->name, gdf_rec_data->size, gdf_rec_data->encoding);
        }
    }
    else if(_type == GeoDataFrame::EOF_REC)
    {
        // check number of columns
        const uint32_t* num_colums_ptr = reinterpret_cast<const uint32_t*>(gdf_rec_data->data);
        if(columnFields.length() < *num_colums_ptr)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "incomplete number of columns received: %ld < %u", columnFields.length(), *num_colums_ptr);
        }

        // check number of rows
        if(numRows != gdf_rec_data->num_rows)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid number of rows identified: %ld != %u", numRows, gdf_rec_data->num_rows);
        }
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
    const double timelimit = TimeLib::latchtime() + static_cast<double>(info->timeout);
    bool complete = false;
    Subscriber::msgRef_t ref;

    // indicate thread is ready to receive
    info->ready_signal.lock();
    {
        info->ready = true;
        info->ready_signal.signal();
    }
    info->ready_signal.unlock();

    // create table of dataframe records
    typedef Table<vector<Subscriber::msgRef_t>> df_table_t;
    df_table_t df_table(info->total_resources, df_table_t::identity, true);

    try
    {
        // while receiving messages
        while(info->dataframe->active && !complete)
        {
            const int recv_status = inq.receiveRef(ref, SYS_TIMEOUT);

            if((recv_status == MsgQ::STATE_TIMEOUT) && (TimeLib::latchtime() > timelimit))
            {
                // timeout has occurred
                throw RunTimeException(CRITICAL, RTE_ERROR, "timeout occurred receiving dataframe");
            }
            else if(recv_status < 0)
            {
                // error in receiving
                inq.dereference(ref);
                throw RunTimeException(CRITICAL, RTE_ERROR, "failed to receive records from queue <%s>: %d", inq.getName(), recv_status);
            }
            else if(ref.size < 0)
            {
                // error in data
                inq.dereference(ref);
                throw RunTimeException(CRITICAL, RTE_ERROR, "received record of invalid size from queue <%s>: %d", inq.getName(), ref.size);
            }
            else if(ref.size > 0)
            {
                // process record
                const RecordInterface rec(reinterpret_cast<unsigned char*>(ref.data), ref.size);
                if(StringLib::match(rec.getRecordType(), gdfRecType))
                {
                    // add reference to list specific to key'ed dataframe
                    gdf_rec_t* rec_data = reinterpret_cast<gdf_rec_t*>(rec.getRecordData());
                    const uint64_t key = rec_data->key;
                    df_table[key].push_back(ref);

                    // assemble on eof
                    if(rec_data->type == EOF_REC)
                    {
                        rec_data->num_rows += info->dataframe->numRows; // modify in place to hold new num rows
                        for(Subscriber::msgRef_t& gdf_ref: df_table[key])
                        {
                            // append columns and metadata to dataframe
                            const RecordInterface gdf_rec(reinterpret_cast<unsigned char*>(gdf_ref.data), gdf_ref.size);
                            gdf_rec_t* gdf_rec_data = reinterpret_cast<gdf_rec_t*>(gdf_rec.getRecordData());
                            info->dataframe->appendDataframe(gdf_rec_data);
                            inq.dereference(gdf_ref);
                        }
                    }

                    // remove key'ed dataframe list from table
                    df_table.remove(key);
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
                info->dataframe->populateGeoColumns();
                complete = true;
            }
        }
    }
    catch(const RunTimeException& e)
    {
        alert(e.level(), RTE_ERROR, &outq, NULL, "Error receiving dataframe: %s", e.what());
        info->dataframe->inError = true;

        // deference any outstanding messages in any of the lists
        uint64_t key = df_table.first(NULL);
        while(key != INVALID_KEY)
        {
            for(Subscriber::msgRef_t& gdf_ref: df_table[key])
            {
                inq.dereference(gdf_ref);
            }
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
    while(dataframe->active)
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
                    if(dataframe->length() > 0)
                    {
                        // execute frame runner
                        if(!runner->run(dataframe))
                        {
                            // exit loop on error
                            mlog(CRITICAL, "error encountered in frame runner: %s", runner->getName());
                            dataframe->active = false;
                        }
                    }

                    // release frame runner
                    runner->releaseLuaObject();
                }
                else
                {
                    // exit loop on termination
                    dataframe->active = false;
                }
            }
        }
    }
    dataframe->signalRunComplete();
    return NULL;
}

/*----------------------------------------------------------------------------
 * populateGeoColumn
 *----------------------------------------------------------------------------*/
void GeoDataFrame::populateGeoColumns (void)
{
    // populate geo columns
    Dictionary<FieldDictionary::entry_t>::Iterator iter(columnFields.fields);
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
            assert(field->getValueEncoding() == DOUBLE);
            zColumn = dynamic_cast<const FieldColumn<double>*>(field);
            zColumnName = name;
        }
    }
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
        uint64_t        key_space   = getLuaInteger(L, 3, true, RequestFields::DEFAULT_KEY_SPACE);
        const int       timeout     = getLuaInteger(L, 4, true, SYS_TIMEOUT);

        // massage key_space
        if(key_space == INVALID_KEY) key_space = 0;

        // create publisher
        Publisher pub(rspq);

        // create and send column records
        Dictionary<FieldDictionary::entry_t>::Iterator column_iter(dataframe->columnFields.fields);
        for(int i = 0; i < column_iter.length; i++)
        {
            const Dictionary<FieldDictionary::entry_t>::kv_t kv = column_iter[i];

            // determine size of column
            const uint32_t value_encoding = kv.value.field->getValueEncoding();
            assert(value_encoding >= 0 && value_encoding < RecordObject::NUM_FIELD_TYPES);
            const long column_size = kv.value.field->length() * RecordObject::FIELD_TYPE_BYTES[value_encoding];
            const long rec_size = offsetof(gdf_rec_t, data) + column_size;

            // create column record
            RecordObject gdf_rec(gdfRecType, rec_size);
            gdf_rec_t* gdf_rec_data = reinterpret_cast<gdf_rec_t*>(gdf_rec.getRecordData());
            gdf_rec_data->key = key_space;
            gdf_rec_data->type = COLUMN_REC;
            gdf_rec_data->size = column_size;
            gdf_rec_data->encoding = kv.value.field->encoding;
            gdf_rec_data->num_rows = kv.value.field->length();
            StringLib::copy(gdf_rec_data->name, kv.value.name, MAX_NAME_SIZE);

            // serialize column data into record
            const long bytes_serialized = kv.value.field->serialize(gdf_rec_data->data, column_size);
            if(bytes_serialized != column_size) throw RunTimeException(CRITICAL, RTE_ERROR, "failed to serialize column %s: %ld", gdf_rec_data->name, bytes_serialized);

            // send column record
            gdf_rec.post(&pub, 0, NULL, true, timeout);
        }

        // create and send meta records
        Dictionary<FieldDictionary::entry_t>::Iterator meta_iter(dataframe->metaFields.fields);
        for(int i = 0; i < meta_iter.length; i++)
        {
            const Dictionary<FieldDictionary::entry_t>::kv_t kv = meta_iter[i];

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
            gdf_rec_data->num_rows = dataframe->length();
            StringLib::copy(gdf_rec_data->name, kv.value.name, MAX_NAME_SIZE);

            // serialize column data into record
            const long bytes_serialized = kv.value.field->serialize(gdf_rec_data->data, element_size);
            if(bytes_serialized != element_size) throw RunTimeException(CRITICAL, RTE_ERROR, "failed to serialize metadata %s: %ld", gdf_rec_data->name, bytes_serialized);

            // send column record
            gdf_rec.post(&pub, 0, NULL, true, timeout);
        }

        // create and send eof record
        {
            const long rec_size = offsetof(gdf_rec_t, data) + sizeof(uint32_t); // for number of columns
            RecordObject gdf_rec(gdfRecType, rec_size);
            gdf_rec_t* gdf_rec_data = reinterpret_cast<gdf_rec_t*>(gdf_rec.getRecordData());
            gdf_rec_data->key = key_space;
            gdf_rec_data->type = EOF_REC;
            gdf_rec_data->num_rows = dataframe->length();
            uint32_t num_columns = dataframe->columnFields.length();
            memcpy(gdf_rec_data->data, &num_columns, sizeof(uint32_t));
            gdf_rec.post(&pub, 0, NULL, true, timeout);
        }
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
        const int total_resources = getLuaInteger(L, 4, true, 1);
        const int timeout = getLuaInteger(L, 5, true, RequestFields::DEFAULT_TIMEOUT);

        // check if already received
        if(dataframe->receivePid)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "dataframe already received");
        }

        // kick off receive thread
        info = new receive_info_t(dataframe, inq_name, outq_name, total_resources, timeout);
        dataframe->receivePid = new Thread(receiveThread, info);

        // wait for ready signal
        // this guarantees the receive thread is ready to receive data
        // specifically, the subscriber to the message queue needs to be created so
        // that subsequent posts to the message queue are not dropped
        info->ready_signal.lock();
        {
            if(!info->ready)
            {
                if(info->ready_signal.wait(0, timeout * 1000))
                {
                    // success
                    status = info->ready;
                }
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
 * luaRun
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaRun  (lua_State* L)
{
    bool status = true;
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
                throw RunTimeException(CRITICAL, RTE_ERROR, "run queue post failed: %d", post_state);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error attaching runner: %s", e.what());
        if(runner) runner->releaseLuaObject();
        status = false;
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
