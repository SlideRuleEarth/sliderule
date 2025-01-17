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

const char* GeoDataFrame::metaRecType = "geodataframe.meta";
const RecordObject::fieldDef_t GeoDataFrame::metaRecDef[] = {
    {"size",        RecordObject::UINT32,   offsetof(meta_rec_t, size),         1,              NULL, NATIVE_FLAGS},
    {"encoding",    RecordObject::UINT32,   offsetof(meta_rec_t, encoding),     1,              NULL, NATIVE_FLAGS},
    {"name",        RecordObject::STRING,   offsetof(column_rec_t, name),       MAX_NAME_SIZE,  NULL, NATIVE_FLAGS},
    {"data",        RecordObject::UINT8,    offsetof(column_rec_t, data),       0,              NULL, NATIVE_FLAGS},
};

const char* GeoDataFrame::columnRecType = "geodataframe.column";
const RecordObject::fieldDef_t GeoDataFrame::columnRecDef[] = {
    {"index",       RecordObject::UINT32,   offsetof(column_rec_t, index),      1,              NULL, NATIVE_FLAGS},
    {"size",        RecordObject::UINT32,   offsetof(column_rec_t, size),       1,              NULL, NATIVE_FLAGS},
    {"encoding",    RecordObject::UINT32,   offsetof(column_rec_t, encoding),   1,              NULL, NATIVE_FLAGS},
    {"num_rows",    RecordObject::UINT32,   offsetof(column_rec_t, num_rows),   1,              NULL, NATIVE_FLAGS},
    {"name",        RecordObject::STRING,   offsetof(column_rec_t, name),       MAX_NAME_SIZE,  NULL, NATIVE_FLAGS},
    {"data",        RecordObject::UINT8,    offsetof(column_rec_t, data),       0,              NULL, NATIVE_FLAGS},
};

