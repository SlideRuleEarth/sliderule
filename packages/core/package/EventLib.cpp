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
    {"time",    RecordObject::INT64,    offsetof(EventLib::log_t, systime),     1,                        NULL, NATIVE_FLAGS},
    {"tid",     RecordObject::INT64,    offsetof(EventLib::log_t, tid),         1,                        NULL, NATIVE_FLAGS},
    {"level",   RecordObject::UINT16,   offsetof(EventLib::log_t, level),       1,                        NULL, NATIVE_FLAGS},
    {"ipv4",    RecordObject::STRING,   offsetof(EventLib::log_t, ipv4),        SockLib::IPV4_STR_LEN,    NULL, NATIVE_FLAGS},
    {"source",  RecordObject::STRING,   offsetof(EventLib::log_t, source),      EventLib::MAX_SRC_STR,    NULL, NATIVE_FLAGS},
    {"message", RecordObject::STRING,   offsetof(EventLib::log_t, message),     0,                        NULL, NATIVE_FLAGS}
};

static RecordObject::fieldDef_t traceRecDef[] =
{
    {"time",    RecordObject::INT64,    offsetof(EventLib::trace_t, systime),   1,                        NULL, NATIVE_FLAGS},
    {"tid",     RecordObject::INT64,    offsetof(EventLib::trace_t, tid),       1,                        NULL, NATIVE_FLAGS},
    {"id",      RecordObject::UINT32,   offsetof(EventLib::trace_t, id),        1,                        NULL, NATIVE_FLAGS},
    {"parent",  RecordObject::UINT32,   offsetof(EventLib::trace_t, parent),    1,                        NULL, NATIVE_FLAGS},
    {"flags",   RecordObject::UINT16,   offsetof(EventLib::trace_t, flags),     1,                        NULL, NATIVE_FLAGS},
    {"level",   RecordObject::UINT16,   offsetof(EventLib::trace_t, level),     1,                        NULL, NATIVE_FLAGS},
    {"ipv4",    RecordObject::STRING,   offsetof(EventLib::trace_t, ipv4),      SockLib::IPV4_STR_LEN,    NULL, NATIVE_FLAGS},
    {"name",    RecordObject::STRING,   offsetof(EventLib::trace_t, name),      EventLib::MAX_NAME_STR,  NULL, NATIVE_FLAGS},
    {"attr",    RecordObject::STRING,   offsetof(EventLib::trace_t, attr),      0,                        NULL, NATIVE_FLAGS}
};

static RecordObject::fieldDef_t metricRecDef[] =
{
    {"time",        RecordObject::INT64,    offsetof(EventLib::telemetry_t, systime),  1,                        NULL, NATIVE_FLAGS},
    {"code",        RecordObject::INT32,    offsetof(EventLib::telemetry_t, code),     1,                        NULL, NATIVE_FLAGS},
    {"duration",    RecordObject::FLOAT,    offsetof(EventLib::telemetry_t, duration), 1,                        NULL, NATIVE_FLAGS},
    {"latitude",    RecordObject::DOUBLE,   offsetof(EventLib::telemetry_t, aoi) + offsetof(MathLib::coord_t, lat), 1, NULL, NATIVE_FLAGS},
    {"longitude",   RecordObject::DOUBLE,   offsetof(EventLib::telemetry_t, aoi) + offsetof(MathLib::coord_t, lon), 1, NULL, NATIVE_FLAGS},
    {"level",       RecordObject::UINT16,   offsetof(EventLib::telemetry_t, level),    1,                        NULL, NATIVE_FLAGS},
    {"ipv4",        RecordObject::STRING,   offsetof(EventLib::telemetry_t, ipv4),     SockLib::IPV4_STR_LEN,    NULL, NATIVE_FLAGS},
    {"client",      RecordObject::STRING,   offsetof(EventLib::telemetry_t, client),   EventLib::MAX_METRIC_STR, NULL, NATIVE_FLAGS},
    {"account",     RecordObject::STRING,   offsetof(EventLib::telemetry_t, account),  EventLib::MAX_METRIC_STR, NULL, NATIVE_FLAGS},
    {"version",     RecordObject::STRING,   offsetof(EventLib::telemetry_t, version),  EventLib::MAX_METRIC_STR, NULL, NATIVE_FLAGS},
    {"message",     RecordObject::STRING,   offsetof(EventLib::telemetry_t, message),  0,                        NULL, NATIVE_FLAGS}
};

