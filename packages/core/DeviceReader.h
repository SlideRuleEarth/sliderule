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

#ifndef __device_reader__
#define __device_reader__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "MsgQ.h"
#include "DeviceIO.h"

/******************************************************************************
 * DEVICE READER CLASS
 ******************************************************************************/

class DeviceReader: public DeviceIO
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Publisher*      outq;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        DeviceReader        (lua_State* L, DeviceObject* _device, const char* outq_name);
                        ~DeviceReader       (void);

        static void*    readerThread        (void* parm);
};

#endif  /* __device_reader__ */