const char* GeoDataFrame::eofRecType = "geodataframe.eof";
const RecordObject::fieldDef_t GeoDataFrame::eofRecDef[] = {
    {"num_rows",    RecordObject::UINT32,   offsetof(eof_rec_t, num_rows),      1,              NULL, NATIVE_FLAGS},
    {"num_columns", RecordObject::UINT32,   offsetof(eof_rec_t, num_columns),   1,              NULL, NATIVE_FLAGS},
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
        const long index = getLuaInteger(L, 2) - 1; // lua indexing starts at 1, convert to c indexing that starts at 0

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
    RECDEF(eofRecType,      eofRecDef,      sizeof(eof_rec_t),      NULL);
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
                const Field* field = dataframe->getColumnData(name.c_str());
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
                            const std::initializer_list<FieldDictionary::entry_t>& meta_list,
                            bool free_on_delete):
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
    receivePid(NULL),
    runPid(NULL),
    pubRunQ(NULL),
    subRunQ(pubRunQ),
    runComplete(false),
    freeOnDelete(free_on_delete)
{
    // set lua functions
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
 * runThread
 *----------------------------------------------------------------------------*/
/*
 * Local Helper Function: add_meta
 */
template<class T>
bool add_meta(GeoDataFrame* dataframe, GeoDataFrame::meta_rec_t* meta_rec_data)
{
    if(sizeof(T) == meta_rec_data->size)
    {
        T* value_ptr = reinterpret_cast<T*>(meta_rec_data->data);
        FieldElement<T>* data = new FieldElement<T>(*value_ptr);
        const char* _name = StringLib::duplicate(meta_rec_data->name);
        dataframe->addMetaData(_name, data);
        return true;
    }
    return false;
}
/*
 * Local Helper Function: add_column
 */
template<class T>
bool add_column(GeoDataFrame* dataframe, GeoDataFrame::column_rec_t* column_rec_data)
{
    FieldColumn<T>* column = new FieldColumn<T>(column_rec_data->data, column_rec_data->size, column_rec_data->encoding);
    const char* _name = StringLib::duplicate(column_rec_data->name);
    if(!dataframe->addColumnData(_name, column))
    {
        delete column;
        delete [] _name;
        return false;
    }
    return true;
}
/*
 * Class Method: receiveThread
 */
void* GeoDataFrame::receiveThread (void* parm)
{
    // cast parameter
    receive_info_t* info = static_cast<receive_info_t*>(parm);

    // initialize processing variables
    Subscriber inq(info->inq_name);
    Publisher outq(info->outq_name);
    const double timelimit = TimeLib::latchtime() + static_cast<double>(info->timeout);
    long num_columns = 0;
    bool complete = false;
    Subscriber::msgRef_t ref;

    // indicate thread is ready to receive
    info->ready_signal.lock();
    {
        info->ready = true;
        info->ready_signal.signal();
    }
    info->ready_signal.unlock();

    try
    {
        // while receiving messages
        while(info->dataframe->active && !complete)
        {
            // check timeout
            if(TimeLib::latchtime() > timelimit)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "timeout occurred receiving dataframe");
            }

            // receive message
            const int recv_status = inq.receiveRef(ref, SYS_TIMEOUT);

            // check for timeout
            if(recv_status == MsgQ::STATE_TIMEOUT)
            {
                continue;
            }

            // check for error in receiving
            if(recv_status < 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "failed to receive records from queue <%s>: %d", inq.getName(), recv_status);
            }

            // check for terminator
            if(ref.size == 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "received unexpected terminator on <%s> before dataframe was complete", inq.getName());
            }

            // check for invalid size
            if(ref.size < 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "received record of invalid size from queue <%s>: %d", inq.getName(), ref.size);
            }

            // process record
            const RecordInterface record(reinterpret_cast<unsigned char*>(ref.data), ref.size);
            if(StringLib::match(record.getRecordType(), metaRecType))
            {
                // meta record
                meta_rec_t* meta_rec_data = reinterpret_cast<meta_rec_t*>(record.getRecordData());
                const uint32_t encoding_mask = meta_rec_data->encoding & Field::VALUE_MASK;

                // create meta field
                bool add_status = false;
                switch(encoding_mask)
                {
                    case BOOL:      add_status = add_meta<bool>    (info->dataframe, meta_rec_data); break;
                    case INT8:      add_status = add_meta<int8_t>  (info->dataframe, meta_rec_data); break;
                    case INT16:     add_status = add_meta<int16_t> (info->dataframe, meta_rec_data); break;
                    case INT32:     add_status = add_meta<int32_t> (info->dataframe, meta_rec_data); break;
                    case INT64:     add_status = add_meta<int64_t> (info->dataframe, meta_rec_data); break;
                    case UINT8:     add_status = add_meta<uint8_t> (info->dataframe, meta_rec_data); break;
                    case UINT16:    add_status = add_meta<uint16_t>(info->dataframe, meta_rec_data); break;
                    case UINT32:    add_status = add_meta<uint32_t>(info->dataframe, meta_rec_data); break;
                    case UINT64:    add_status = add_meta<uint64_t>(info->dataframe, meta_rec_data); break;
                    case FLOAT:     add_status = add_meta<float>   (info->dataframe, meta_rec_data); break;
                    case DOUBLE:    add_status = add_meta<double>  (info->dataframe, meta_rec_data); break;
                    case TIME8:     add_status = add_meta<time8_t> (info->dataframe, meta_rec_data); break;
                    default:        break;
                }

                // check add status
                if(!add_status)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "failed to add metadata %s of size %u with encoding %u", meta_rec_data->name, meta_rec_data->size, meta_rec_data->encoding);
                }
            }
            else if(StringLib::match(record.getRecordType(), columnRecType))
            {
                // column record
                column_rec_t* column_rec_data = reinterpret_cast<column_rec_t*>(record.getRecordData());
                const uint32_t encoding_mask = column_rec_data->encoding & Field::VALUE_MASK;

                // set number of rows on first column
                if(info->dataframe->numRows == 0)
                {
                    info->dataframe->numRows = column_rec_data->num_rows;
                }

                // create column field
                bool add_status = false;
                switch(encoding_mask)
                {
                    case BOOL:      add_status = add_column<bool>    (info->dataframe, column_rec_data); break;
                    case INT8:      add_status = add_column<int8_t>  (info->dataframe, column_rec_data); break;
                    case INT16:     add_status = add_column<int16_t> (info->dataframe, column_rec_data); break;
                    case INT32:     add_status = add_column<int32_t> (info->dataframe, column_rec_data); break;
                    case INT64:     add_status = add_column<int64_t> (info->dataframe, column_rec_data); break;
                    case UINT8:     add_status = add_column<uint8_t> (info->dataframe, column_rec_data); break;
                    case UINT16:    add_status = add_column<uint16_t>(info->dataframe, column_rec_data); break;
                    case UINT32:    add_status = add_column<uint32_t>(info->dataframe, column_rec_data); break;
                    case UINT64:    add_status = add_column<uint64_t>(info->dataframe, column_rec_data); break;
                    case FLOAT:     add_status = add_column<float>   (info->dataframe, column_rec_data); break;
                    case DOUBLE:    add_status = add_column<double>  (info->dataframe, column_rec_data); break;
                    case TIME8:     add_status = add_column<time8_t> (info->dataframe, column_rec_data); break;
                    default:        break;
                }

                // check add status
                if(!add_status)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "failed to add column with %u rows and with encoding %u", column_rec_data->num_rows, column_rec_data->encoding);
                }

                // count column
                num_columns++;
            }
            else if(StringLib::match(record.getRecordType(), eofRecType))
            {
                // eof record
                eof_rec_t* eof_rec_data = reinterpret_cast<eof_rec_t*>(record.getRecordData());

                // check number of columns
                if(num_columns != eof_rec_data->num_columns)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "incomplete number of columns received: %ld != %u", num_columns, eof_rec_data->num_columns);
                }

                // check number of rows
                if(info->dataframe->numRows != eof_rec_data->num_rows)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "invalid number of rows identified: %ld != %u", info->dataframe->numRows, eof_rec_data->num_rows);
                }

                // complete dataframe
                info->dataframe->populateGeoColumns();
                complete = true;
            }
            else
            {
                // record of non-targeted type - pass through
                outq.postCopy(ref.data, ref.size);
            }

            // dereference
            inq.dereference(ref);
        }
    }
    catch(const RunTimeException& e)
    {
        inq.dereference(ref);
        alert(e.level(), RTE_ERROR, &outq, NULL, "Error receiving dataframe: %s", e.what());
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

        // create publisher
        Publisher pub(rspq);

        // create and send meta records
        Dictionary<FieldDictionary::entry_t>::Iterator meta_iter(dataframe->metaFields.fields);
        for(int i = 0; i < meta_iter.length; i++)
        {
            const Dictionary<FieldDictionary::entry_t>::kv_t kv = meta_iter[i];

            // determine size of element
            const uint32_t value_encoding = kv.value.field->getValueEncoding();
            if(value_encoding < 0 || value_encoding >= RecordObject::NUM_FIELD_TYPES) continue;
            const long element_size = kv.value.field->length() * RecordObject::FIELD_TYPE_BYTES[value_encoding];
            const long rec_size = offsetof(column_rec_t, data) + element_size;

            // create meta record
            RecordObject meta_rec(metaRecType, rec_size);
            meta_rec_t* meta_rec_data = reinterpret_cast<meta_rec_t*>(meta_rec.getRecordData());
            meta_rec_data->key = key_space;
            meta_rec_data->size = element_size;
            meta_rec_data->encoding = kv.value.field->encoding;
            StringLib::copy(meta_rec_data->name, kv.value.name, MAX_NAME_SIZE);

            // serialize column data into record
            const long bytes_serialized = kv.value.field->serialize(meta_rec_data->data, element_size);
            if(bytes_serialized != element_size) throw RunTimeException(CRITICAL, RTE_ERROR, "failed to serialize column %s: %ld", meta_rec_data->name, bytes_serialized);

            // send column record
            meta_rec.post(&pub, 0, NULL, true, timeout);
        }

        // create and send column records
        Dictionary<FieldDictionary::entry_t>::Iterator column_iter(dataframe->columnFields.fields);
        for(int i = 0; i < column_iter.length; i++)
        {
            const Dictionary<FieldDictionary::entry_t>::kv_t kv = column_iter[i];

            // determine size of column
            const uint32_t value_encoding = kv.value.field->getValueEncoding();
            assert(value_encoding >= 0 && value_encoding < RecordObject::NUM_FIELD_TYPES);
            const long column_size = kv.value.field->length() * RecordObject::FIELD_TYPE_BYTES[value_encoding];
            const long rec_size = offsetof(column_rec_t, data) + column_size;

            // create column record
            RecordObject column_rec(columnRecType, rec_size);
            column_rec_t* column_rec_data = reinterpret_cast<column_rec_t*>(column_rec.getRecordData());
            column_rec_data->key = key_space;
            column_rec_data->index = i;
            column_rec_data->size = column_size;
            column_rec_data->encoding = kv.value.field->encoding;
            column_rec_data->num_rows = kv.value.field->length();
            StringLib::copy(column_rec_data->name, kv.value.name, MAX_NAME_SIZE);

            // serialize column data into record
            const long bytes_serialized = kv.value.field->serialize(column_rec_data->data, column_size);
            if(bytes_serialized != column_size) throw RunTimeException(CRITICAL, RTE_ERROR, "failed to serialize column %s: %ld", column_rec_data->name, bytes_serialized);

            // send column record
            column_rec.post(&pub, 0, NULL, true, timeout);
        }

        // create and send eof record
        RecordObject eof_rec(eofRecType);
        eof_rec_t* eof_rec_data = reinterpret_cast<eof_rec_t*>(eof_rec.getRecordData());
        eof_rec_data->key = key_space;
        eof_rec_data->num_rows = dataframe->length();
        eof_rec_data->num_columns = dataframe->columnFields.length();
        eof_rec.post(&pub, 0, NULL, true, timeout);
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
 * luaReceive - :receive(<input q>, [<number of resources>], [<outq>], [<timeout>])
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
        const int total_resources = getLuaInteger(L, 3);
        const char* outq_name = getLuaString(L, 4);
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
