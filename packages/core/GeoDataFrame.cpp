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

const char* GeoDataFrame::metaRecType = "geodataframe.meta"; // extended elevation measurement record
const RecordObject::fieldDef_t GeoDataFrame::metaRecDef[] = {
    {"num_rows",            RecordObject::UINT64,   offsetof(meta_rec_t, num_rows),             1,                      NULL, NATIVE_FLAGS},
    {"num_columns",         RecordObject::UINT32,   offsetof(meta_rec_t, num_columns),          1,                      NULL, NATIVE_FLAGS},
    {"time_column_index",   RecordObject::UINT32,   offsetof(meta_rec_t, time_column_index),    1,                      NULL, NATIVE_FLAGS},
    {"x_column_index",      RecordObject::UINT32,   offsetof(meta_rec_t, x_column_index),       1,                      NULL, NATIVE_FLAGS},
    {"y_column_index",      RecordObject::UINT32,   offsetof(meta_rec_t, y_column_index),       1,                      NULL, NATIVE_FLAGS},
    {"z_column_index",      RecordObject::UINT32,   offsetof(meta_rec_t, z_column_index),       1,                      NULL, NATIVE_FLAGS},
};

const char* GeoDataFrame::columnRecType = "geodataframe.column";
const RecordObject::fieldDef_t GeoDataFrame::columnRecDef[] = {
    {"index",               RecordObject::UINT32,   offsetof(column_rec_t, index),              1,                      NULL, NATIVE_FLAGS},
    {"size",                RecordObject::UINT32,   offsetof(column_rec_t, size),               1,                      NULL, NATIVE_FLAGS},
    {"encoding",            RecordObject::UINT32,   offsetof(column_rec_t, encoding),           1,                      NULL, NATIVE_FLAGS},
    {"name",                RecordObject::STRING,   offsetof(column_rec_t, name),               MAX_COLUMN_NAME_SIZE,   NULL, NATIVE_FLAGS},
    {"data",                RecordObject::UINT8,    offsetof(column_rec_t, data),               0,                      NULL, NATIVE_FLAGS},
};

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor - FrameColumn
 *----------------------------------------------------------------------------*/
