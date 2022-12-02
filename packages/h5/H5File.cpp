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

const char* H5File::ObjectType = "H5File";
const char* H5File::LuaMetaName = "H5File";
const struct luaL_Reg H5File::LuaMetaTable[] = {
    {"read",        luaRead},
    {"dir",         luaTraverse},
    {"inspect",     luaInspect},
    {NULL,          NULL}
};

const char* H5File::recType = "h5file";
const RecordObject::fieldDef_t H5File::recDef[] = {
    {"dataset", RecordObject::STRING,   offsetof(h5file_t, dataset), MAX_NAME_STR,  NULL, NATIVE_FLAGS},
    {"datatype",RecordObject::UINT32,   offsetof(h5file_t, datatype),1,             NULL, NATIVE_FLAGS},
    {"elements",RecordObject::UINT32,   offsetof(h5file_t, elements),1,             NULL, NATIVE_FLAGS},
    {"size",    RecordObject::UINT32,   offsetof(h5file_t, size),    1,             NULL, NATIVE_FLAGS},
    {"data",    RecordObject::UINT8,    sizeof(h5file_t),            0,             NULL, NATIVE_FLAGS}
};

/******************************************************************************
 * HDF5 FILE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - H5File(<asset>, <resource>)
 *
 *  <filename> is the name of the HDF5 file to be read from or written to
 *----------------------------------------------------------------------------*/
