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
 *      with removal: boolean of whether to remove duplicate rows or leave alone
 *----------------------------------------------------------------------------*/
int DeduplicateRunner::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new DeduplicateRunner(L));
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
DeduplicateRunner::DeduplicateRunner (lua_State* L):
    GeoDataFrame::FrameRunner(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
DeduplicateRunner::~DeduplicateRunner (void)
{
}

/*----------------------------------------------------------------------------
 * run
 *----------------------------------------------------------------------------*/
bool DeduplicateRunner::run (GeoDataFrame* dataframe)
{
    // check for empty dataframe
    if(dataframe->length() <= 0) return true;

    // get columns
    vector<string> column_names = dataframe->getColumnNames();
    vector<const FieldUntypedColumn*> columns;
    for(const string& column_name: column_names)
    {
        columns.push_back(&(*dataframe)[column_name.c_str()]);
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

    // initialize hash table
    std::unordered_set<string, Hasher, std::equal_to<void>> hash_table;

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
            printf("duplicate\n");
        }
        else
        {
            hash_table.emplace(key);
        }
    }

    // update runtime
    return true;
}
