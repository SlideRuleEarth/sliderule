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

#include "H5DatasetDevice.h"
#include "H5Lib.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* H5DatasetDevice::recType = "h5dataset";
const RecordObject::fieldDef_t H5DatasetDevice::recDef[] = {
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
int H5DatasetDevice::luaCreate (lua_State* L)
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
        return createLuaObject(L, new H5DatasetDevice(L, (role_t)_role, filename, dataset_name, id, raw_mode, datatype));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating H5DatasetDevice: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5DatasetDevice::init (void)
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
H5DatasetDevice::H5DatasetDevice (lua_State* L, role_t _role, const char* filename, const char* dataset_name, long id, bool raw_mode, RecordObject::valType_t datatype):
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

    /* Read File */
    try
    {
        dataSize = H5Lib::readAs(fileName, dataName, (RecordObject::valType_t)recData->datatype, (uint8_t**)&dataBuffer);
        connected = true;
    }
    catch (const std::runtime_error& e)
    {
        mlog(CRITICAL, "Failed to create H5DatasetDevice: %s\n", e.what());
        dataBuffer = NULL;
        dataSize = false;
        connected = false;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5DatasetDevice::~H5DatasetDevice (void)
{
    closeConnection();
    if(config) delete [] config;
    if(dataName) delete [] dataName;
    if(fileName) delete [] fileName;
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool H5DatasetDevice::isConnected (int num_open)
{
    (void)num_open;

    return connected;
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void H5DatasetDevice::closeConnection (void)
{
    connected = false;
    if(dataBuffer) delete [] dataBuffer;
    dataBuffer = NULL;
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int H5DatasetDevice::writeBuffer (const void* buf, int len)
{
    (void)buf;
    (void)len;

    return TIMEOUT_RC;
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int H5DatasetDevice::readBuffer (void* buf, int len)
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
int H5DatasetDevice::getUniqueId (void)
{
    return 0;
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* H5DatasetDevice::getConfig (void)
{
    return config;
}
