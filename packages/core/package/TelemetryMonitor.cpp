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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "TelemetryMonitor.h"
#include "Monitor.h"
#include "EventLib.h"
#include "TimeLib.h"
#include "RecordObject.h"
#include "OrchestratorLib.h"

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<level>)
 *----------------------------------------------------------------------------*/
int TelemetryMonitor::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parmeters */
        const event_level_t level = static_cast<event_level_t>(getLuaInteger(L, 1));
        const char* eventq_name = getLuaString(L, 2, true, EVENTQ);
        const char* outq_name = getLuaString(L, 3);

        /* Return Dispatch Object */
        return createLuaObject(L, new TelemetryMonitor(L, level, eventq_name, outq_name));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * processEvent
 *----------------------------------------------------------------------------*/
void TelemetryMonitor::processEvent(const unsigned char* event_buf_ptr, int event_size)
{
    /* Cast to Log Message */
    const EventLib::telemetry_t* event = reinterpret_cast<const EventLib::telemetry_t*>(event_buf_ptr);

    /* Filter Events */
    if(event->level < eventLevel) return;

    /* Post Telemetry to Manager */
    ManagerLib::telemetry();

    /* Post Metric to Orchestrator */
    OrchestratorLib::metric(event->endpoint, event->duration);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
TelemetryMonitor::TelemetryMonitor(lua_State* L, event_level_t level, const char* eventq_name, const char* outq_name):
    Monitor(L, level, eventq_name)
{
    outQ = new Publisher(outq_name);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
TelemetryMonitor::~TelemetryMonitor(void)
{
    delete outQ;
}

/*----------------------------------------------------------------------------
 * jsonOutput
 *----------------------------------------------------------------------------*/
int TelemetryMonitor::jsonOutput (const EventLib::trace_t* event, char* event_buffer)
{
    char* msg = event_buffer;

    /* Populate Message */
    if(event->attr[0] == '{')
    {
        /* Attribute String with No Quotes */
        msg += StringLib::formats(msg, 1024,
            "{\"systime\":%ld,\"ipv4\":\"%s\",\"flags\":%d,\"level\":\"%s\",\"tid\":%ld,\"id\":%ld,\"parent\":%ld,\"name\":\"%s\",\"attr\":%s}\n",
            event->systime, event->ipv4, event->flags, EventLib::lvl2str((event_level_t)event->level),
            event->tid, (long)event->id, (long)event->parent, event->name, event->attr);
    }
    else
    {
        /* Attribute String Quoted */
        msg += StringLib::formats(msg, 1024,
            "{\"systime\":%ld,\"ipv4\":\"%s\",\"flags\":%d,\"level\":\"%s\",\"tid\":%ld,\"id\":%ld,\"parent\":%ld,\"name\":\"%s\",\"attr\":\"%s\"}\n",
            event->systime, event->ipv4, event->flags, EventLib::lvl2str((event_level_t)event->level),
            event->tid, (long)event->id, (long)event->parent, event->name, event->attr);
    }

    /* Return Size of Message */
    return msg - event_buffer + 1;
}
