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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "AlertMonitor.h"
#include "Monitor.h"
#include "SystemConfig.h"
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
int AlertMonitor::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parmeters */
        const event_level_t level = static_cast<event_level_t>(getLuaInteger(L, 1, true, SystemConfig::settings().logLevel.value));
        const char* eventq_name = getLuaString(L, 2, true, EVENTQ);

        /* Return Object */
        return createLuaObject(L, new AlertMonitor(L, level, eventq_name));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
AlertMonitor::AlertMonitor(lua_State* L, event_level_t level, const char* eventq_name):
    Monitor(L, level, eventq_name, EventLib::alertRecType)
{
    // build file name
    StringLib::format(fileName, MAX_FILENAME_SIZE, "/tmp/%s.alerts", eventq_name);

    // open file
    fileHandle = fopen(fileName, "w");
    if(fileHandle == NULL)
    {
        stopMonitor();
        char err_buf[256];
        throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to open alert file %s: %s", fileName, strerror_r(errno, err_buf, sizeof(err_buf)));
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
AlertMonitor::~AlertMonitor(void)
{
    stopMonitor();

    // close file
    const int rc1 = fclose(fileHandle);
    if(rc1 != 0)
    {
        char err_buf[256];
        mlog(CRITICAL, "Failed (%d) to close alert file: %s", rc1, strerror_r(errno, err_buf, sizeof(err_buf)));
    }

    // delete file
    const int rc2 = remove(fileName);
    if(rc2 != 0)
    {
        char err_buf[256];
        mlog(CRITICAL, "Failed (%d) to delete file %s: %s", rc2, fileName, strerror_r(errno, err_buf, sizeof(err_buf)));
    }
}

/*----------------------------------------------------------------------------
 * snapshot
 *----------------------------------------------------------------------------*/
const char* AlertMonitor::snapshot (void)
{
    fflush(fileHandle);
    return fileName;
}

/*----------------------------------------------------------------------------
 * processEvent
 *----------------------------------------------------------------------------*/
void AlertMonitor::processEvent(const unsigned char* event_buf_ptr, int event_size)
{
    (void)event_size;

    /* Cast to Structure */
    const EventLib::alert_t* event = reinterpret_cast<const EventLib::alert_t*>(event_buf_ptr);

    /* Filter Events */
    if(event->level < eventLevel) return;

    /* Write Message */
    char message_buffer[MAX_OUTPUT_SIZE];
    const int message_size = StringLib::formats(message_buffer, MAX_OUTPUT_SIZE, "[%s/%d] %s\n", EventLib::lvl2str((event_level_t)event->level), event->code, event->text);
    fwrite(message_buffer, 1, message_size, fileHandle);
}
