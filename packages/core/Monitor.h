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

#ifndef __monitor__
#define __monitor__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "DispatchObject.h"
#include "RecordObject.h"
#include "OsApi.h"
#include "EventLib.h"

/******************************************************************************
 * Monitor CLASS
 ******************************************************************************/

class Monitor: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            TEXT,
            JSON,
            CLOUD,
            RECORD
        } format_t;

        typedef enum {
            TERM = 0,
            LOCAL = 1,
            MSGQ = 2
        } cat_mode_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaCreate (lua_State* L);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual void processEvent   (const unsigned char* event_buf_ptr, int event_size);
                     Monitor        (lua_State* L, uint8_t type_mask, event_level_t level, format_t format, const char* eventq_name);
                     ~Monitor       (void) override;

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_EVENT_SIZE = 1280;
        static const int MAX_TAIL_SIZE = 65536;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*    monitorThread   (void* parm);

        static int      textOutput      (EventLib::event_t* event, char* event_buffer);
        static int      jsonOutput      (EventLib::event_t* event, char* event_buffer);
        static int      cloudOutput     (EventLib::event_t* event, char* event_buffer);

        static int      luaConfig       (lua_State* L);
        static int      luaTail         (lua_State* L);
        static int      luaCat          (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool            active;
        Thread*         pid;
        Subscriber*     inQ;
        uint8_t         eventTypeMask;
        event_level_t   eventLevel;
        format_t        outputFormat;
        char*           eventTailArray; // [][MAX_EVENT_SIZE]
        int             eventTailSize;
        int             eventTailIndex;
};

#endif  /* __monitor__ */
