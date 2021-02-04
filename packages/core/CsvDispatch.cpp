/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CsvDispatch.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CsvDispatch::LuaMetaName = "CsvDispatch";
const struct luaL_Reg CsvDispatch::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS - REPORT DISPATCH
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate
 *
 *   <field name table> <outq_name> 
 *----------------------------------------------------------------------------*/
int CsvDispatch::luaCreate (lua_State* L)
{

    try
    {
        /* Get Output Queue Name */
        int tblindex = 1;
        const char* outq_name = getLuaString(L, 2);

        /* Parse Header Columns */
        const char** _columns = NULL;
        int _num_columns = lua_rawlen(L, tblindex);
        if(lua_istable(L, tblindex) && _num_columns > 0)
        {
            _columns = new const char* [_num_columns];
            for(int i = 0; i < _num_columns; i++)
            {
                lua_rawgeti(L, tblindex, i+1);
                _columns[i] = StringLib::duplicate(getLuaString(L, -1));
            }
        }

        /* Create Report Dispatch */
        return createLuaObject(L, new CsvDispatch(L, outq_name, _columns, _num_columns));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 *******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 * 
 *  Note: object takes ownership of columns pointer and must free memory
 *----------------------------------------------------------------------------*/
CsvDispatch::CsvDispatch (lua_State* L, const char* outq_name, const char** _columns, int _num_columns):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    /* Initialize Members */
    outQ = new Publisher(outq_name);
    columns = _columns;
    num_columns = _num_columns;

    /* Build Header Row */
    char hdrrow[MAX_STR_SIZE];
    hdrrow[0] = '\0';
    for(int i = 0; i < num_columns; i++)
    {
        const char* fmtstr = "%s, ";
        if(i == (num_columns - 1)) fmtstr = "%s\n";
        
        char colstr[RecordObject::MAX_VAL_STR_SIZE];
        StringLib::format(colstr, RecordObject::MAX_VAL_STR_SIZE, fmtstr, columns[i]);
        StringLib::concat(hdrrow, colstr, MAX_STR_SIZE);
    }

    /* Send Out Header Row */
    int len = StringLib::size(hdrrow, MAX_STR_SIZE);
    outQ->postCopy(hdrrow, len, SYS_TIMEOUT);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CsvDispatch::~CsvDispatch (void)
{
    delete outQ;

    for(int i = 0; i < num_columns; i++)
    {
        delete [] columns[i];
    }
    delete [] columns;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool CsvDispatch::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    char valrow[MAX_STR_SIZE];

    /* Build Row */
    valrow[0] = '\0';
    for(int i = 0; i < num_columns; i++)
    {
        const char* fmtstr = "%s, ";
        if(i == (num_columns - 1)) fmtstr = "%s\n";

        char valstr[RecordObject::MAX_VAL_STR_SIZE];
        RecordObject::field_t field = record->getField(columns[i]);

        if(record->getValueText(field, valstr))
        {
            char colstr[RecordObject::MAX_VAL_STR_SIZE];
            StringLib::format(colstr, RecordObject::MAX_VAL_STR_SIZE, fmtstr, valstr);
            StringLib::concat(valrow, colstr, MAX_STR_SIZE);
        }
    }

    /* Send Out Row */
    int len = StringLib::size(valrow, MAX_STR_SIZE);
    int status = outQ->postCopy(valrow, len, SYS_TIMEOUT);

    /* Check and Return Status */
    return status > 0;
}
