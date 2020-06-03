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

const char* Hdf5DatasetHandle::recType = "Hdf5Dataset";
const RecordObject::fieldDef_t Hdf5DatasetHandle::recDef[] = {
    {"ID",      RecordObject::INT64,    offsetof(h5rec_t, id),      sizeof(((h5rec_t*)0)->id),      NATIVE_FLAGS},
    {"OFFSET",  RecordObject::UINT32,   offsetof(h5rec_t, offset),  sizeof(((h5rec_t*)0)->offset),  NATIVE_FLAGS},
    {"SIZE",    RecordObject::UINT32,   offsetof(h5rec_t, size),    sizeof(((h5rec_t*)0)->size),    NATIVE_FLAGS}
};

const char* Hdf5DatasetHandle::LuaMetaName = "Hdf5DatasetHandle";
const struct luaL_Reg Hdf5DatasetHandle::LuaMetaTable[] = {
    {NULL,          NULL}
};

/*----------------------------------------------------------------------------
 * luaCreate - create(<dataset name>, [<id>])
 *----------------------------------------------------------------------------*/
int Hdf5DatasetHandle::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* dataset_name    = getLuaString(L, 1);
        long        id              = getLuaInteger(L, 2, true, 0);

        /* Return Dispatch Object */
        return createLuaObject(L, new Hdf5DatasetHandle(L, dataset_name, id));
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
Hdf5DatasetHandle::Hdf5DatasetHandle (lua_State* L, const char* dataset_name, long id):
    Hdf5Handle(L, LuaMetaName, LuaMetaTable)
{
    int def_elements = sizeof(recDef) / sizeof(RecordObject::fieldDef_t);
    RecordObject::defineRecord(recType, "ID", sizeof(h5rec_t), recDef, def_elements, 8);

    LocalLib::set(&rec, 0, sizeof(rec));

    name = StringLib::duplicate(dataset_name);
    rec.id = id;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Hdf5DatasetHandle::~Hdf5DatasetHandle (void)
{
    if(name) delete [] name;
}

/*----------------------------------------------------------------------------
 * open
 *----------------------------------------------------------------------------*/
bool Hdf5DatasetHandle::open (const char* filename, DeviceObject::role_t role)
{
    unsigned flags;
    bool status = false;

    /* Set Flags */
    if(role == DeviceObject::READER)        flags = H5F_ACC_RDONLY;
    else if(role == DeviceObject::WRITER)   flags = H5F_ACC_TRUNC;
    else                                    flags = H5F_ACC_RDWR;

    /* Open File */
    hid_t file = H5Fopen(filename, flags, H5P_DEFAULT);
    hid_t dataset = H5Dopen(file, name, H5P_DEFAULT);
    hid_t space = H5Dget_space(dataset);

    /* Read Data */
    int ndims = H5Sget_simple_extent_ndims(space);
    if(ndims <= MAX_NDIMS)
    {
        hsize_t* dims = new hsize_t[ndims];
        H5Sget_simple_extent_dims(space, dims, NULL);

        size_t type_size = H5Tget_size(dataset);

        printf("The size and the dims are: %d, %ld\n", ndims, (long)type_size);
    }
    else
    {
        mlog(CRITICAL, "Number of dimensions exceeded maximum allowed: %d\n", ndims);
    }

    H5Fclose(file);

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
int Hdf5DatasetHandle::read (void* buf, int len)
{
    (void)buf;
    (void)len;

    return 0;
}

/*----------------------------------------------------------------------------
 * write
 *----------------------------------------------------------------------------*/
int Hdf5DatasetHandle::write (const void* buf, int len)
{
    (void)buf;
    (void)len;

    return 0;
}

/*----------------------------------------------------------------------------
 * close
 *----------------------------------------------------------------------------*/
void Hdf5DatasetHandle::close (void)
{
}
