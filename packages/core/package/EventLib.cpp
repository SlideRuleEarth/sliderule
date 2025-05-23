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

#include "OsApi.h"
#include "EventLib.h"
#include "SystemConfig.h"
#include "TimeLib.h"
#include "MsgQ.h"
#include "StringLib.h"
#include "RecordObject.h"
#include "Dictionary.h"
#include "List.h"

#include <cstdarg>
#include <atomic>

/******************************************************************************
 * FILE DATA
 ******************************************************************************/

/*
 * These objects are declared outside of the EventLib header file so that
 * the corresponding RecordObject and MsgQ headers are not needed in the
 * EventLib header, which creates a cyclic dependency.
 */
static Publisher* outq;

static RecordObject::fieldDef_t logRecDef[] =
{
    {"time",    RecordObject::INT64,    offsetof(EventLib::log_t, time),        1,                        NULL, NATIVE_FLAGS},
    {"level",   RecordObject::UINT32,   offsetof(EventLib::log_t, level),       1,                        NULL, NATIVE_FLAGS},
    {"ipv4",    RecordObject::STRING,   offsetof(EventLib::log_t, ipv4),        EventLib::MAX_IPV4_STR,   NULL, NATIVE_FLAGS},
    {"source",  RecordObject::STRING,   offsetof(EventLib::log_t, source),      EventLib::MAX_SRC_STR,    NULL, NATIVE_FLAGS},
    {"message", RecordObject::STRING,   offsetof(EventLib::log_t, message),     0,                        NULL, NATIVE_FLAGS}
};

static RecordObject::fieldDef_t traceRecDef[] =
{
    {"time",    RecordObject::INT64,    offsetof(EventLib::trace_t, time),      1,                        NULL, NATIVE_FLAGS},
    {"tid",     RecordObject::INT64,    offsetof(EventLib::trace_t, tid),       1,                        NULL, NATIVE_FLAGS},
    {"id",      RecordObject::UINT32,   offsetof(EventLib::trace_t, id),        1,                        NULL, NATIVE_FLAGS},
    {"parent",  RecordObject::UINT32,   offsetof(EventLib::trace_t, parent),    1,                        NULL, NATIVE_FLAGS},
    {"flags",   RecordObject::UINT32,   offsetof(EventLib::trace_t, flags),     1,                        NULL, NATIVE_FLAGS},
    {"level",   RecordObject::UINT32,   offsetof(EventLib::trace_t, level),     1,                        NULL, NATIVE_FLAGS},
    {"ipv4",    RecordObject::STRING,   offsetof(EventLib::trace_t, ipv4),      EventLib::MAX_IPV4_STR,   NULL, NATIVE_FLAGS},
    {"name",    RecordObject::STRING,   offsetof(EventLib::trace_t, name),      EventLib::MAX_NAME_STR,   NULL, NATIVE_FLAGS},
    {"attr",    RecordObject::STRING,   offsetof(EventLib::trace_t, attr),      0,                        NULL, NATIVE_FLAGS}
};

static RecordObject::fieldDef_t telemetryRecDef[] =
{
    {"time",        RecordObject::INT64,    offsetof(EventLib::telemetry_t, time),      1,                          NULL, NATIVE_FLAGS},
    {"code",        RecordObject::INT32,    offsetof(EventLib::telemetry_t, code),      1,                          NULL, NATIVE_FLAGS},
    {"duration",    RecordObject::FLOAT,    offsetof(EventLib::telemetry_t, duration),  1,                          NULL, NATIVE_FLAGS},
    {"latitude",    RecordObject::DOUBLE,   offsetof(EventLib::telemetry_t, latitude),  1,                          NULL, NATIVE_FLAGS},
    {"longitude",   RecordObject::DOUBLE,   offsetof(EventLib::telemetry_t, longitude), 1,                          NULL, NATIVE_FLAGS},
    {"level",       RecordObject::UINT16,   offsetof(EventLib::telemetry_t, level),     1,                          NULL, NATIVE_FLAGS},
    {"ip",          RecordObject::STRING,   offsetof(EventLib::telemetry_t, source_ip), EventLib::MAX_TLM_STR,      NULL, NATIVE_FLAGS},
    {"client",      RecordObject::STRING,   offsetof(EventLib::telemetry_t, client),    EventLib::MAX_TLM_STR,      NULL, NATIVE_FLAGS},
    {"account",     RecordObject::STRING,   offsetof(EventLib::telemetry_t, account),   EventLib::MAX_TLM_STR,      NULL, NATIVE_FLAGS},
    {"version",     RecordObject::STRING,   offsetof(EventLib::telemetry_t, version),   EventLib::MAX_TLM_STR,      NULL, NATIVE_FLAGS}
};

