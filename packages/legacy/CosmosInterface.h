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

#ifndef __cosmos_interface__
#define __cosmos_interface__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CommandableObject.h"
#include "MsgQ.h"
#include "TcpSocket.h"
#include "Ordering.h"
#include "OsApi.h"

/******************************************************************************
 * COSMOS INTERFACE CLASS
 ******************************************************************************/

class CosmosInterface: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*  TYPE;
        static const char*  SYNC_PATTERN;
        static const int    SYNC_OFFSET = 0;
        static const int    SYNC_SIZE = 4;
        static const int    LENGTH_OFFSET = 4;
        static const int    LENGTH_SIZE = 2;
        static const int    HEADER_SIZE = SYNC_SIZE + LENGTH_SIZE;
        static const int    MAX_PACKET_SIZE = 0x10006;
        static const int    DEFAULT_MAX_CONNECTIONS = 5;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef int (*active_handler_t) (int fd, int flags, void* parm);

        typedef struct
        {
            CosmosInterface*    ci;
            const char*         ip_addr;
            int                 port;
            active_handler_t    handler;
        } listener_t;

        struct tlm_t
        {
            CosmosInterface*    ci;
            Subscriber*         sub;
            TcpSocket*          sock;
            Thread*             pid;

            tlm_t(void)
            {   ci = NULL;
                sub = NULL;
                sock = NULL;
                pid = NULL; }

            ~tlm_t(void)
            {   if(sub) delete sub;
                if(sock) delete sock;
                if(pid) delete pid; }
        };

        struct cmd_t
        {
            CosmosInterface*    ci;
            Publisher*          pub;
            TcpSocket*          sock;
            Thread*             pid;

            cmd_t(void)
            {   ci = NULL;
                pub = NULL;
                sock = NULL;
                pid = NULL; }

            ~cmd_t(void)
            {   if(pub) delete pub;
                if(sock) delete sock;
                if(pid) delete pid; }
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                    interfaceActive;
        int                     maxConnections;

        // telemetry connections
        Thread*                 tlmListenerPid;
        listener_t              tlmListener;
        MgOrdering<tlm_t*>      tlmConnections;
        Mutex                   tlmConnMut;
        const char*             tlmQName;

        // command connections
        Thread*                 cmdListenerPid;
        listener_t              cmdListener;
        MgOrdering<cmd_t*>      cmdConnections;
        Mutex                   cmdConnMut;
        const char*             cmdQName;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CosmosInterface         (CommandProcessor* cmd_proc, const char* obj_name, const char* tlmq_name, const char* cmdq_name, const char* tlm_ip, int tlm_port, const char* cmd_ip, int cmd_port, int max_connections);
                        ~CosmosInterface        (void);

        static void*    listenerThread          (void* parm);
        static int      pollHandler             (int fd, short* events, void* parm);
        static int      tlmActiveHandler        (int fd, int flags, void* parm);
        static int      cmdActiveHandler        (int fd, int flags, void* parm);
        static void*    telemetryThread         (void* parm);
        static void*    commandThread           (void* parm);

        static int      connectHandler          (int sock, const char* ip_addr, void* parm);
};

#endif  /* __cosmos_interface__ */
