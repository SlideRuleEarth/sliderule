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

#ifndef __orchestrator_lib__
#define __orchestrator_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "core.h"

/******************************************************************************
 * ORCHESTRATOR LIBRARY CLASS
 ******************************************************************************/

class OrchestratorLib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const long INVALID_TX_ID = -1;
        static const int MAX_LOCKS_PER_NODE = 3; // must match value in intelligent load balancer

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        struct Node {
            const char* member;
            long transaction;

            Node (const char* _member, long _transaction) {
                assert(_member);
                member = StringLib::duplicate(_member);
                transaction = _transaction;
            }

            ~Node (void) {
                delete [] member;
            }
        };

        typedef struct {
            long code;
            const char* response;
            int size;
        } rsps_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void             init                (void);
        static void             deinit              (void);

        static rsps_t           request             (EndpointObject::verb_t verb, const char* resource, const char* data);

        static bool             registerService     (const char* service, int lifetime, const char* address, bool initial_registration, bool verbose=false);
        static long             selflock            (const char* service, int timeout_secs, int locks_per_node, bool verbose);
        static vector<Node*>*   lock                (const char* service, int nodes_needed, int timeout_secs, int locks_per_node, bool verbose=false);
        static bool             unlock              (long transactions[], int num_transactions, bool verbose=false);
        static bool             health              (void);
        static bool             metric              (const unsigned char* metric_buf, int buf_size);
        static int              getNodes            (void);

        static int              luaUrl              (lua_State* L);
        static int              luaRegisterService  (lua_State* L);
        static int              luaSelfLock         (lua_State* L);
        static int              luaLock             (lua_State* L);
        static int              luaUnlock           (lua_State* L);
        static int              luaHealth           (lua_State* L);
        static int              luaGetNodes         (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const char*  URL;
};

#endif  /* __orchestrator_lib__ */
