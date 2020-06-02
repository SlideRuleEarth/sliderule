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

#include "OsApi.h"
#include "Hdf5File.h"
#include "StringLib.h"
#include "LogLib.h"
#include "DeviceObject.h"

/******************************************************************************
 * HDF5 FILE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - hdf5file(<role>, <filename>)
 *
 *  <role> is either dev.READER or dev.WRITER
 *
 *  <filename> is the name ofthe HDF5 file to be read from or written to
 *----------------------------------------------------------------------------*/
int Hdf5File::luaCreate(lua_State* L)
{
    try
    {
        /* Get Parameters */
        int         role      = (int)getLuaInteger(L, 1);
        const char* file_str  = getLuaString(L, 2);

        /* Check Access Type */
        if(role != DeviceObject::READER && role != DeviceObject::WRITER)
        {
            throw LuaException("unrecognized file access specified: %d\n", role);
        }

        /* Return File Device Object */
        return createLuaObject(L, new Hdf5File(L, (DeviceObject::role_t)role, file_str));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating HDF5 File: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Hdf5File::Hdf5File (lua_State* L, role_t _role, const char* _filename):
    DeviceObject(L, _role)
{
    assert(_filename);

    fp = NULL;

    /* Set Filename */
    filename = StringLib::duplicate(_filename);

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s (%s)", filename, role == READER ? "READER" : "WRITER") + 1;
    config = new char[cfglen];
    sprintf(config, "%s (%s)", filename, role == READER ? "READER" : "WRITER");
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Hdf5File::~Hdf5File (void)
{
    closeConnection();
    if(filename) delete [] filename;
    if(config) delete [] config;
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool Hdf5File::isConnected (int num_open)
{
    (void)num_open;
    return (fp != NULL);
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void Hdf5File::closeConnection (void)
{
    if(fp != NULL)
    {
        if(fp != stdout && fp != stderr)
        {
            fclose(fp);
        }
        fp = NULL;
    }
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int Hdf5File::writeBuffer (const void* buf, int len)
{
    (void)buf;
    (void)len;

    return 0;
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int Hdf5File::readBuffer (void* buf, int len)
{
    (void)buf;
    (void)len;

    return 0;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int Hdf5File::getUniqueId (void)
{
    return fileno(fp);
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* Hdf5File::getConfig (void)
{
    return config;
}

/*----------------------------------------------------------------------------
 * getFilename
 *----------------------------------------------------------------------------*/
const char* Hdf5File::getFilename (void)
{
    return filename;
}
