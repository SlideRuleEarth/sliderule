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

        /* Check Column Table */
        if(_columns == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "must supply table of column names");
        }

        /* Create Report Dispatch */
        return createLuaObject(L, new CsvDispatch(L, outq_name, _columns, _num_columns));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
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
    assert(_columns);

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
    int len = StringLib::size(hdrrow, MAX_STR_SIZE) + 1;
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
bool CsvDispatch::processRecord (RecordObject* record, okey_t key, recVec_t* records)
{
    (void)key;
    (void)records;

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
    int len = StringLib::size(valrow, MAX_STR_SIZE) + 1;
    int status = outQ->postCopy(valrow, len, SYS_TIMEOUT);

    /* Check and Return Status */
    return status > 0;
}
