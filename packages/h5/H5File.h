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

#ifndef __h5_file__
#define __h5_file__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <hdf5.h>

#include "OsApi.h"
#include "DeviceObject.h"

/******************************************************************************
 * HDF5 FILE CLASS
 ******************************************************************************/

class H5File: public DeviceObject
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate           (lua_State* L);

                            H5File              (lua_State* L, const char* _filename);
        virtual             ~H5File             ();

        virtual bool        isConnected         (int num_open=0);   // is the file open
        virtual void        closeConnection     (void);             // close the file
        virtual int         writeBuffer         (const void* buf, int len);
        virtual int         readBuffer          (void* buf, int len);
        virtual int         getUniqueId         (void);             // returns file descriptor
        virtual const char* getConfig           (void);             // returns filename with attribute list

        const char*         getFilename         (void);

        static int          luaTraverse         (lua_State* L);
        static int          luaInspect          (lua_State* L);

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool            connected;
        char*           filename; // user supplied prefix
        char*           config; // <filename>(<type>,<access>,<io>)
};

#endif  /* __h5_file__ */
