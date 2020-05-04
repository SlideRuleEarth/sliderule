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

#ifndef __datasrv_socket_reader__
#define __datasrv_socket_reader__

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class DatasrvSocketReader: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

        static int parseApidSet (const char* apid_set, uint16_t apids[]);

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        unsigned char*      request;
        int                 request_size;

        bool                read_active;
        Thread*             reader;
        Publisher*          outq;
        const char*         ip_addr;
        int                 port;
        unsigned long       bytes_read;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        DatasrvSocketReader (CommandProcessor* cmd_proc, const char* obj_name, const char* ip_addr, int port, const char* outq_name, const char* start_time, const char* end_time, const char* req_arch_str, uint16_t apids[], int num_apids);
        ~DatasrvSocketReader (void);

        static void* reader_thread (void* parm);

        bool initConnection (TcpSocket* sock);
        int logPktStatsCmd (int argc, char argv[][MAX_CMD_SIZE]);
        void addRqstParm (List<unsigned char>& l, const char* parm);
};

#endif  /* __datasrv_socket_reader__ */
