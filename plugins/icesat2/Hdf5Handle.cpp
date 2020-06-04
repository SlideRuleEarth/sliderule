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
 * HDF5 HANDLE METHODS (PURE VIRTUAL)
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Hdf5Handle::Hdf5Handle (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table)
{
}

/*----------------------------------------------------------------------------
 * Destructor
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

    /* Initialize Attributes to Zero */
    handle = INVALID_RC;
    LocalLib::set(&rec, 0, sizeof(rec));
    dataBuffer = NULL;
    dataSize = 0;
    dataOffset = 0;

    /* Set Name of Dataset */
    dataName = StringLib::duplicate(dataset_name);

    /* Set ID of Dataset Record */
    rec.id = id;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Hdf5DatasetHandle::~Hdf5DatasetHandle (void)
{
    if(dataName) delete [] dataName;
}

/*----------------------------------------------------------------------------
 * open
 *----------------------------------------------------------------------------*/
bool Hdf5DatasetHandle::open (const char* filename, DeviceObject::role_t role)
{
    unsigned flags;
    bool status = false;
    hid_t file = INVALID_RC;
    hid_t dataset = INVALID_RC;
    hid_t space = INVALID_RC;
    hid_t datatype = INVALID_RC;

    /* Check Reentry */
    if(dataBuffer)
    {
        mlog(CRITICAL, "Dataset already opened: %s\n", dataName);
        return false;
    }

    /* Set Flags */
    if(role == DeviceObject::READER)        flags = H5F_ACC_RDONLY;
    else if(role == DeviceObject::WRITER)   flags = H5F_ACC_TRUNC;
    else                                    flags = H5F_ACC_RDWR;

    do
    {
        /* Open File */
        mlog(INFO, "Opening file: %s\n", filename);
        file = H5Fopen(filename, flags, H5P_DEFAULT);
        if(file < 0)
        {
            mlog(CRITICAL, "Failed to open file: %s\n", filename);
            break;
        }

        /* Open Dataset */
        dataset = H5Dopen(file, dataName, H5P_DEFAULT);
        if(dataset < 0)
        {
            mlog(CRITICAL, "Failed to open dataset: %s\n", dataName);
            break;
        }

        /* Open Dataspace */
        space = H5Dget_space(dataset);
        if(space < 0)
        {
            mlog(CRITICAL, "Failed to open dataspace on dataset: %s\n", dataName);
            break;
        }

        /* Get Datatype */
        datatype = H5Dget_type(dataset);
        size_t typesize = H5Tget_size(datatype);

        /* Read Data */
        int ndims = H5Sget_simple_extent_ndims(space);
        if(ndims <= MAX_NDIMS)
        {
            hsize_t* dims = new hsize_t[ndims];
            H5Sget_simple_extent_dims(space, dims, NULL);

            /* Get Size of Data Buffer */
            dataSize = typesize;
            for(int d = 0; d < ndims; d++)
            {
                dataSize *= dims[d];
            }

            /* Allocate Data Buffer */
            try
            {
                dataBuffer = new uint8_t[dataSize];
            }
            catch (const std::bad_alloc& e)
            {
                mlog(CRITICAL, "Failed to allocate space for dataset: %d\n", dataSize);
                break;
            }

            /* Read Dataset */
            mlog(INFO, "Reading %d bytes of data from %s\n", dataSize, dataName);
            if(H5Dread(dataset, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, dataBuffer) >= 0)
            {
                status = true;
            }
            else
            {
                mlog(CRITICAL, "Failed to read data from %s\n", dataName);
                break;
            }
        }
        else
        {
            mlog(CRITICAL, "Number of dimensions exceeded maximum allowed: %d\n", ndims);
            break;
        }
    }
    while(false);

    /* Clean Up */
    if(datatype > 0) H5Tclose(datatype);
    if(space > 0) H5Sclose(space);
    if(dataset > 0) H5Dclose(dataset);
    if(file > 0) H5Fclose(file);

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * read
 *----------------------------------------------------------------------------*/
int Hdf5DatasetHandle::read (void* buf, int len)
{
    int bytes_remaining = dataSize - dataOffset;
    int bytes_to_copy = MIN(len, bytes_remaining);
    LocalLib::copy(buf, &dataBuffer[dataOffset], bytes_to_copy);
    dataOffset += bytes_to_copy;
    return bytes_to_copy;
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
    if(dataBuffer) delete [] dataBuffer;
    dataBuffer = NULL;
}
