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

#ifndef __uart_device__
#define __uart_device__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "DeviceObject.h"

/******************************************************************************
 * UART CLASS
 ******************************************************************************/

class Uart: public DeviceObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            NONE = 'N',
            EVEN = 'E',
            ODD = 'O'
        } parity_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate           (lua_State* L);

                        Uart                (lua_State* L, const char* _device, int _baud, parity_t _parity);
        virtual         ~Uart               ();

        bool            isConnected         (int num_connections=0);
        void            closeConnection     (void);
        int             writeBuffer         (const void* buf, int len);
        int             readBuffer          (void* buf, int len);
        int             getUniqueId         (void);
        const char*     getConfig           (void);

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        int         fd;
        char*       config; // <dev>:<baud>:8<parity>1
};

#endif  /* __uart_device__ */