static RecordObject::fieldDef_t alertRecDef[] =
{
    {"code",    RecordObject::INT32,    offsetof(EventLib::alert_t, code),      1,                        NULL, NATIVE_FLAGS},
    {"level",   RecordObject::INT32,    offsetof(EventLib::alert_t, level),     1,                        NULL, NATIVE_FLAGS},
    {"text",    RecordObject::STRING,   offsetof(EventLib::alert_t, text),      EventLib::MAX_ALERT_STR, NULL, NATIVE_FLAGS}
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* EventLib::logRecType = "eventrec";
const char* EventLib::traceRecType = "tracerec";
const char* EventLib::telemetryRecType = "metricrec";
const char* EventLib::alertRecType = "exceptrec";

std::atomic<uint32_t> EventLib::trace_id{1};
Thread::key_t EventLib::trace_key;

event_level_t EventLib::logLevel;
event_level_t EventLib::traceLevel;
event_level_t EventLib::telemetryLevel;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void EventLib::init (const char* eventq)
{
    /* Define Records */
    RECDEF(logRecType, logRecDef, offsetof(log_t, message) + 1, NULL);
    RECDEF(traceRecType, traceRecDef, offsetof(trace_t, attr) + 1, NULL);
    RECDEF(telemetryRecType, metricRecDef, offsetof(telemetry_t, message) + 1, NULL);
    RECDEF(alertRecType, alertRecDef, sizeof(alert_t), "code");

    /* Create Thread Global */
    trace_key = Thread::createGlobal();
    Thread::setGlobal(trace_key, (void*)ORIGIN);

    /* Set Default Event Level */
    logLevel = INFO;
    traceLevel = INFO;
    telemetryLevel = CRITICAL;

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
 * str2lvl
 *----------------------------------------------------------------------------*/
bool EventLib::setLvl (type_t type, event_level_t lvl)
{
    switch(type)
    {
        case LOG:       logLevel = lvl;    return true;
        case TRACE:     traceLevel = lvl;  return true;
        case TELEMETRY:    telemetryLevel = lvl; return true;
        default:                           return false;
    }
}

/*----------------------------------------------------------------------------
 * str2lvl
 *----------------------------------------------------------------------------*/
event_level_t EventLib::getLvl (type_t type)
{
    switch(type)
    {
        case LOG:       return logLevel;
        case TRACE:     return traceLevel;
        case TELEMETRY:    return telemetryLevel;
        default:        return INVALID_EVENT_LEVEL;
    }
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
        case TELEMETRY:    return "TELEMETRY";
        default:        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * logMsg
 *----------------------------------------------------------------------------*/
bool EventLib::logMsg(const char* file_name, unsigned int line_number, event_level_t lvl, const char* msg_fmt, ...)
{
    log_t event;

    /* Return Here If Nothing to Do */
    if(lvl < logLevel) return true;

    /* Initialize Log Message */
    event.systime   = TimeLib::gpstime();
    event.tid       = Thread::getId();
    event.level     = lvl;

    /* Copy IP Address */
    StringLib::copy(event.ipv4, SockLib::sockipv4(), SockLib::IPV4_STR_LEN);

    /* Build Name - <Filename>:<Line Number> */
    const char* last_path_delimeter = StringLib::find(file_name, PATH_DELIMETER, false);
    const char* file_name_only = last_path_delimeter ? last_path_delimeter + 1 : file_name;
    StringLib::format(event.source, MAX_SRC_STR, "%s:%d", file_name_only, line_number);

    /* Build Message - <log message> */
    va_list args;
    va_start(args, msg_fmt);
    const int vlen = vsnprintf(event.message, MAX_MSG_STR - 1, msg_fmt, args);
    const int msg_size = MAX(MIN(vlen + 1, MAX_MSG_STR), 1);
    event.message[msg_size - 1] = '\0';
    va_end(args);

    /* Post Log Message */
    const int event_record_size = offsetof(log_t, message) + msg_size;
    RecordObject record(logRecType, event_record_size, false);
    log_t* data = reinterpret_cast<log_t*>(record.getRecordData());
    memcpy(data, &event, event_record_size);
    return record.post(outq, 0, NULL, false);
}

/*----------------------------------------------------------------------------
 * alertMsg
 *----------------------------------------------------------------------------*/
bool EventLib::alertMsg (const char* file_name, unsigned int line_number, event_level_t lvl, int code, void* rspsq, const bool* active, const char* errmsg, ...)
{
    bool status = true;

    /* Allocate and Initialize Alert Record */
    RecordObject record(alertRecType);
    alert_t* alert = reinterpret_cast<alert_t*>(record.getRecordData());
    alert->code = code;
    alert->level = (int32_t)lvl;

    /* Build Message */
    va_list args;
    va_start(args, errmsg);
    const int vlen = vsnprintf(alert->text, MAX_ALERT_STR - 1, errmsg, args);
    const int attr_size = MAX(MIN(vlen + 1, MAX_ALERT_STR), 1);
    alert->text[attr_size - 1] = '\0';
    va_end(args);

    /* Generate Corresponding Log Message */
    EventLib::logMsg(file_name, line_number, lvl, "%s", alert->text);

    /* Post Alert Record */
    if(rspsq)
    {
        Publisher* rspsq_ptr = reinterpret_cast<Publisher*>(rspsq); // avoids cyclic dependency with RecordObject
        status = record.post(rspsq_ptr, 0, active);
    }

    return status;
}

/*----------------------------------------------------------------------------
 * startTrace
 *----------------------------------------------------------------------------*/
uint32_t EventLib::startTrace(uint32_t parent, const char* name, event_level_t lvl, const char* attr_fmt, ...)
{
    trace_t event;

    /* Return Here If Nothing to Do */
    if(lvl < traceLevel) return parent;

    /* Initialize Trace */
    event.systime   = TimeLib::latchtime() * 1000000; // us
    event.tid       = Thread::getId();
    event.id        = trace_id++;;
    event.parent    = parent;
    event.flags     = START;
    event.level     = lvl;
    event.name[0]   = '\0';
    event.attr[0]   = '\0';

    /* Copy IP Address */
    StringLib::copy(event.ipv4, SockLib::sockipv4(), SockLib::IPV4_STR_LEN);

    /* Copy Name */
    StringLib::copy(event.name, name, MAX_NAME_STR);

    /* Build Attribute */
    va_list args;
    va_start(args, attr_fmt);
    const int vlen = vsnprintf(event.attr, MAX_ATTR_STR - 1, attr_fmt, args);
    const int attr_size = MAX(MIN(vlen + 1, MAX_ATTR_STR), 1);
    event.attr[attr_size - 1] = '\0';
    va_end(args);

    /* Send Event */
    const int event_record_size = offsetof(trace_t, attr) + attr_size;
    RecordObject record(traceRecType, event_record_size, false);
    trace_t* data = reinterpret_cast<trace_t*>(record.getRecordData());
    memcpy(data, &event, event_record_size);
    record.post(outq, 0, NULL, false);

    /* Return Trace ID */
    return event.id;
}

/*----------------------------------------------------------------------------
 * stopTrace
 *----------------------------------------------------------------------------*/
void EventLib::stopTrace(uint32_t id, event_level_t lvl)
{
    trace_t event;

    /* Return Here If Nothing to Do */
    if(lvl < traceLevel) return;

    /* Initialize Trace */
    event.systime   = TimeLib::latchtime() * 1000000; // us
    event.tid       = 0;
    event.id        = id;
    event.parent    = ORIGIN;
    event.flags     = STOP;
    event.level     = lvl;
    event.name[0]   = '\0';
    event.attr[0]   = '\0';

    /* Copy IP Address */
    StringLib::copy(event.ipv4, SockLib::sockipv4(), SockLib::IPV4_STR_LEN);

    /* Send Event */
    const int event_record_size = offsetof(trace_t, attr) + 1;
    RecordObject record(traceRecType, event_record_size, false);
    trace_t* data = reinterpret_cast<trace_t*>(record.getRecordData());
    memcpy(data, &event, event_record_size);
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
 * generateMetric
 *----------------------------------------------------------------------------*/
bool EventLib::captureMetric (event_level_t lvl, int code, float duration, MathLib::coord_t aoi, const char* client, const char* account, const char* msg_fmt, ...)
{
    telemetry_t event;

    /* Return Here If Nothing to Do */
    if(lvl < telemetryLevel) return;

    /* Initialize Metric Message */
    event.systime   = TimeLib::gpstime();
    event.level     = lvl;
    event.code      = code;
    event.duration  = duration;
    event.aoi       = aoi;

    /* Copy String Arguments */
    StringLib::copy(event.client, client, EventLib::MAX_METRIC_STR);
    StringLib::copy(event.account, account, EventLib::MAX_METRIC_STR);
    StringLib::copy(event.version, LIBID, EventLib::MAX_METRIC_STR);

    /* Copy IP Address */
    StringLib::copy(event.ipv4, SockLib::sockipv4(), SockLib::IPV4_STR_LEN);

    /* Build Message - <message> */
    va_list args;
    va_start(args, msg_fmt);
    const int vlen = vsnprintf(event.message, MAX_MSG_STR - 1, msg_fmt, args);
    const int msg_size = MAX(MIN(vlen + 1, MAX_MSG_STR), 1);
    event.message[msg_size - 1] = '\0';
    va_end(args);

    /* Post Metric Message */
    const int event_record_size = offsetof(telemetry_t, message) + msg_size;
    RecordObject record(telemetryRecType, event_record_size, false);
    telemetry_t* data = reinterpret_cast<telemetry_t*>(record.getRecordData());
    memcpy(data, &event, event_record_size);
    return record.post(outq, 0, NULL, false);
}