GeoDataFrame::FrameColumn::FrameColumn(lua_State* L, const Field& _column):
    LuaObject (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    column(_column)
{
}

/*----------------------------------------------------------------------------
 * luaGetData - [<index>]
 *----------------------------------------------------------------------------*/
int GeoDataFrame::FrameColumn::luaGetData (lua_State* L)
{
    try
    {
        GeoDataFrame::FrameColumn* lua_obj = dynamic_cast<GeoDataFrame::FrameColumn*>(getLuaSelf(L, 1));
        const long index = getLuaInteger(L, 2);

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
        mlog(e.level(), "Error exporting %s: %s", OBJECT_TYPE, e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void GeoDataFrame::init(void)
{
    RECDEF(metaRecType,     metaRecDef,     sizeof(meta_rec_t),     NULL);
    RECDEF(columnRecType,   columnRecDef,   sizeof(column_rec_t),   NULL);
}

/*----------------------------------------------------------------------------
 * luaReceive - core.rxdataframe(<msgq>, <rspq>)
 *----------------------------------------------------------------------------*/
template<class T>
void add_column(GeoDataFrame* dataframe, GeoDataFrame::column_rec_t* column_rec_data, uint32_t encoding_mask)
{
    FieldColumn<T>* column = new FieldColumn<T>(column_rec_data->data, column_rec_data->size, encoding_mask);
    if(!dataframe->addColumnData(column_rec_data->name, column))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "failed to add column %s of size %u", column_rec_data->name, column_rec_data->size);
    }
}

int GeoDataFrame::luaReceive(lua_State* L)
{
    bool status = true;
    Publisher* pubq = NULL;
    bool complete = false;
    bool terminator_received = false;

    long num_columns = 0;
    uint32_t time_index = INVALID_INDEX;
    uint32_t x_index = INVALID_INDEX;
    uint32_t y_index = INVALID_INDEX;
    uint32_t z_index = INVALID_INDEX;

    // create initial dataframe
    GeoDataFrame* dataframe = new GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE, {}, {});
    dataframe->freeOnDelete = true;

    try
    {
        // parameter #1 - get subscriber (userdata)
        LuaLibraryMsg::msgSubscriberData_t* msg_data = static_cast<LuaLibraryMsg::msgSubscriberData_t*>(luaL_checkudata(L, 1, LuaLibraryMsg::LUA_SUBMETANAME));
        if(!msg_data) throw RunTimeException(CRITICAL, RTE_ERROR, "Must supply subscriber for parameter #1");
        Subscriber* subq = msg_data->sub;

        // parameter #2 - get output message queue (string)
        const char* outq_name = getLuaString(L, 2, true, NULL);
        if(outq_name) pubq = new Publisher(outq_name);

        // parameter #3 - get request timeout
        const long timeout = getLuaInteger(L, 3, true, RequestFields::DEFAULT_TIMEOUT);
        const double timelimit = TimeLib::latchtime() + static_cast<double>(timeout);

        // while receiving messages
        while(!terminator_received)
        {
            // check timeout
            if(TimeLib::latchtime() > timelimit)
            {
                alert(CRITICAL, RTE_ERROR, pubq, NULL, "timeout occurred receiving dataframe");
            }

            // receive message
            Subscriber::msgRef_t ref;
            const int recv_status = subq->receiveRef(ref, SYS_TIMEOUT);
            if(recv_status > 0)
            {
                // process terminator
                if(ref.size == 0)
                {
                    terminator_received = true;
                    if(!complete)
                    {
                        alert(CRITICAL, RTE_ERROR, pubq, NULL, "received terminator on <%s> before dataframe was complete", subq->getName());
                    }
                }
                // process record
                else if(ref.size > 0)
                {
                    RecordInterface* record = new RecordInterface(reinterpret_cast<unsigned char*>(ref.data), ref.size);

                    if(StringLib::match(record->getRecordType(), metaRecType))
                    {
                        // meta record
                        meta_rec_t* meta_rec_data = reinterpret_cast<meta_rec_t*>(record->getRecordData());
                        dataframe->numRows = meta_rec_data->num_rows;
                        time_index = meta_rec_data->time_column_index;
                        x_index = meta_rec_data->x_column_index;
                        y_index = meta_rec_data->y_column_index;
                        z_index = meta_rec_data->z_column_index;
                        num_columns = meta_rec_data->num_columns;
                    }
                    else if(StringLib::match(record->getRecordType(), columnRecType))
                    {
                        // column record
                        column_rec_t* column_rec_data = reinterpret_cast<column_rec_t*>(record->getRecordData());

                        // create encoding mask
                        uint32_t encoding_mask = 0;
                        if(column_rec_data->index == time_index) encoding_mask |= TIME_COLUMN;
                        if(column_rec_data->index == x_index) encoding_mask |= X_COLUMN;
                        if(column_rec_data->index == y_index) encoding_mask |= Y_COLUMN;
                        if(column_rec_data->index == z_index) encoding_mask |= Z_COLUMN;

                        // create field column
                        switch(column_rec_data->encoding)
                        {
                            case BOOL:      add_column<bool>    (dataframe, column_rec_data, encoding_mask); break;
                            case INT8:      add_column<int8_t>  (dataframe, column_rec_data, encoding_mask); break;
                            case INT16:     add_column<int16_t> (dataframe, column_rec_data, encoding_mask); break;
                            case INT32:     add_column<int32_t> (dataframe, column_rec_data, encoding_mask); break;
                            case INT64:     add_column<int64_t> (dataframe, column_rec_data, encoding_mask); break;
                            case UINT8:     add_column<uint8_t> (dataframe, column_rec_data, encoding_mask); break;
                            case UINT16:    add_column<uint16_t>(dataframe, column_rec_data, encoding_mask); break;
                            case UINT32:    add_column<uint32_t>(dataframe, column_rec_data, encoding_mask); break;
                            case UINT64:    add_column<uint64_t>(dataframe, column_rec_data, encoding_mask); break;
                            case FLOAT:     add_column<float>   (dataframe, column_rec_data, encoding_mask); break;
                            case DOUBLE:    add_column<double>  (dataframe, column_rec_data, encoding_mask); break;
                            case TIME8:     add_column<time8_t> (dataframe, column_rec_data, encoding_mask); break;
                            // case STRING:    add_column<string>  (dataframe, column_rec_data, encoding_mask); break; -- not yet supported
                            default:        alert(CRITICAL, RTE_ERROR, pubq, NULL, "unable to add column %s with encoding: %u", column_rec_data->name, column_rec_data->encoding);
                        }

                        // last column
                        if(column_rec_data->index == (num_columns - 1))
                        {
                            dataframe->populateGeoColumns();
                            complete = true;
                        }
                    }
                    else if(pubq)
                    {
                        // record of non-targeted type - pass through
                        pubq->postCopy(ref.data, ref.size);
                    }

                    // free record
                    delete record;
                }
                else
                {
                    alert(CRITICAL, RTE_ERROR, pubq, NULL, "received record of invalid size from queue <%s>: %d", subq->getName(), ref.size);
                }
            }
            else if(recv_status != MsgQ::STATE_TIMEOUT)
            {
                alert(CRITICAL, RTE_ERROR, pubq, NULL, "failed to receive records from queue <%s>: %d", subq->getName(), recv_status);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error receiving dataframe: %s", e.what());
        status = false;
    }

    /* Cleanup */
    delete pubq;

    /* Return Dataframe LuaObject */
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
 * addColumnData
 *----------------------------------------------------------------------------*/
bool GeoDataFrame::addColumnData (const char* name, Field* column)
{
    if(column->length() == numRows)
    {
        const FieldDictionary::entry_t entry = {name, column};
        columnFields.add(entry);
        return true;
    }
    return false;
}

/*----------------------------------------------------------------------------
 * getColumnData
 *----------------------------------------------------------------------------*/
Field* GeoDataFrame::getColumnData (const char* name, Field::type_t _type) const
{
    const FieldDictionary::entry_t entry = columnFields.fields[name];
    if(!entry.field) throw RunTimeException(CRITICAL, RTE_ERROR, "%s field is null", name);
    if(_type != Field::FIELD && _type != entry.field->type) throw RunTimeException(CRITICAL, RTE_ERROR, "%s is incorrect type: %d", name, static_cast<int>(entry.field->type));
    return entry.field;
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
Field* GeoDataFrame::getMetaData (const char* name, Field::type_t _type)
{
    const FieldDictionary::entry_t entry = metaFields.fields[name];
    if(!entry.field) throw RunTimeException(CRITICAL, RTE_ERROR, "%s field is null", name);
    if(_type != entry.field->type) throw RunTimeException(CRITICAL, RTE_ERROR, "%s is incorrect type: %d", name, static_cast<int>(entry.field->type));
    return entry.field;
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
                            const std::initializer_list<FieldDictionary::entry_t>& meta_list):
    LuaObject (L, OBJECT_TYPE, meta_name, meta_table),
    Field(DATAFRAME, 0),
    numRows(0),
    columnFields(column_list),
    metaFields(meta_list),
    timeColumn(NULL),
    xColumn(NULL),
    yColumn(NULL),
    zColumn(NULL),
    active(true),
    pid(NULL),
    pubRunQ(NULL),
    subRunQ(pubRunQ),
    runComplete(false),
    freeOnDelete(false)
{
    // set lua functions
    LuaEngine::setAttrFunc(L, "export",     luaExport);
    LuaEngine::setAttrFunc(L, "import",     luaImport);
    LuaEngine::setAttrFunc(L, "__index",    luaGetColumnData);
    LuaEngine::setAttrFunc(L, "meta",       luaGetMetaData);
    LuaEngine::setAttrFunc(L, "run",        luaRun);
    LuaEngine::setAttrFunc(L, "finished",   luaRunComplete);

    // start runner
    pid = new Thread(runThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoDataFrame::~GeoDataFrame(void)
{
    active = false;
    delete pid;

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
 * luaImport - import(<lua table>)
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaImport (lua_State* L)
{
    bool status = true;
    try
    {
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        lua_obj->fromLua(L, 2);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error importing %s: %s", OBJECT_TYPE, e.what());
        status = false;
    }

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
        const char* column_name = getLuaString(L, 2);

        // check the metatable for the key (to support functions)
        luaL_getmetatable(L, lua_obj->LuaMetaName);
        lua_pushstring(L, column_name);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1))  return 1;
        else lua_pop(L, 1);

        // handle column access
        const Field& column_field = lua_obj->columnFields[column_name];
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

/*----------------------------------------------------------------------------
 * luaSend - :send(<rspq>, [<timeout>])
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaSend(lua_State* L)
{
    bool status = true;

    try
    {
        // get parameters
        GeoDataFrame*   dataframe   = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
        const char*     rspq        = getLuaString(L, 2);
        const int       timeout     = getLuaInteger(L, 3, true, SYS_TIMEOUT);

        // create publisher
        Publisher pub(rspq);

        // create column field iterator
        Dictionary<FieldDictionary::entry_t>::Iterator iter(dataframe->columnFields.fields);

        // get geo-column indices
        uint32_t time_column_index = INVALID_INDEX;
        uint32_t x_column_index = INVALID_INDEX;
        uint32_t y_column_index = INVALID_INDEX;
        uint32_t z_column_index = INVALID_INDEX;
        for(int i = 0; i < iter.length; i++)
        {
            const Dictionary<FieldDictionary::entry_t>::kv_t kv = iter[i];
            if(kv.value.field->encoding & TIME_COLUMN) time_column_index = i;
            if(kv.value.field->encoding & X_COLUMN) x_column_index = i;
            if(kv.value.field->encoding & Y_COLUMN) y_column_index = i;
            if(kv.value.field->encoding & Z_COLUMN) z_column_index = i;
        }

        // create and send meta record
        RecordObject meta_rec(metaRecType);
        meta_rec_t* meta_rec_data = reinterpret_cast<meta_rec_t*>(meta_rec.getRecordData());
        meta_rec_data->num_rows = dataframe->length();
        meta_rec_data->num_columns = dataframe->columnFields.length();
        meta_rec_data->time_column_index = time_column_index;
        meta_rec_data->x_column_index = x_column_index;
        meta_rec_data->y_column_index = y_column_index;
        meta_rec_data->z_column_index = z_column_index;
        meta_rec.post(&pub, 0, NULL, true, timeout);

        // create and send column records
        for(int i = 0; i < iter.length; i++)
        {
            const Dictionary<FieldDictionary::entry_t>::kv_t kv = iter[i];

            // determine size of column
            const uint32_t encoding = kv.value.field->getValueEncoding();
            assert(encoding >= 0 && encoding < RecordObject::NUM_FIELD_TYPES);
            const long column_size = kv.value.field->length() * RecordObject::FIELD_TYPE_BYTES[encoding];
            const long rec_size = offsetof(column_rec_t, data) + column_size;

            // create column record
            RecordObject column_rec(columnRecType, rec_size);
            column_rec_t* column_rec_data = reinterpret_cast<column_rec_t*>(column_rec.getRecordData());
            column_rec_data->index = i;
            column_rec_data->size = column_size;
            column_rec_data->encoding = encoding;
            StringLib::copy(column_rec_data->name, kv.value.name, MAX_COLUMN_NAME_SIZE);

            // serialize column data into record
            const long bytes_serialized = kv.value.field->serialize(column_rec_data->data, column_size);
            if(bytes_serialized != column_size) throw RunTimeException(CRITICAL, RTE_ERROR, "failed to serialize column %s", column_rec_data->name);

            // send column record
            column_rec.post(&pub, 0, NULL, true, timeout);
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
