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

#include "h5.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* H5DatasetDevice::recType = "h5dataset";
const RecordObject::fieldDef_t H5DatasetDevice::recDef[] = {
    {"id",      RecordObject::INT64,    offsetof(h5dataset_t, id),      1,  NULL, NATIVE_FLAGS},
    {"datatype",RecordObject::UINT32,   offsetof(h5dataset_t, datatype),1,  NULL, NATIVE_FLAGS},
    {"offset",  RecordObject::UINT32,   offsetof(h5dataset_t, offset),  1,  NULL, NATIVE_FLAGS},
    {"size",    RecordObject::UINT32,   offsetof(h5dataset_t, size),    1,  NULL, NATIVE_FLAGS},
    {"data",    RecordObject::UINT8,    sizeof(h5dataset_t),            0,  NULL, NATIVE_FLAGS}
};

/******************************************************************************
 * HDF5 DATASET HANDLE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<role>, <filename>, <dataset name>, [<id>], [<raw>], [<datatype>], [col], [startrow], [numrows])
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
        RecordObject::valType_t datatype = (RecordObject::valType_t)getLuaInteger(L, 6, true, RecordObject::DYNAMIC);
        long        col             = getLuaInteger(L, 7, true, 0);
        long        startrow        = getLuaInteger(L, 8, true, 0);
        long        numrows         = getLuaInteger(L, 9, true, H5Api::ALL_ROWS);

        /* Check Access Type */
        if(_role != DeviceObject::READER && _role != DeviceObject::WRITER)
        {
            throw RunTimeException(CRITICAL, "unrecognized file access specified: %d", _role);
        }

        /* Return Dispatch Object */
        return createLuaObject(L, new H5DatasetDevice(L, (role_t)_role, filename, dataset_name, id, raw_mode, datatype, col, startrow, numrows));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating H5DatasetDevice: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5DatasetDevice::init (void)
{
    int def_elements = sizeof(recDef) / sizeof(RecordObject::fieldDef_t);
    RecordObject::recordDefErr_t rc = RecordObject::defineRecord(recType, "id", sizeof(h5dataset_t), recDef, def_elements, 8);
    if(rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d", recType, rc);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5DatasetDevice::H5DatasetDevice (lua_State* L, role_t _role, const char* filename, const char* dataset_name, long id, bool raw_mode, 
                                    RecordObject::valType_t datatype, long col, long startrow, long numrows):
    DeviceObject(L, _role)
{
    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, traceId, "h5_device", "{\"file\":\"%s\", \"dataset\":%s}", filename, dataset_name);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Api

    /* Set Record */
    recObj = new RecordObject(recType);
    recData = (h5dataset_t*)recObj->getRecordData();

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
        H5Api::info_t info = H5Api::read(fileName, dataName, datatype, col, startrow, numrows);
        recData->datatype = (uint32_t)info.datatype;
        dataBuffer = info.data;
        dataSize = info.datasize;
        connected = true;
    }
    catch (const RunTimeException& e)
    {
        mlog(e.level(), "Failed to create H5DatasetDevice for %s/%s: %s", filename, dataset_name, e.what());
        dataBuffer = NULL;
        dataSize = false;
        connected = false;
    }

    /* Stop Trace */
    stop_trace(INFO, trace_id);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5DatasetDevice::~H5DatasetDevice (void)
{
    closeConnection();
    delete recObj;
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
            if(bytes_to_copy > 0)
            {
                LocalLib::copy(buf, &dataBuffer[dataOffset], bytes_to_copy);
                dataOffset += bytes_to_copy;
                bytes = bytes_to_copy;
            }
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
