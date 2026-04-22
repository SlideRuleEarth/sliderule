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
#include "H5DataFrame.h"
#include "SystemConfig.h"
#include "GeoDataFrame.h"
#include "H5CoroLib.h"
#include "H5Object.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - H5DataFrame(<asset>, <resource>)
 *
 *  <filename> is the name of the HDF5 file to be read from or written to
 *----------------------------------------------------------------------------*/
int H5DataFrame::luaCreate(lua_State* L)
{
    H5Coro::Fields* _parms = NULL;
    H5Object* _h5obj = NULL;

    try
    {
        /* Get Parameters */
        _parms                  = dynamic_cast<H5Coro::Fields*>(getLuaObject(L, 1, H5Coro::Fields::OBJECT_TYPE));
        _h5obj                  = dynamic_cast<H5Object*>(getLuaObject(L, 2, H5Object::OBJECT_TYPE));
        const char* _group      = getLuaString(L, 3, true, NULL);
        const okey_t _df_key    = getLuaInteger(L, 4, true, 0);
        const long _timeout_ms  = getLuaInteger(L, 5, true, SystemConfig::settings().requestTimeoutSec.value * 1000);
        const char* time_column = getLuaString(L, 6, true, NULL);
        const char* x_column    = getLuaString(L, 7, true, NULL);
        const char* y_column    = getLuaString(L, 8, true, NULL);
        const char* z_column    = getLuaString(L, 9, true, NULL);

        /* Create and Return Object */
        return createLuaObject(L, new H5DataFrame(L, _parms, _h5obj, _group, _df_key, _timeout_ms, time_column, x_column, y_column, z_column));
    }
    catch(const RunTimeException& e)
    {
        if(_parms) _parms->releaseLuaObject();
        if(_h5obj) _h5obj->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5DataFrame::H5DataFrame (lua_State* L, H5Coro::Fields* _parms, H5Object* _h5obj, const char* _group, okey_t _df_key, long _timeout,
                          const char* time_column, const char* x_column, const char* y_column, const char* z_column):
    GeoDataFrame(L, LUA_META_NAME, LUA_META_TABLE, {}, {{"group", &group}}, _parms->crs.value.c_str(), _df_key),
    h5obj(_h5obj),
    data(_parms->variables, _h5obj, _group, _parms->col.value, _parms->startRow.value, _parms->numRows.value),
    group(_group, Field::META_SOURCE_ID),
    timeout(_timeout), // milliseconds
    timeColumn(time_column),
    xColumn(x_column),
    yColumn(y_column),
    zColumn(z_column)
{
    joinPid = new Thread(joinThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5DataFrame::~H5DataFrame (void)
{
    delete joinPid;
    h5obj->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * setGeoColumns
 *----------------------------------------------------------------------------*/
void H5DataFrame::setGeoColumns (void)
{
    try
    {
        // set time field
        if(timeColumn)
        {
            FieldUntypedColumn* time_field = columnFields.fields[timeColumn].field;
            if(time_field->type != COLUMN) throw RunTimeException(CRITICAL, RTE_FAILURE, "time field not a column");
            if(time_field->getValueEncoding() != TIME8) throw RunTimeException(CRITICAL, RTE_FAILURE, "time field not time8");
            time_field->setEncodingFlags(TIME_COLUMN);
        }

        // set x field
        if(xColumn)
        {
            FieldUntypedColumn* x_field = columnFields.fields[xColumn].field;
            if(x_field->type != COLUMN) throw RunTimeException(CRITICAL, RTE_FAILURE, "x field not a column");
            if(x_field->getValueEncoding() != DOUBLE) throw RunTimeException(CRITICAL, RTE_FAILURE, "x field not double");
            x_field->setEncodingFlags(X_COLUMN);
        }

        // set y field
        if(yColumn)
        {
            FieldUntypedColumn* y_field = columnFields.fields[yColumn].field;
            if(y_field->type != COLUMN) throw RunTimeException(CRITICAL, RTE_FAILURE, "y field not a column");
            if(y_field->getValueEncoding() != DOUBLE) throw RunTimeException(CRITICAL, RTE_FAILURE, "y field not double");
            y_field->setEncodingFlags(Y_COLUMN);
        }

        // set z field
        if(zColumn)
        {
            FieldUntypedColumn* z_field = columnFields.fields[zColumn].field;
            if(z_field->type != COLUMN) throw RunTimeException(CRITICAL, RTE_FAILURE, "z field not a column");
            if(z_field->getValueEncoding() != FLOAT) throw RunTimeException(CRITICAL, RTE_FAILURE, "z field not float");
            z_field->setEncodingFlags(Z_COLUMN);
        }

        // populate dataframe geo columns
        populateGeoColumns();
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to set geo columns: %s", e.what());
    }
}

/*----------------------------------------------------------------------------
 * joinThread
 *----------------------------------------------------------------------------*/
void* H5DataFrame::joinThread (void* parm)
{
    assert(parm);

    // get dataframe
    H5DataFrame* dataframe = static_cast<H5DataFrame*>(parm);

    try
    {
        // join datasets
        dataframe->data.joinToGDF(dataframe, dataframe->timeout, true);

        // add datasets
        dataframe->setNumRows(dataframe->data.addToGDF(dataframe));

        // set geo columns
        dataframe->setGeoColumns();
    }
    catch(const RunTimeException& e)
    {
        // handle error
        mlog(e.level(), "Failed to join h5 dataframe: %s", e.what());
        dataframe->inError = true;
    }

    // dataFrame finished
    dataframe->signalComplete();
    return NULL;
}
