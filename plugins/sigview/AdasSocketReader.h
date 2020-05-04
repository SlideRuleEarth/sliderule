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

#ifndef __adas_socket_reader__
#define __adas_socket_reader__

#include "core.h"
#include "legacy.h"

class AdasSocketReader: public CommandableObject
{
    public:

        static const char* TYPE;

        static CommandableObject* createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        bool            read_active;
        Thread*         reader;
        Publisher*      outq;
        TcpSocket*      sock;
        int             bytes_read;

                        AdasSocketReader    (CommandProcessor* cmd_proc, const char* obj_name, const char* ip_addr, int port, const char* outq_name);
                        ~AdasSocketReader   (void);

        static void*    reader_thread       (void* parm);

        int             logPktStatsCmd      (int argc, char argv[][MAX_CMD_SIZE]);
        bool            initConnection      (void);
};

#endif  /* __adas_socket_reader__ */
