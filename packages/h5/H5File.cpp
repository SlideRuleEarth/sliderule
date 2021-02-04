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

#include "H5Lib.h"
#include "H5Lite.h"
#include "H5File.h"
#include "H5Array.h"
#include "core.h"

/******************************************************************************
 * HDF5 FILE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - H5File(<filename>)
 *
 *  <filename> is the name of the HDF5 file to be read from or written to
 *----------------------------------------------------------------------------*/
int H5File::luaCreate(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* _filename   = getLuaString(L, 1);

        /* Return File Device Object */
        return createLuaObject(L, new H5File(L, _filename));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating HDF5 File: %s\n", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5File::H5File (lua_State* L, const char* _filename):
    DeviceObject(L, READER),
    connected(false),
    filename(StringLib::duplicate(_filename))
{
    assert(_filename);

    /* Add Additional Meta Functions */
    LuaEngine::setAttrFunc(L, "dir", luaTraverse);
    LuaEngine::setAttrFunc(L, "inspect", luaInspect);

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s (%s)", filename, role == READER ? "READER" : "WRITER") + 1;
    config = new char[cfglen];
    sprintf(config, "%s (%s)", filename, role == READER ? "READER" : "WRITER");
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5File::~H5File (void)
{
    closeConnection();
    if(filename) delete [] filename;
    if(config) delete [] config;
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool H5File::isConnected (int num_open)
{
    (void)num_open;

    return connected;
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void H5File::closeConnection (void)
{
    connected = false;
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int H5File::writeBuffer (const void* buf, int len)
{
    (void)buf;
    (void)len;

    return TIMEOUT_RC;
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int H5File::readBuffer (void* buf, int len)
{
    (void)buf;
    (void)len;

    return TIMEOUT_RC;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int H5File::getUniqueId (void)
{
    return 0;
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* H5File::getConfig (void)
{
    return config;
}

/*----------------------------------------------------------------------------
 * getFilename
 *----------------------------------------------------------------------------*/
const char* H5File::getFilename (void)
{
    return filename;
}

/*----------------------------------------------------------------------------
 * luaTraverse - :dir([<max depth>], [<starting group>])
 *----------------------------------------------------------------------------*/
int H5File::luaTraverse (lua_State* L)
{
    bool  status = false;

    try
    {
        /* Get Self */
        H5File* lua_obj = (H5File*)getLuaSelf(L, 1);

        /* Get Parameters */
        uint32_t max_depth = getLuaInteger(L, 2, true, 32);
        const char* group_path = getLuaString(L, 3, true, NULL);

        /* Traverse File */
//        status = H5Lib::traverse(lua_obj->filename, max_depth, group_path);
        status = H5Lite::traverse(lua_obj->filename, max_depth, group_path);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error traversing hdf5 file: %s\n", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaInspect - :inspect(<dataset>, <datatype>)
 *----------------------------------------------------------------------------*/
int H5File::luaInspect (lua_State* L)
{
    bool    status = true;

    try
    {
        /* Get Self */
        H5File* lua_obj = (H5File*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* dataset_name = getLuaString(L, 2);
        const char* datatype_name = getLuaString(L, 3, true, "double");

        /* Open Dataset */
        if(StringLib::match("double", datatype_name))
        {
            H5Array<double> values(lua_obj->filename, dataset_name);
            for(int i = 0; i < values.size; i++) printf("%lf\n", values[i]);
        }
        else if(StringLib::match("float", datatype_name))
        {
            H5Array<float> values(lua_obj->filename, dataset_name);
            for(int i = 0; i < values.size; i++) printf("%f\n", values[i]);
        }
        else if(StringLib::match("long", datatype_name))
        {
            H5Array<long> values(lua_obj->filename, dataset_name);
            for(int i = 0; i < values.size; i++) printf("%ld\n", values[i]);
        }
        else if(StringLib::match("int", datatype_name))
        {
            H5Array<int> values(lua_obj->filename, dataset_name);
            for(int i = 0; i < values.size; i++) printf("%d\n", values[i]);
        }
        else if(StringLib::match("short", datatype_name))
        {
            H5Array<short> values(lua_obj->filename, dataset_name);
            for(int i = 0; i < values.size; i++) printf("%d\n", values[i]);
        }
        else if(StringLib::match("char", datatype_name))
        {
            H5Array<char> values(lua_obj->filename, dataset_name);
            for(int i = 0; i < values.size; i++) printf("%c\n", values[i]);
        }
        else if(StringLib::match("byte", datatype_name))
        {
            H5Array<unsigned char> values(lua_obj->filename, dataset_name);
            for(int i = 0; i < values.size; i++) printf("%02X\n", values[i]);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error inspecting hdf5 file: %s\n", e.what());
        status = false;
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}