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

#include <hdf5.h>

#include "Hdf5Handle.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Hdf5Handle::OBJECT_TYPE = "Hdf5Handle";

const char* Hdf5Handle::recType = "h5rec";
const RecordObject::fieldDef_t Hdf5Handle::recDef[] = {
    {"ID",      RecordObject::INT64,    offsetof(h5rec_t, id),      sizeof(((h5rec_t*)0)->id),      NATIVE_FLAGS},
    {"DATA",    RecordObject::STRING,   offsetof(h5rec_t, data),    sizeof(((h5rec_t*)0)->data),    NATIVE_FLAGS | RecordObject::POINTER},
    {"OFFSET",  RecordObject::UINT32,   offsetof(h5rec_t, offset),  sizeof(((h5rec_t*)0)->offset),  NATIVE_FLAGS},
    {"SIZE",    RecordObject::UINT32,   offsetof(h5rec_t, size),    sizeof(((h5rec_t*)0)->size),    NATIVE_FLAGS}
};

/******************************************************************************
 * HDF5 HANDLE METHODS (PURE VIRTUAL)
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Hdf5Handle::Hdf5Handle (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table)
{
    int def_elements = sizeof(recDef) / sizeof(RecordObject::fieldDef_t);
    RecordObject::defineRecord(recType, "ID", sizeof(h5rec_t), recDef, def_elements, 8);

    recObj = new RecordObject(recType);
    recData = (h5rec_t*)recObj->getRecordData();
    recData->data = sizeof(h5rec_t);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Hdf5Handle::~Hdf5Handle (void)
{
    delete recObj;
}
