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

#include <aws/core/Aws.h>
#include <aws/firehose/FirehoseClient.h>
#include <aws/firehose/model/PutRecordRequest.h>

#include "TelemetryMonitor.h"
#include "Monitor.h"
#include "EventLib.h"
#include "TimeLib.h"
#include "RecordObject.h"
#include "SystemConfig.h"

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

        /* Return Dispatch Object */
        return createLuaObject(L, new TelemetryMonitor(L, level, eventq_name));
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
    (void)event_size;

    /* Cast to Telemetry Structure */
    const EventLib::telemetry_t* event = reinterpret_cast<const EventLib::telemetry_t*>(event_buf_ptr);

    /* Filter Events */
    if(event->level < eventLevel) return;

    /* Build Telemetry JSON */
    const TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(event->time);
    const TimeLib::date_t date = TimeLib::gmt2date(gmt);
    const FString rqst(R"json({
        "record_time": "%04d-%02d-%02d %02d:%02d:%02d",
        "source_ip": "%s",
        "aoi": {"x": %lf, "y": %lf},
        "client": "%s",
        "endpoint": "%s",
        "duration": %f,
        "status_code": %d,
        "account": "%s",
        "version": "%s"
    }\n)json",
        date.year, date.month, date.day,
        gmt.hour, gmt.minute, gmt.second,
        event->source_ip,
        event->longitude, event->latitude,
        event->client,
        event->endpoint,
        event->duration,
        event->code,
        event->account,
        event->version);

    /* Post Telemetry to Firehose */
    Aws::Firehose::Model::Record record;
    record.SetData(Aws::Utils::ByteBuffer(reinterpret_cast<const unsigned char*>(rqst.c_str()), rqst.length()));
    request.SetRecord(record);
    auto outcome = firehoseClient.PutRecord(request);

    /* Log Errors */
    if(outcome.IsSuccess())
    {
        inError = false;
    }
    else if(!inError)
    {
        inError = true;
        mlog(CRITICAL, "Failed to post telemetry to firehose: %s", outcome.GetError().GetMessage().c_str());
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
TelemetryMonitor::TelemetryMonitor(lua_State* L, event_level_t level, const char* eventq_name):
    Monitor(L, level, eventq_name, EventLib::telemetryRecType),
    inError(false)
{
    request.SetDeliveryStreamName(SystemConfig::settings().recorderStream.value.c_str());
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
TelemetryMonitor::~TelemetryMonitor(void)
{
    stopMonitor();
}
