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

#include "Hdf5DatasetDevice.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Hdf5DatasetDevice::recType = "h5dataset";
const RecordObject::fieldDef_t Hdf5DatasetDevice::recDef[] = {
    {"ID",      RecordObject::INT64,    offsetof(h5dataset_t, id),      1,  NULL, NATIVE_FLAGS},
    {"DATASET", RecordObject::STRING,   offsetof(h5dataset_t, dataset), 1,  NULL, NATIVE_FLAGS | RecordObject::POINTER},
    {"DATATYPE",RecordObject::UINT32,   offsetof(h5dataset_t, datatype),1,  NULL, NATIVE_FLAGS},
    {"OFFSET",  RecordObject::UINT32,   offsetof(h5dataset_t, offset),  1,  NULL, NATIVE_FLAGS},
    {"SIZE",    RecordObject::UINT32,   offsetof(h5dataset_t, size),    1,  NULL, NATIVE_FLAGS},
    {"DATA",    RecordObject::UINT8,    sizeof(h5dataset_t),            0,  NULL, NATIVE_FLAGS}
};

/******************************************************************************
 * HDF5 DATASET HANDLE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<role>, <filename>, <dataset name>, [<id>], [<raw>], [<datatype>])
 *----------------------------------------------------------------------------*/
int Hdf5DatasetDevice::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        int         _role           = (int)getLuaInteger(L, 1);
        const char* filename        = getLuaString(L, 2);
        const char* dataset_name    = getLuaString(L, 3);
        long        id              = getLuaInteger(L, 4, true, 0);
        bool        raw_mode        = getLuaBoolean(L, 5, true, true);
        RecordObject::valType_t datatype = (RecordObject::valType_t)getLuaInteger(L, 6, true, RecordObject::INVALID_VALUE);

        /* Check Access Type */
        if(_role != DeviceObject::READER && _role != DeviceObject::WRITER)
        {
            throw LuaException("unrecognized file access specified: %d\n", _role);
        }

        /* Return Dispatch Object */
        return createLuaObject(L, new Hdf5DatasetDevice(L, (role_t)_role, filename, dataset_name, id, raw_mode, datatype));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating Hdf5DatasetDevice: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Hdf5DatasetDevice::init (void)
{
    int def_elements = sizeof(recDef) / sizeof(RecordObject::fieldDef_t);
    RecordObject::recordDefErr_t rc = RecordObject::defineRecord(recType, "ID", sizeof(h5dataset_t), recDef, def_elements, 8);
    if(rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d\n", recType, rc);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Hdf5DatasetDevice::Hdf5DatasetDevice (lua_State* L, role_t _role, const char* filename, const char* dataset_name, long id, bool raw_mode, RecordObject::valType_t datatype):
    DeviceObject(L, _role)
{
    /* Set Record */
    recObj = new RecordObject(recType);
    recData = (h5dataset_t*)recObj->getRecordData();
    recData->dataset = sizeof(h5dataset_t);
    recData->datatype = (uint32_t)datatype;

    /* Initialize Attributes to Zero */
    dataBuffer = NULL;
    dataSize = 0;
    dataOffset = 0;

    /* Set Attributes */
    rawMode = raw_mode;
    fileName = StringLib::duplicate(filename);
    dataName = StringLib::duplicate(dataset_name);

    /* Set ID of Dataset Record */
    recData->id = id;

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s (%s)", fileName, role == READER ? "READER" : "WRITER") + 1;
    config = new char[cfglen];
    sprintf(config, "%s (%s)", fileName, role == READER ? "READER" : "WRITER");

    /* Open File */
    connected = h5open();
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Hdf5DatasetDevice::~Hdf5DatasetDevice (void)
{
    closeConnection();
    if(config) delete [] config;
    if(dataName) delete [] dataName;
    if(fileName) delete [] fileName;
}

/*----------------------------------------------------------------------------
 * h5open
 *----------------------------------------------------------------------------*/
bool Hdf5DatasetDevice::h5open (void)
{
    unsigned flags;
    bool status = false;
    hid_t file = INVALID_RC;
    hid_t dataset = INVALID_RC;
    hid_t space = INVALID_RC;
    hid_t datatype = INVALID_RC;
    bool datatype_allocated = false;

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
        mlog(INFO, "Opening file: %s\n", fileName);
        file = H5Fopen(fileName, flags, H5P_DEFAULT);
        if(file < 0)
        {
            mlog(CRITICAL, "Failed to open file: %s\n", fileName);
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
        if(recData->datatype == RecordObject::INTEGER)
        {
            datatype = H5T_NATIVE_INT;
        }
        else if(recData->datatype == RecordObject::REAL)
        {
            datatype = H5T_NATIVE_DOUBLE;
        }
        else
        {
            datatype = H5Dget_type(dataset);
            datatype_allocated = true;
        }

        /* Get Datatype Size */
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
    if(datatype_allocated && datatype > 0) H5Tclose(datatype);
    if(space > 0) H5Sclose(space);
    if(dataset > 0) H5Dclose(dataset);
    if(file > 0) H5Fclose(file);

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool Hdf5DatasetDevice::isConnected (int num_open)
{
    (void)num_open;

    return connected;
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void Hdf5DatasetDevice::closeConnection (void)
{
    connected = false;
    if(dataBuffer) delete [] dataBuffer;
    dataBuffer = NULL;
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int Hdf5DatasetDevice::writeBuffer (const void* buf, int len)
{
    (void)buf;
    (void)len;

    return TIMEOUT_RC;
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int Hdf5DatasetDevice::readBuffer (void* buf, int len)
{
    int bytes = SHUTDOWN_RC;

    if(connected)
    {
        int bytes_remaining = dataSize - dataOffset;

        if(rawMode)
        {
            int bytes_to_copy = MIN(len, bytes_remaining);
            LocalLib::copy(buf, &dataBuffer[dataOffset], bytes_to_copy);
            dataOffset += bytes_to_copy;
            bytes = bytes_to_copy;
        }
        else // record
        {
            int bytes_to_copy = MIN(len - recObj->getAllocatedMemory(), bytes_remaining);
            if(bytes_to_copy > 0)
            {
                recData->offset = dataOffset;
                recData->size = bytes_to_copy;
                unsigned char* rec_buf = (unsigned char*)buf;
                int bytes_written = recObj->serialize(&rec_buf, RecordObject::COPY, len);
                LocalLib::copy(&rec_buf[bytes_written], &dataBuffer[dataOffset], bytes_to_copy);
                dataOffset += bytes_to_copy;
                bytes = bytes_written + bytes_to_copy;
            }
        }

        if(bytes < 0) connected = false;
    }

    return bytes;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int Hdf5DatasetDevice::getUniqueId (void)
{
    return 0;
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* Hdf5DatasetDevice::getConfig (void)
{
    return config;
}
