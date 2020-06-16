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

#include "Hdf5File.h"
#include "core.h"

/******************************************************************************
 * HDF5 FILE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - hdf5file(<filename>)
 *
 *  <filename> is the name of the HDF5 file to be read from or written to
 *----------------------------------------------------------------------------*/
int Hdf5File::luaCreate(lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* _filename   = getLuaString(L, 1);

        /* Return File Device Object */
        return createLuaObject(L, new Hdf5File(L, _filename));
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
Hdf5File::Hdf5File (lua_State* L, const char* _filename):
    DeviceObject(L, READER),
    connected(false),
    filename(StringLib::duplicate(_filename))
{
    assert(_filename);

    /* Add Additional Meta Functions */
    LuaEngine::setAttrFunc(L, "dir", luaTraverse);

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

    return connected;
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void Hdf5File::closeConnection (void)
{
    connected = false;
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int Hdf5File::writeBuffer (const void* buf, int len)
{
    (void)buf;
    (void)len;

    return TIMEOUT_RC;
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int Hdf5File::readBuffer (void* buf, int len)
{
    (void)buf;
    (void)len;

    return TIMEOUT_RC;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int Hdf5File::getUniqueId (void)
{
    return 0;
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

/*----------------------------------------------------------------------------
 * luaTraverse - :dir([<max depth>], [<starting group>])
 *----------------------------------------------------------------------------*/
typedef union {
    struct {
        uint32_t depth;
        uint32_t max;
    } curr;
    uint64_t data;
} rdepth_t;

herr_t hdf5_iter_op_func (hid_t loc_id, const char *name, const H5L_info_t *info, void *operator_data)
{
    (void)info;

    herr_t      retval = 0;
    rdepth_t    recurse = { .data = (uint64_t)operator_data };

    for(unsigned i = 0; i < recurse.curr.depth; i++) mlog(RAW, "  ");

    H5O_info_t object_info;
    H5Oget_info_by_name(loc_id, name, &object_info, H5P_DEFAULT);
    switch (object_info.type)
    {
        case H5O_TYPE_GROUP:
        {
            H5L_info_t link_info;
            H5Lget_info(loc_id, name, &link_info, H5P_DEFAULT);
            if(link_info.type == H5L_TYPE_HARD)
            {
                mlog(RAW, "%s: {", name);
                recurse.curr.depth++;
                if(recurse.curr.depth < recurse.curr.max)
                {
                    mlog(RAW, "\n");
                    retval = H5Literate_by_name(loc_id, name, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, hdf5_iter_op_func, (void*)recurse.data, H5P_DEFAULT);
                    for(unsigned i = 0; i < recurse.curr.depth - 1; i++) mlog(RAW, "  ");
                    mlog(RAW, "}\n");
                }
                else
                {
                    mlog(RAW, " }\n");
                }
            }
            else
            {
                mlog(RAW, "*%s\n", name);
            }
            break;
        }
        case H5O_TYPE_DATASET:
        {
            mlog(RAW, "%s\n", name);
            break;
        }
        case H5O_TYPE_NAMED_DATATYPE:
        {
            mlog(RAW, "%s (type)\n", name);
            break;
        }
        default:
        {
            mlog(RAW, "%s (unknown)\n", name);
        }
    }

    return retval;
}

int Hdf5File::luaTraverse (lua_State* L)
{
    bool        status = false;
    rdepth_t    recurse = {.data = 0};
    hid_t       file = INVALID_RC;
    hid_t       group = INVALID_RC;

    try
    {
        /* Get Self */
        Hdf5File* lua_obj = (Hdf5File*)getLuaSelf(L, 1);

        /* Get Maximum Recursion Depth */
        uint32_t max_depth = getLuaInteger(L, 2, true, 32);
        recurse.curr.max = max_depth;

        /* Open File */
        file = H5Fopen(lua_obj->filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        if(file < 0)
        {
            throw LuaException("Failed to open file: %s", lua_obj->filename);
        }

        /* Open Group */
        const char* group_path = getLuaString(L, 3, true, NULL);
        if(group_path)
        {
            group = H5Gopen(file, group_path, H5P_DEFAULT);
            if(group < 0)
            {
                throw LuaException("Failed to open group: %s", group_path);
            }
        }

        /* Display File Structure */
        if(H5Literate(group > 0 ? group : file, H5_INDEX_NAME, H5_ITER_NATIVE, NULL, hdf5_iter_op_func, (void*)recurse.data) >= 0)
        {
            status = true;
        }
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error traversing hdf5 file: %s\n", e.errmsg);
    }

    /* Clean Up */
    if(file > 0) H5Fclose(file);
    if(group > 0) H5Gclose(group);

    /* Return Status */
    return returnLuaStatus(L, status);
}
