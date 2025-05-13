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

#include "LogMonitor.h"
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
int LogMonitor::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parmeters */
        const event_level_t level = static_cast<event_level_t>(getLuaInteger(L, 1));
        const format_t output_format = static_cast<format_t>(getLuaInteger(L, 2));
        const char* eventq_name = getLuaString(L, 3, true, EVENTQ);

        /* Return Dispatch Object */
        return createLuaObject(L, new LogMonitor(L, level, output_format, eventq_name));
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
void LogMonitor::processEvent(const unsigned char* event_buf_ptr, int event_size)
{
    /* Cast to Log Structure */
    const EventLib::log_t* event = reinterpret_cast<const EventLib::log_t*>(event_buf_ptr);

    /* Filter Events */
    if(event->level < eventLevel) return;

    /* Format Event */
    char event_buffer[MAX_LOG_OUTPUT_SIZE];
    if(outputFormat == CLOUD)
    {
        event_size = cloudOutput(event, event_buffer);
    }
    else if(outputFormat == TEXT)
    {
        event_size = textOutput(event, event_buffer);
    }
    else return; // unsupported format

    /* Write Log Message */
    fwrite(event_buffer, 1, event_size, stdout);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LogMonitor::LogMonitor(lua_State* L, event_level_t level, format_t output_format, const char* eventq_name):
    Monitor(L, level, eventq_name, EventLib::logRecType),
    outputFormat(output_format)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LogMonitor::~LogMonitor(void)
{
    stopMonitor();
}

/*----------------------------------------------------------------------------
 * textOutput
 *----------------------------------------------------------------------------*/
int LogMonitor::textOutput (const EventLib::log_t* event, char* event_buffer)
{
    char* msg = event_buffer;

    /* Populate Prefix */
    const TimeLib::gmt_time_t timeinfo = TimeLib::gps2gmttime(event->time);
    const TimeLib::date_t dateinfo = TimeLib::gmt2date(timeinfo);
    const double seconds = (double)timeinfo.second + ((double)timeinfo.millisecond / 1000.0);
    msg += StringLib::formats(msg, MAX_LOG_OUTPUT_SIZE, "%04d-%02d-%02dT%02d:%02d:%.03lfZ %s:%s:%s ",
        timeinfo.year, dateinfo.month, dateinfo.day, timeinfo.hour, timeinfo.minute, seconds,
        event->ipv4, EventLib::lvl2str((event_level_t)event->level), event->source);

    /* Populate Message */
    msg += StringLib::formats(msg, MAX_LOG_OUTPUT_SIZE - (msg - event_buffer), "%s\n", event->message);

    /* Return Size of Message */
    return msg - event_buffer + 1;
}

/*----------------------------------------------------------------------------
 * cloudOutput
 *----------------------------------------------------------------------------*/
int LogMonitor::cloudOutput (const EventLib::log_t* event, char* event_buffer)
{
    char* msg = event_buffer;

    /* Populate Message */
    msg += StringLib::formats(msg, MAX_LOG_OUTPUT_SIZE, "ip=%s level=%s caller=%s msg=\"%s\"\n",
        event->ipv4, EventLib::lvl2str_lc((event_level_t)event->level), event->source, event->message);

    /* Return Size of Message */
    return msg - event_buffer + 1;
}
