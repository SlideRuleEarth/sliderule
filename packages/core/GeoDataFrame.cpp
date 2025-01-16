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
 * luaImport - import(<lua table>)
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaImport (lua_State* L)
{
    bool status = true;
    GeoDataFrame* dataframe = NULL;

    try
    {
        // create initial dataframe
        dataframe = new GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE, {}, {}, true);

        // build columns of dataframe
        lua_pushnil(L); // prime the stack for the next key
        while(lua_next(L, 1) != 0)
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
        dataframe->columnFields.fromLua(L, 1);

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

        // optionally build metadata
        if(lua_gettop(L) >= 2)
        {
            // build keys of metadata
            lua_pushnil(L); // prime the stack for the next key
            while(lua_next(L, 2) != 0)
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
            dataframe->metaFields.fromLua(L, 2);
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
 * luaReceive - core.rxdataframe(<msgq>, <rspq>)
 *----------------------------------------------------------------------------*/
template<class T>
void add_meta(GeoDataFrame* dataframe, GeoDataFrame::meta_rec_t* meta_rec_data)
{
    if(sizeof(T) == meta_rec_data->size)
    {
        T* value_ptr = reinterpret_cast<T*>(meta_rec_data->data);
        FieldElement<T>* data = new FieldElement<T>(*value_ptr);
        const char* _name = StringLib::duplicate(meta_rec_data->name);
        dataframe->addMetaData(_name, data);
    }
}

template<class T>
void add_column(GeoDataFrame* dataframe, GeoDataFrame::column_rec_t* column_rec_data)
{
    FieldColumn<T>* column = new FieldColumn<T>(column_rec_data->data, column_rec_data->size, column_rec_data->encoding);
    const char* _name = StringLib::duplicate(column_rec_data->name);
    if(!dataframe->addColumnData(_name, column))
    {
        delete column;
        delete [] _name;
        throw RunTimeException(CRITICAL, RTE_ERROR, "size mismatch, %ld != %u", dataframe->length(), column_rec_data->size);
    }
}

int GeoDataFrame::luaReceive(lua_State* L)
{
    bool status = true;
    Publisher* pubq = NULL;
    bool complete = false;
    long num_columns = 0;

    // create initial dataframe
    GeoDataFrame* dataframe = new GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE, {}, {}, true);
    lua_pop(L, 2); // creation of GeoDataFrame LuaObject leaves two objects on the stack

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
        while(!complete && status)
        {
            // check timeout
            if(TimeLib::latchtime() > timelimit)
            {
                alert(CRITICAL, RTE_ERROR, pubq, NULL, "timeout occurred receiving dataframe");
                status = false;
            }

            // receive message
            Subscriber::msgRef_t ref;
            const int recv_status = subq->receiveRef(ref, SYS_TIMEOUT);
            if(recv_status > 0)
            {
                // process terminator
                if(ref.size == 0)
                {
                    alert(CRITICAL, RTE_ERROR, pubq, NULL, "received unexpected terminator on <%s> before dataframe was complete", subq->getName());
                    complete = true;
                    status = false;
                }
                // process record
                else if(ref.size > 0)
                {
                    RecordInterface* record = new RecordInterface(reinterpret_cast<unsigned char*>(ref.data), ref.size);

                    if(StringLib::match(record->getRecordType(), metaRecType))
                    {
                        // meta record
                        meta_rec_t* meta_rec_data = reinterpret_cast<meta_rec_t*>(record->getRecordData());
                        const uint32_t encoding_mask = meta_rec_data->encoding & Field::VALUE_MASK;

                        // create meta field
                        switch(encoding_mask)
                        {
                            case BOOL:      add_meta<bool>    (dataframe, meta_rec_data); break;
                            case INT8:      add_meta<int8_t>  (dataframe, meta_rec_data); break;
                            case INT16:     add_meta<int16_t> (dataframe, meta_rec_data); break;
                            case INT32:     add_meta<int32_t> (dataframe, meta_rec_data); break;
                            case INT64:     add_meta<int64_t> (dataframe, meta_rec_data); break;
                            case UINT8:     add_meta<uint8_t> (dataframe, meta_rec_data); break;
                            case UINT16:    add_meta<uint16_t>(dataframe, meta_rec_data); break;
                            case UINT32:    add_meta<uint32_t>(dataframe, meta_rec_data); break;
                            case UINT64:    add_meta<uint64_t>(dataframe, meta_rec_data); break;
                            case FLOAT:     add_meta<float>   (dataframe, meta_rec_data); break;
                            case DOUBLE:    add_meta<double>  (dataframe, meta_rec_data); break;
                            case TIME8:     add_meta<time8_t> (dataframe, meta_rec_data); break;
                            // case STRING:    add_meta<string>  (dataframe, meta_rec_data); break; -- not yet supported
                            default:        throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported meta encoding %u", meta_rec_data->encoding);
                        }

                        // count column
                        num_columns++;
                    }
                    else if(StringLib::match(record->getRecordType(), columnRecType))
                    {
                        // column record
                        column_rec_t* column_rec_data = reinterpret_cast<column_rec_t*>(record->getRecordData());
                        const uint32_t encoding_mask = column_rec_data->encoding & Field::VALUE_MASK;

                        // set number of rows on first column
                        if(dataframe->numRows == 0)
                        {
                            dataframe->numRows = column_rec_data->num_rows;
                        }

                        // create column field
                        try
                        {
                            switch(encoding_mask)
                            {
                                case BOOL:      add_column<bool>    (dataframe, column_rec_data); break;
                                case INT8:      add_column<int8_t>  (dataframe, column_rec_data); break;
                                case INT16:     add_column<int16_t> (dataframe, column_rec_data); break;
                                case INT32:     add_column<int32_t> (dataframe, column_rec_data); break;
                                case INT64:     add_column<int64_t> (dataframe, column_rec_data); break;
                                case UINT8:     add_column<uint8_t> (dataframe, column_rec_data); break;
                                case UINT16:    add_column<uint16_t>(dataframe, column_rec_data); break;
                                case UINT32:    add_column<uint32_t>(dataframe, column_rec_data); break;
                                case UINT64:    add_column<uint64_t>(dataframe, column_rec_data); break;
                                case FLOAT:     add_column<float>   (dataframe, column_rec_data); break;
                                case DOUBLE:    add_column<double>  (dataframe, column_rec_data); break;
                                case TIME8:     add_column<time8_t> (dataframe, column_rec_data); break;
                                // case STRING:    add_column<string>  (dataframe, column_rec_data); break; -- not yet supported
                                default:        throw RunTimeException(CRITICAL, RTE_ERROR, "unsupported column encoding %u", column_rec_data->encoding);
                            }
                        }
                        catch(const RunTimeException& e)
                        {
                            alert(e.level(), RTE_ERROR, pubq, NULL, "unable to add column %s: %s", column_rec_data->name, e.what());
                            status = false;
                        }

                        // last column
                        if(column_rec_data->index == (num_columns - 1))
                        {
                            dataframe->populateGeoColumns();
                            complete = true;
                        }
                    }
                    else if(StringLib::match(record->getRecordType(), eofRecType))
                    {
                        // eof record
                        eof_rec_t* eof_rec_data = reinterpret_cast<eof_rec_t*>(record->getRecordData());
                        if(num_columns == eof_rec_data->num_columns)
                        {
                            if(dataframe->numRows == eof_rec_data->num_rows)
                            {
                                // complete dataframe
                                dataframe->populateGeoColumns();
                                complete = true;
                            }
                            else
                            {
                                alert(CRITICAL, RTE_ERROR, pubq, NULL, "invalid number of rows identified: %ld != %u", dataframe->numRows, eof_rec_data->num_rows);
                                status = false;
                            }
                        }
                        else
                        {
                            alert(CRITICAL, RTE_ERROR, pubq, NULL, "incomplete number of columns received: %ld != %u", num_columns, eof_rec_data->num_columns);
                            status = false;
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
                    status = false;
                }

                // dereference
                subq->dereference(ref);
            }
            else if(recv_status != MsgQ::STATE_TIMEOUT)
            {
                alert(CRITICAL, RTE_ERROR, pubq, NULL, "failed to receive records from queue <%s>: %d", subq->getName(), recv_status);
                status = false;
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
    pid(NULL),
    pubRunQ(NULL),
    subRunQ(pubRunQ),
    runComplete(false),
    freeOnDelete(free_on_delete)
{
    // set lua functions
    LuaEngine::setAttrFunc(L, "export",     luaExport);
    LuaEngine::setAttrFunc(L, "send",       luaSend);
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

        // create and send meta records
        Dictionary<FieldDictionary::entry_t>::Iterator meta_iter(dataframe->metaFields.fields);
        for(int i = 0; i < meta_iter.length; i++)
        {
            const Dictionary<FieldDictionary::entry_t>::kv_t kv = meta_iter[i];

            // determine size of element
            const uint32_t value_encoding = kv.value.field->getValueEncoding();
            assert(value_encoding >= 0 && value_encoding < RecordObject::NUM_FIELD_TYPES);
            const long element_size = kv.value.field->length() * RecordObject::FIELD_TYPE_BYTES[value_encoding];
            const long rec_size = offsetof(column_rec_t, data) + element_size;

            // create meta record
            RecordObject meta_rec(metaRecType, rec_size);
            meta_rec_t* meta_rec_data = reinterpret_cast<meta_rec_t*>(meta_rec.getRecordData());
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
        RecordObject eof_rec(metaRecType);
        eof_rec_t* eof_rec_data = reinterpret_cast<eof_rec_t*>(eof_rec.getRecordData());
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
