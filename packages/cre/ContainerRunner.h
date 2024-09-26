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

#ifndef __container_runner__
#define __container_runner__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "MsgQ.h"
#include "LuaObject.h"
#include "CreParms.h"

/******************************************************************************
 * CONTAINER RUNNER CLASS
 ******************************************************************************/

class ContainerRunner: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        static const int RESULT_SIGNAL = 0;
        static const int WAIT_TIMEOUT = 30;

        static const char* SANDBOX_MOUNT;
        static const char* HOST_DIRECTORY;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate           (lua_State* L);
        static void         init                (void);
        static void         deinit              (void);
        static const char*  getRegistry         (void);
        static int          luaList             (lua_State* L);
        static int          luaSetRegistry      (lua_State* L);
        static int          luaSettings         (lua_State* L);
        static int          luaCreateUnique     (lua_State* L);
        static int          luaDeleteUnique     (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const char* REGISTRY;

        bool            active;
        Thread*         controlPid;
        Publisher*      outQ;
        const char*     hostSandboxDirectory;
        Cond            resultLock;
        CreParms*       parms;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        ContainerRunner         (lua_State* L, CreParms* _parms, const char* host_shared_directory, const char* outq_name);
                        ~ContainerRunner        (void) override;
        static void*    controlThread           (void* parm);
        static void     processContainerLogs    (const char* buffer, int buffer_size, const char* id);
};

#endif  /* __container_runner__ */