int H5File::luaCreate(lua_State* L)
{
    Asset* _asset = NULL;

    try
    {
        /* Get Parameters */
        _asset = (Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* _resource = getLuaString(L, 2);

        /* Return File Device Object */
        return createLuaObject(L, new H5File(L, _asset, _resource));
    }
    catch(const RunTimeException& e)
    {
        if(_asset) _asset->releaseLuaObject();
        mlog(e.level(), "Error creating HDF5 File: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5File::init (void)
{
    RECDEF(recType, recDef, sizeof(h5file_t), NULL);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5File::H5File (lua_State* L, Asset* _asset, const char* _resource):
    LuaObject(L, ObjectType, LuaMetaName, LuaMetaTable)
{
    asset = _asset;
    resource = StringLib::duplicate(_resource);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5File::~H5File (void)
{
    if(resource) delete [] resource;
    asset->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * readThread
 *----------------------------------------------------------------------------*/
void* H5File::readThread (void* parm)
{
    dataset_info_t* info = (dataset_info_t*)parm;

    /* Declare and Initialize Results */
    H5Coro::info_t results;
    results.data = NULL;

    try
    {
        /* Read Dataset */
        results = H5Coro::read(info->h5file->asset, info->h5file->resource, info->dataset, info->valtype, info->col, info->startrow, info->numrows, &(info->h5file->context));
    }
    catch (const RunTimeException& e)
    {
        mlog(e.level(), "Failed to read dataset %s://%s/%s: %s", info->h5file->asset->getName(), info->h5file->resource, info->dataset, e.what());
    }

    /* Post Results to Output Queue */
    if(results.data)
    {
        /* Create Output Queue Publisher */
        Publisher outq(info->outqname);

        /* Create Record Object */
        RecordObject rec_obj(recType);
        h5file_t* rec_data = (h5file_t*)rec_obj.getRecordData();
        StringLib::copy(rec_data->dataset, info->dataset, MAX_NAME_STR);
        rec_data->datatype = (uint32_t)results.datatype;
        rec_data->elements = results.elements;
        rec_data->size = results.datasize;

        /* Post Record */
        unsigned char* rec_buf;
        int rec_size = rec_obj.serialize(&rec_buf, RecordObject::REFERENCE, sizeof(h5file_t) + results.datasize);
        int status = outq.postCopy(rec_buf, rec_size - results.datasize, results.data, results.datasize, SYS_TIMEOUT);
        if(status <= 0)
        {
            mlog(CRITICAL, "Failed (%d) to post h5 dataset: %s/%s", status, info->h5file->asset->getName(), info->dataset);
        }

        /* Clean Up Result Data */
        delete [] results.data;
    }

    /* Clean Up Thread Info */
    delete [] info->dataset;
    delete [] info->outqname;
    delete info;

    /* Exit Thread */
    return NULL;
}

/*----------------------------------------------------------------------------
 * luaRead - :read(<table of datasets>, <output q>)
 *----------------------------------------------------------------------------*/
int H5File::luaRead (lua_State* L)
{
    bool status = true;

    int self_index = 1;
    int tbl_index = 2;
    int outq_index = 3;

    List<Thread*> pids;
    const char* outq_name = NULL;
    H5File* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (H5File*)getLuaSelf(L, self_index);

        /* Get Output Queue */
        outq_name = getLuaString(L, outq_index);

        /* Process Table of Datasets */
        int num_datasets = lua_rawlen(L, tbl_index);
        if(lua_istable(L, tbl_index) && num_datasets > 0)
        {
            for(int i = 0; i < num_datasets; i++)
            {
                const char* dataset;
                long col, startrow, numrows;
                RecordObject::valType_t valtype;

                /* Get Dataset Entry */
                lua_rawgeti(L, tbl_index, i+1);
                if(lua_istable(L, -1))
                {
                    lua_getfield(L, -1, "dataset");
                    dataset = getLuaString(L, -1);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "valtype");
                    valtype = (RecordObject::valType_t)getLuaInteger(L, -1, true, RecordObject::DYNAMIC);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "col");
                    col = getLuaInteger(L, -1, true, 0);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "startrow");
                    startrow = getLuaInteger(L, -1, true, 0);
                    lua_pop(L, 1);

                    lua_getfield(L, -1, "numrows");
                    numrows = getLuaInteger(L, -1, true, H5Coro::ALL_ROWS);
                    lua_pop(L, 1);
                }
                else
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "expecting dataset entry");
                }

                /* Start Thread */
                dataset_info_t* info = new dataset_info_t;
                info->dataset = StringLib::duplicate(dataset);
                info->valtype = valtype;
                info->col = col;
                info->startrow = startrow;
                info->numrows = numrows;
                info->outqname = StringLib::duplicate(outq_name);
                info->h5file = lua_obj;
                Thread* pid = new Thread(readThread, info);
                pids.add(pid);

                /* Clean up stack */
                lua_pop(L, 1);
            }
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "expecting list of datasets");
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failed to read resource: %s", e.what());
        status = false;
    }

    /* Clean Up and Terminate */
    if(lua_obj && outq_name)
    {
        /* Wait for Threads to Complete */
        for(int i = 0; i < pids.length(); i++)
        {
            delete pids[i]; // performs join
        }

        /* Status Complete */
        mlog(INFO, "Finished reading %d datasets from %s", pids.length(), lua_obj->asset->getName());

        /* Terminate Data */
        Publisher outQ(outq_name);
        outQ.postCopy("", 0);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
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
        status = H5Coro::traverse(lua_obj->asset, lua_obj->resource, max_depth, group_path);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error traversing hdf5 file: %s", e.what());
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
            H5Array<double> values(lua_obj->asset, lua_obj->resource, dataset_name);
            for(int i = 0; i < values.size; i++) print2term("%lf\n", values[i]);
        }
        else if(StringLib::match("float", datatype_name))
        {
            H5Array<float> values(lua_obj->asset, lua_obj->resource, dataset_name);
            for(int i = 0; i < values.size; i++) print2term("%f\n", values[i]);
        }
        else if(StringLib::match("long", datatype_name))
        {
            H5Array<long> values(lua_obj->asset, lua_obj->resource, dataset_name);
            for(int i = 0; i < values.size; i++) print2term("%ld\n", values[i]);
        }
        else if(StringLib::match("int", datatype_name))
        {
            H5Array<int> values(lua_obj->asset, lua_obj->resource, dataset_name);
            for(int i = 0; i < values.size; i++) print2term("%d\n", values[i]);
        }
        else if(StringLib::match("short", datatype_name))
        {
            H5Array<short> values(lua_obj->asset, lua_obj->resource, dataset_name);
            for(int i = 0; i < values.size; i++) print2term("%d\n", values[i]);
        }
        else if(StringLib::match("char", datatype_name))
        {
            H5Array<char> values(lua_obj->asset, lua_obj->resource, dataset_name);
            for(int i = 0; i < values.size; i++) print2term("%c\n", values[i]);
        }
        else if(StringLib::match("byte", datatype_name))
        {
            H5Array<unsigned char> values(lua_obj->asset, lua_obj->resource, dataset_name);
            for(int i = 0; i < values.size; i++) print2term("%02X\n", values[i]);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error inspecting hdf5 file: %s\n", e.what());
        status = false;
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}