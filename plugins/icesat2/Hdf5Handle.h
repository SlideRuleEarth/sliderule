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

#ifndef __hdf5_handle__
#define __hdf5_handle__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <hdf5.h>

#include "LuaObject.h"
#include "RecordObject.h"
#include "Ordering.h"

/******************************************************************************
 * HDF5 HANDLER (PURE VIRTUAL)
 ******************************************************************************/

class Hdf5Handle: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual bool    open    (hid_t h) = 0;
        virtual bool    read    (hid_t h, void* buf, int len) = 0;
        virtual bool    write   (hid_t h, const void* buf, int len) = 0;
        virtual bool    close   (hid_t h) = 0;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                Hdf5Handle  (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]);
        virtual ~Hdf5Handle (void) = 0;
};

/******************************************************************************
 * HDF5 DATASET HANDLER
 ******************************************************************************/

class Hdf5DatasetHandle: public Hdf5Handle
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const long INFINITE_CHUNK = -1;

        static const char* datasetRecType;
        static const RecordObject::fieldDef_t datasetRecDef[];

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];        

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            int64_t     id;
            uint32_t    offset;
            uint32_t    size;
        } dataset_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char* name;
        long        chunk;
        dataset_t   dataset;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                Hdf5DatasetHandle   (lua_State* L);
                ~Hdf5DatasetHandle  (void);

        bool    open                (hid_t h);
        bool    read                (hid_t h, void* buf, int len);
        bool    write               (hid_t h, const void* buf, int len);
        bool    close               (hid_t h);
};

#endif  /* __hdf5_handle__ */
