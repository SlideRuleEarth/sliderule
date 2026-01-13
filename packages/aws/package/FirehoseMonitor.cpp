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

#include "FirehoseMonitor.h"
#include "Monitor.h"
#include "EventLib.h"
#include "TimeLib.h"
#include "StringLib.h"
#include "SystemConfig.h"

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<level>)
 *----------------------------------------------------------------------------*/
int FirehoseMonitor::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parmeters */
        const event_level_t level = static_cast<event_level_t>(getLuaInteger(L, 1));
        const char* rec_type = getLuaString(L, 2);
        const char* stream_name = getLuaString(L, 3);
        const char* eventq_name = getLuaString(L, 4, true, EVENTQ);

        /* Return Dispatch Object */
        return createLuaObject(L, new FirehoseMonitor(L, level, rec_type, stream_name, eventq_name));
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
void FirehoseMonitor::processEvent(const unsigned char* event_buf_ptr, int event_size)
{
    (void)event_size;

    /* Build JSON */
    const FString* json = convertToJson(event_buf_ptr, eventLevel);
    if(!json) return;

    /* Post Alert to Firehose */
    Aws::Firehose::Model::Record record;
    record.SetData(Aws::Utils::ByteBuffer(reinterpret_cast<const unsigned char*>(json->c_str()), json->length()));
    request.SetRecord(record);
    auto outcome = firehoseClient.PutRecord(request);
    delete json;

    /* Log Errors */
    if(outcome.IsSuccess())
    {
        if(inError)
        {
            mlog(INFO, "Successfully posted to firehose");
        }
        inError = false;
    }
    else if(!inError)
    {
        inError = true;
        mlog(CRITICAL, "Failed to post to firehose: %s", outcome.GetError().GetMessage().c_str());
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
FirehoseMonitor::FirehoseMonitor(lua_State* L, event_level_t level, const char* rec_type, const char* stream_name, const char* eventq_name):
    Monitor(L, level, eventq_name, rec_type),
    convertToJson(NULL),
    inError(false)
{
    request.SetDeliveryStreamName(stream_name);
    if(StringLib::match(rec_type, EventLib::alertRecType)) convertToJson = jsonAlert;
    else if(StringLib::match(rec_type, EventLib::telemetryRecType)) convertToJson = jsonTlm;
    else if(StringLib::match(rec_type, EventLib::logRecType)) throw RunTimeException(CRITICAL, RTE_FAILURE, "Log messages are currently unsupported source for firehose");
    else if(StringLib::match(rec_type, EventLib::traceRecType)) throw RunTimeException(CRITICAL, RTE_FAILURE, "Trace messages are currently unsupported source for firehose");
    else throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid record type supplied to firehose: %s", rec_type);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
FirehoseMonitor::~FirehoseMonitor(void)
{
    stopMonitor();
}

/*----------------------------------------------------------------------------
 * jsonAlert
 *----------------------------------------------------------------------------*/
const FString* FirehoseMonitor::jsonAlert (const unsigned char* event_buf_ptr, event_level_t lvl)
{
    const EventLib::alert_t* event = reinterpret_cast<const EventLib::alert_t*>(event_buf_ptr);
    if(event->level < lvl) return NULL;
    const TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(TimeLib::gpstime());
    const TimeLib::date_t date = TimeLib::gmt2date(gmt);
    const char* encoded_str = StringLib::jsonize(event->text);
    const FString* json = new FString(R"json({"timestamp":"%04d-%02d-%02dT%02d:%02d:%02dZ","code":%d,"cluster":"%s","version":"%s","message":"%s"})json""\n",
        date.year, date.month, date.day,
        gmt.hour, gmt.minute, gmt.second,
        event->code,
        SystemConfig::settings().cluster.value.c_str(),
        LIBID,
        encoded_str);
    delete [] encoded_str;
    return json;
}

/*----------------------------------------------------------------------------
 * jsonTlm
 *----------------------------------------------------------------------------*/
const FString* FirehoseMonitor::jsonTlm (const unsigned char* event_buf_ptr, event_level_t lvl)
{
    const EventLib::telemetry_t* event = reinterpret_cast<const EventLib::telemetry_t*>(event_buf_ptr);
    if(event->level < lvl) return NULL;
    const TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(event->time);
    const TimeLib::date_t date = TimeLib::gmt2date(gmt);
    const FString* json = new FString(R"json({"timestamp":"%04d-%02d-%02dT%02d:%02d:%02dZ","source_ip":"%s","aoi_x":%lf,"aoi_y":%lf,"client":"%s","endpoint":"%s","duration":%f,"code":%d,"account":"%s","cluster":"%s","version":"%s"})json""\n",
        date.year, date.month, date.day,
        gmt.hour, gmt.minute, gmt.second,
        event->source_ip,
        event->longitude, event->latitude,
        event->client,
        event->endpoint,
        event->duration,
        event->code,
        event->account,
        SystemConfig::settings().cluster.value.c_str(),
        LIBID);
    return json;
}
