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

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoDataFrame::OBJECT_TYPE = "GeoDataFrame";
const char* GeoDataFrame::GDF = "gdf";
const char* GeoDataFrame::META = "meta";

const char* GeoDataFrame::FrameColumn::OBJECT_TYPE = "FrameColumn";
const char* GeoDataFrame::FrameColumn::LUA_META_NAME = "FrameColumn";
const struct luaL_Reg GeoDataFrame::FrameColumn::LUA_META_TABLE[] = {
    {"__index", luaGetData},
    {NULL,      NULL}
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
 * luaExport - export() --> lua table 
 *----------------------------------------------------------------------------*/
int GeoDataFrame::luaExport (lua_State* L)
{
    try
    {
        GeoDataFrame* lua_obj = dynamic_cast<GeoDataFrame*>(getLuaSelf(L, 1));
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
int GeoDataFrame::luaGetMetaData  (lua_State* L)
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
 * length
 *----------------------------------------------------------------------------*/
long GeoDataFrame::length(void) const
{
    return static_cast<long>(indexColumn.size());
}

/*----------------------------------------------------------------------------
 * addRow
 *----------------------------------------------------------------------------*/
long GeoDataFrame::addRow(void)
{
    // update index
    const long len = static_cast<long>(indexColumn.size());
    indexColumn.push_back(len);

    // return size of dataframe
    return len + 1;
}

/*----------------------------------------------------------------------------
 * addColumnData
 *----------------------------------------------------------------------------*/
void GeoDataFrame::addColumnData (const char* name, Field* column)
{
    const FieldDictionary::entry_t entry = {name, column};
    columnFields.add(entry);
}

/*----------------------------------------------------------------------------
 * getColumnNames
 *----------------------------------------------------------------------------*/
vector<string> GeoDataFrame::getColumnNames(void) const
{
    vector<string> names;
    char** keys = NULL;
    int num_keys = columnFields.fields.getKeys(&keys);
    for(int i = 0; i < num_keys; i++)
    {
        names.emplace_back(string(keys[i]));
        delete [] keys[i];
    }
    delete [] keys;
    return names;
}

/*----------------------------------------------------------------------------
 * getColumnData
 *----------------------------------------------------------------------------*/
Field* GeoDataFrame::getColumnData (const char* name) const
{
    const FieldDictionary::entry_t entry = columnFields.fields[name];
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
Field* GeoDataFrame::getMetaData (const char* name)
{
    const FieldDictionary::entry_t entry = metaFields.fields[name];
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
    columnFields (column_list),
    metaFields (meta_list),
    timeColumn(NULL),
    xColumn(NULL),
    yColumn(NULL),
    zColumn(NULL)
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

    // set lua functions
    LuaEngine::setAttrFunc(L, "export",     luaExport);
    LuaEngine::setAttrFunc(L, "import",     luaImport);
    LuaEngine::setAttrFunc(L, "__index",    luaGetColumnData);
    LuaEngine::setAttrFunc(L, "meta",       luaGetMetaData);
    
    // initialized
    initialized = true;
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
        
        provided = true; // even if no element within table are set, presence of table is sufficient
        initialized = true;
    }
}
