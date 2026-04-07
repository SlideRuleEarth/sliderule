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

#include <math.h>
#include <float.h>
#include <unordered_set>
#include <string_view>

#include "OsApi.h"
#include "GeoLib.h"
#include "DeduplicateRunner.h"

/******************************************************************************
 * DATA
 ******************************************************************************/

const char* DeduplicateRunner::LUA_META_NAME = "DeduplicateRunner";
const struct luaL_Reg DeduplicateRunner::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

struct Hasher // FNV-1a
{
    using is_transparent = void; // enables heterogeneous lookup

    size_t operator()(std::string_view v) const
    {
        uint64_t hash = 1469598103934665603ULL;
        for (unsigned char c : v) {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<msgq>, <column names>, <with removal>), where
 *      msgq:  post rows that are duplicate
 *      column names: list of columns to compare
 *      with removal: boolean of whether to remove duplicate rows or just report
 *----------------------------------------------------------------------------*/
int DeduplicateRunner::luaCreate (lua_State* L)
{
    try
    {
        // get parameters
        const char* qname = getLuaString(L, 1, true, NULL);
        const int column_names_index = 2;
        const bool with_removal = getLuaBoolean(L, 3, true, false);

        // get column names
        vector<string> column_names;
        if(lua_type(L, column_names_index) == LUA_TTABLE)
        {
            const int num_columns = lua_rawlen(L, column_names_index);
            for(int c = 0; c < num_columns; c++)
            {
                lua_rawgeti(L, column_names_index, c + 1);
                const char* name = getLuaString(L, -1);
                column_names.emplace_back(name);
                lua_pop(L, 1);
            }
        }
        else if(lua_type(L, column_names_index) == LUA_TSTRING)
        {
            const char* name = getLuaString(L, column_names_index);
            column_names.emplace_back(name);
        }

        // create lua object
        return createLuaObject(L, new DeduplicateRunner(L, qname, column_names, with_removal));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", OBJECT_TYPE, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
DeduplicateRunner::DeduplicateRunner (lua_State* L, const char* qname, const vector<string>& _column_names, bool _with_removal):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE),
    pubQ(NULL),
    columnNames(_column_names),
    withRemoval(_with_removal)
{
    if(qname)
    {
        pubQ = new Publisher(qname);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
DeduplicateRunner::~DeduplicateRunner (void)
{
    delete pubQ;
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool DeduplicateRunner::run (GeoDataFrame* dataframe)
{
    // check for empty dataframe
    if(dataframe->length() <= 0) return true;

    // get column names
    vector<string> column_names;
    if(columnNames.empty()) column_names = dataframe->getColumnNames();
    else column_names = columnNames;

    // get columns
    vector<FieldUntypedColumn*> columns;
    for(const string& column_name: column_names)
    {
        columns.push_back(dataframe->getUnsafe(column_name.c_str()));
    }

    // get size of row
    long row_size = 0;
    for(size_t col = 0; col < columns.size(); col++)
    {
        row_size += columns[col]->getTypeSize();
    }

    // check row size
    if(row_size <= 0)
    {
        mlog(CRITICAL, "invalid row size: %ld", row_size);
        return false;
    }

    // initialize state variables
    std::unordered_set<string, Hasher, std::equal_to<void>> hash_table;
    vector<long> rows_to_remove;

    // loop through each row and hash data
    uint8_t* hash_data = new uint8_t[row_size];
    for(long row = 0; row < dataframe->length(); row++)
    {
        // serialize data into hash buffer
        uint8_t* hash_data_ptr = hash_data;
        long hash_data_left = row_size;
        for(size_t col = 0; col < columns.size(); col++)
        {
            if(hash_data_left > 0)
            {
                const long bytes_serialized = columns[col]->raw(hash_data_ptr, hash_data_left, row);
                hash_data_ptr += bytes_serialized;
                hash_data_left -= bytes_serialized;
            }
            else // mismatched row size detected
            {
                mlog(CRITICAL, "insufficient space to serialize row %ld of size %ld", row, row_size);
                delete [] hash_data;
                return false;
            }
        }

        // check all data is serialized
        if(hash_data_left != 0)
        {
            mlog(CRITICAL, "failed to fully serialize row %ld of size %ld: %ld bytes remaining", row, row_size, hash_data_left);
            delete [] hash_data;
            return false;
        }

        // insert data into hash table
        std::string_view key(reinterpret_cast<const char*>(hash_data), row_size);
        if(hash_table.find(key) != hash_table.end())
        {
            if(pubQ)
            {
                // build duplicate record to post
                FieldDictionary duplicate({});
                for(size_t col = 0; col < columns.size(); col++)
                {
                    Field* element = columns[col]->row(row);
                    if(!duplicate.add(column_names[col].c_str(), element, true))
                    {
                        mlog(CRITICAL, "failed to add duplicate entry for %s row %ld", column_names[col].c_str(), row);
                        delete element;
                    }
                }
                pubQ->postString("%s", duplicate.toJson().c_str());
            }

            if(withRemoval)
            {
                // save off row to remove
                rows_to_remove.push_back(row);
            }
        }
        else
        {
            hash_table.emplace(key);
        }
    }

    // remove duplicate rows
    if(withRemoval)
    {
        vector<uint8_t> mask(dataframe->length(), 1);
        for(long row: rows_to_remove) mask[row] = 0;
        for(size_t col = 0; col < columns.size(); col++)
        {
            dataframe->setNumRows(columns[col]->filter(mask));
        }
    }

    // update runtime
    return true;
}