static RecordObject::fieldDef_t alertRecDef[] =
{
    {"code",    RecordObject::INT32,    offsetof(EventLib::alert_t, code),      1,                        NULL, NATIVE_FLAGS},
    {"level",   RecordObject::UINT32,   offsetof(EventLib::alert_t, level),     1,                        NULL, NATIVE_FLAGS},
    {"text",    RecordObject::STRING,   offsetof(EventLib::alert_t, text),      EventLib::MAX_ALERT_STR,  NULL, NATIVE_FLAGS}
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* EventLib::logRecType = "eventrec";
const char* EventLib::traceRecType = "tracerec";
const char* EventLib::telemetryRecType = "telemetryrec";
const char* EventLib::alertRecType = "exceptrec";

std::atomic<uint32_t> EventLib::trace_id{1};
Thread::key_t EventLib::trace_key;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void EventLib::init (const char* eventq)
{
    /* Define Records */
    RECDEF(logRecType, logRecDef, sizeof(log_t), NULL);
    RECDEF(traceRecType, traceRecDef, sizeof(trace_t), NULL);
    RECDEF(telemetryRecType, telemetryRecDef, sizeof(telemetry_t), NULL);
    RECDEF(alertRecType, alertRecDef, sizeof(alert_t), NULL);

    /* Create Thread Global */
    trace_key = Thread::createGlobal();
    Thread::setGlobal(trace_key, (void*)ORIGIN);

    /* Create Output Q */
    outq = new Publisher(eventq);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void EventLib::deinit (void)
{
    /* Cleanup Output Q */
    delete outq;
}

/*----------------------------------------------------------------------------
 * lvl2str
 *----------------------------------------------------------------------------*/
const char* EventLib::lvl2str (event_level_t lvl)
{
    switch(lvl)
    {
        case DEBUG:     return "DEBUG";
        case INFO:      return "INFO";
        case WARNING:   return "WARNING";
        case ERROR:     return "ERROR";
        case CRITICAL:  return "CRITICAL";
        default:        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * lvl2str_lc
 *----------------------------------------------------------------------------*/
const char* EventLib::lvl2str_lc (event_level_t lvl)
{
    switch(lvl)
    {
        case DEBUG:     return "debug";
        case INFO:      return "info";
        case WARNING:   return "warning";
        case ERROR:     return "error";
        case CRITICAL:  return "critical";
        default:        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * type2str
 *----------------------------------------------------------------------------*/
const char* EventLib::type2str (type_t type)
{
    switch(type)
    {
        case LOG:       return "LOG";
        case TRACE:     return "TRACE";
        case TELEMETRY: return "TELEMETRY";
        case ALERT:     return "ALERT";
        default:        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * logMsg
 *----------------------------------------------------------------------------*/
bool EventLib::logMsg(const char* file_name, unsigned int line_number, event_level_t lvl, const char* msg_fmt, ...)
{
    /* Return Here If Nothing to Do */
    if(lvl < SystemConfig::settings().logLevel.value) return true;

    /* Initialize Log Message */
    RecordObject record(logRecType, 0, false);
    log_t* event    = reinterpret_cast<log_t*>(record.getRecordData());
    event->time     = TimeLib::gpstime();
    event->level    = lvl;

    /* Copy IP Address */
    StringLib::copy(event->ipv4, SystemConfig::settings().ipv4.value.c_str(), EventLib::MAX_IPV4_STR);

    /* Build Name - <Filename>:<Line Number> */
    const char* last_path_delimeter = StringLib::find(file_name, PATH_DELIMETER, false);
    const char* file_name_only = last_path_delimeter ? last_path_delimeter + 1 : file_name;
    StringLib::format(event->source, MAX_SRC_STR, "%s:%d", file_name_only, line_number);

    /* Build Message - <log message> */
    va_list args;
    va_start(args, msg_fmt);
    const int vlen = vsnprintf(event->message, MAX_MSG_STR - 1, msg_fmt, args);
    const int msg_size = MAX(MIN(vlen + 1, MAX_MSG_STR), 1);
    event->message[msg_size - 1] = '\0';
    va_end(args);

    /* Post Log Message */
    record.setUsedData(offsetof(log_t, message) + msg_size);
    return record.post(outq, 0, NULL, false);
}

/*----------------------------------------------------------------------------
 * startTrace
 *----------------------------------------------------------------------------*/
uint32_t EventLib::startTrace(uint32_t parent, const char* name, event_level_t lvl, const char* attr_fmt, ...)
{
    const uint32_t id = trace_id++;

    /* Return Here If Nothing to Do */
    if(lvl < SystemConfig::settings().traceLevel.value) return parent;

    /* Initialize Trace */
    RecordObject record(traceRecType, 0, false);
    trace_t* event  = reinterpret_cast<trace_t*>(record.getRecordData());
    event->time     = TimeLib::latchtime() * 1000000; // us
    event->tid      = Thread::getId();
    event->id       = id;
    event->parent   = parent;
    event->flags    = START;
    event->level    = lvl;
    event->name[0]  = '\0';
    event->attr[0]  = '\0';

    /* Copy IP Address */
    StringLib::copy(event->ipv4, SystemConfig::settings().ipv4.value.c_str(), EventLib::MAX_IPV4_STR);

    /* Copy Name */
    StringLib::copy(event->name, name, MAX_NAME_STR);

    /* Build Attribute */
    va_list args;
    va_start(args, attr_fmt);
    const int vlen = vsnprintf(event->attr, MAX_ATTR_STR - 1, attr_fmt, args);
    const int attr_size = MAX(MIN(vlen + 1, MAX_ATTR_STR), 1);
    event->attr[attr_size - 1] = '\0';
    va_end(args);

    /* Send Event */
    record.setUsedData(offsetof(trace_t, attr) + attr_size);
    record.post(outq, 0, NULL, false);

    /* Return Trace ID */
    return id;
}

/*----------------------------------------------------------------------------
 * stopTrace
 *----------------------------------------------------------------------------*/
void EventLib::stopTrace(uint32_t id, event_level_t lvl)
{
    /* Return Here If Nothing to Do */
    if(lvl < SystemConfig::settings().traceLevel.value) return;

    /* Initialize Trace */
    RecordObject record(traceRecType, 0, false);
    trace_t* event  = reinterpret_cast<trace_t*>(record.getRecordData());
    event->time     = TimeLib::latchtime() * 1000000; // us
    event->tid      = 0;
    event->id       = id;
    event->parent   = ORIGIN;
    event->flags    = STOP;
    event->level    = lvl;
    event->name[0]  = '\0';
    event->attr[0]  = '\0';

    /* Copy IP Address */
    StringLib::copy(event->ipv4, SystemConfig::settings().ipv4.value.c_str(), EventLib::MAX_IPV4_STR);

    /* Send Event */
    record.setUsedData(offsetof(trace_t, attr) + 1);
    record.post(outq, 0, NULL, false);
}

/*----------------------------------------------------------------------------
 * stashId
 *----------------------------------------------------------------------------*/
void EventLib::stashId (uint32_t id)
{
    Thread::setGlobal(trace_key, reinterpret_cast<void*>(static_cast<unsigned long long>(id))); // NOLINT(performance-no-int-to-ptr)
}

/*----------------------------------------------------------------------------
 * grabId
 *----------------------------------------------------------------------------*/
uint32_t EventLib::grabId (void)
{
    return static_cast<uint32_t>(reinterpret_cast<unsigned long long>(Thread::getGlobal(trace_key)));
}

/*----------------------------------------------------------------------------
 * sendTlm
 *----------------------------------------------------------------------------*/
bool EventLib::sendTlm (event_level_t lvl, const tlm_input_t& tlm)
{
    /* Return Here If Nothing to Do */
    if(lvl < SystemConfig::settings().telemetryLevel.value) return true;

    /* Initialize Telemetry Message */
    RecordObject record(telemetryRecType, 0, false);
    telemetry_t* event  = reinterpret_cast<telemetry_t*>(record.getRecordData());
    event->time         = TimeLib::gpstime();
    event->level        = lvl;
    event->code         = tlm.code;
    event->duration     = tlm.duration;
    event->latitude     = tlm.latitude;
    event->longitude    = tlm.longitude;

    /* Copy String Arguments */
    StringLib::copy(event->endpoint, tlm.endpoint, EventLib::MAX_TLM_STR);
    StringLib::copy(event->source_ip, tlm.source_ip, EventLib::MAX_TLM_STR);
    StringLib::copy(event->client, tlm.client, EventLib::MAX_TLM_STR);
    StringLib::copy(event->account, tlm.account, EventLib::MAX_TLM_STR);
    StringLib::copy(event->version, LIBID, EventLib::MAX_TLM_STR);

    /* Post Telemetry Message */
    return record.post(outq, 0, NULL, false);
}

/*----------------------------------------------------------------------------
 * sendAlert
 *----------------------------------------------------------------------------*/
bool EventLib::sendAlert (event_level_t lvl, int code, void* rspsq, const bool* active, const char* errmsg, ...)
{
    /* Return Here If Nothing to Do */
    if(lvl < SystemConfig::settings().alertLevel.value) return true;

    /* Allocate and Initialize Alert Record */
    RecordObject record(alertRecType, 0, false);
    alert_t* event  = reinterpret_cast<alert_t*>(record.getRecordData());
    event->code     = code;
    event->level    = (int32_t)lvl;

    /* Build Message */
    va_list args;
    va_start(args, errmsg);
    const int vlen = vsnprintf(event->text, MAX_ALERT_STR - 1, errmsg, args);
    const int text_size = MAX(MIN(vlen + 1, MAX_ALERT_STR), 1);
    event->text[text_size - 1] = '\0';
    va_end(args);

    /* Log Alert */
    mlog(static_cast<event_level_t>(event->level), "<alert=%d> %s", event->code, event->text);

    /* Post to Event Q
     *  - must allocate here so that the record is still owned for the call to
     *    post it below. */
    bool status = record.post(outq, 0, active, false, SYS_TIMEOUT, RecordObject::ALLOCATE);

    /* Post to Response Q */
    if(rspsq)
    {
        Publisher* rspsq_ptr = reinterpret_cast<Publisher*>(rspsq); // avoids cyclic dependency with RecordObject
        status = status && record.post(rspsq_ptr, 0, active);
    }

    return status;
}
