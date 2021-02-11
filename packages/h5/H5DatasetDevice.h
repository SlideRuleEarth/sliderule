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

#ifndef __h5_dataset__
#define __h5_dataset__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "RecordObject.h"
#include "DeviceObject.h"

/******************************************************************************
 * HDF5 DATASET HANDLER
 ******************************************************************************/

class H5DatasetDevice: public DeviceObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* recType;
        static const RecordObject::fieldDef_t recDef[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            int64_t     id;
            uint32_t    dataset; // record object pointer
            uint32_t    datatype; // RecordObject::fieldType_t
            uint32_t    offset;
            uint32_t    size;
        } h5dataset_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        RecordObject*   recObj;
        h5dataset_t*    recData;
        char*           fileName;
        const char*     dataName;
        uint8_t*        dataBuffer;
        int             dataSize;
        int             dataOffset;
        bool            rawMode;

        bool            connected;
        char*           config; // <filename>(<type>,<access>,<io>)

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            H5DatasetDevice     (lua_State* L, role_t _role, const char* filename, const char* dataset_name, long id, bool raw_mode, RecordObject::valType_t datatype);
                            ~H5DatasetDevice    (void);

        virtual bool        isConnected         (int num_open=0);   // is the file open
        virtual void        closeConnection     (void);             // close the file
        virtual int         writeBuffer         (const void* buf, int len);
        virtual int         readBuffer          (void* buf, int len);
        virtual int         getUniqueId         (void);             // returns file descriptor
        virtual const char* getConfig           (void);             // returns filename with attribute list
};

#endif  /* __h5_dataset__ */
