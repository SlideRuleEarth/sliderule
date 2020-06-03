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

#include "Hdf5Handle.h"
#include "LuaObject.h"
#include "RecordObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Hdf5Handle::OBJECT_TYPE = "Hdf5Handle";

/******************************************************************************
 * HDF5 HANDLE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Hdf5Handle::Hdf5Handle (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table)
{
}

/*----------------------------------------------------------------------------
 * Pure Virtual Destructor
 *----------------------------------------------------------------------------*/
Hdf5Handle::~Hdf5Handle (void) {}

/******************************************************************************
 * HDF5 DATASET HANDLE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Static Data
 *----------------------------------------------------------------------------*/

const char* Hdf5DatasetHandle::datasetRecType = "Hdf5Dataset";
RecordObject::fieldDef_t Hdf5DatasetHandle::datasetRecDef[] = {
    {"ID",       INT64,  offsetof(dataset_t, id),       sizeof(((dataset_t*)0)->id),        NATIVE_FLAGS},
    {"OFFSET",  UINT32,  offsetof(dataset_t, offset),   sizeof(((dataset_t*)0)->offset),    NATIVE_FLAGS},
    {"SIZE",    UINT32,  offsetof(dataset_t, size),     sizeof(((dataset_t*)0)->size),      NATIVE_FLAGS}
};

const char* Hdf5DatasetHandle::LuaMetaName = "Hdf5DatasetHandle";
const struct luaL_Reg Hdf5DatasetHandle::LuaMetaTable[] = {
    {NULL,          NULL}
};

/*----------------------------------------------------------------------------
 * luaCreate - create(<dataset name>, [<chunk size>], [<id>])
 *----------------------------------------------------------------------------*/
int Hdf5DatasetHandle::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* dataset_name    = getLuaString(L, 1);
        long        chunk_size      = getLuaInteger(L, 2, true, INFINITE_CHUNK);
        long        id              = getLuaInteger(L, 3, true, 0);

        /* Return Dispatch Object */
        return createLuaObject(L, new Hdf5DatasetHandle(L, dataset_name, chunk_size, id));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Hdf5DatasetHandle::Hdf5DatasetHandle (lua_State* L, const char* dataset_name, long chunk_size, long id):
    Hdf5Handle(L, meta_name, meta_table)
{
    int def_elements = sizeof(datasetRecDef) / sizeof(RecordObject::fieldDef_t);
    RecordObject::defineRecord(datasetRecType, "ID", sizeof(dataset_t), datasetRecDef, def_elements, 8);

    LocalLib::set(&dataset, 0, sizeof(dataset));

    name = StringLib::duplicate(dataset_name);
    chunk = chunk_size;
    dataset.id = id;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Hdf5DatasetHandle::~Hdf5DatasetHandle (void) 
{
    if(name) delete [] name;
}

/*----------------------------------------------------------------------------
 * readRecord
 *----------------------------------------------------------------------------*/
bool Hdf5DatasetHandle::readRecord  (RecordObject** record, okey_t* key)
{
    (void)record;
    (void)key;

    return true;
}

/*----------------------------------------------------------------------------
 * writeRecord
 *----------------------------------------------------------------------------*/
bool Hdf5DatasetHandle::writeRecord (RecordObject* record, okey_t key)
{
    (void)record;
    (void)key;

    return true;
}
