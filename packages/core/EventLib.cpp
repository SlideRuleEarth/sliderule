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
#include "RecordObject.h"

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

static RecordObject::fieldDef_t rec_def[] =
{
    {"time",    RecordObject::INT64,    offsetof(EventLib::event_t, systime), 1,                        NULL, NATIVE_FLAGS},
    {"tid",     RecordObject::INT64,    offsetof(EventLib::event_t, tid),     1,                        NULL, NATIVE_FLAGS},
    {"id",      RecordObject::UINT32,   offsetof(EventLib::event_t, id),      1,                        NULL, NATIVE_FLAGS},
    {"parent",  RecordObject::UINT32,   offsetof(EventLib::event_t, parent),  1,                        NULL, NATIVE_FLAGS},
    {"flags",   RecordObject::UINT16,   offsetof(EventLib::event_t, flags),   1,                        NULL, NATIVE_FLAGS},
    {"type",    RecordObject::UINT8,    offsetof(EventLib::event_t, type),    1,                        NULL, NATIVE_FLAGS},
    {"level",   RecordObject::UINT8,    offsetof(EventLib::event_t, level),   1,                        NULL, NATIVE_FLAGS},
    {"ipv4",    RecordObject::STRING,   offsetof(EventLib::event_t, ipv4),    SockLib::IPV4_STR_LEN,    NULL, NATIVE_FLAGS},
    {"name",    RecordObject::STRING,   offsetof(EventLib::event_t, name),    EventLib::MAX_NAME_SIZE,  NULL, NATIVE_FLAGS},
    {"attr",    RecordObject::STRING,   offsetof(EventLib::event_t, attr),    0,                        NULL, NATIVE_FLAGS}
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* EventLib::rec_type = "eventrec";

int EventLib::rec_type_size;

std::atomic<uint32_t> EventLib::trace_id{1};
Thread::key_t EventLib::trace_key;

event_level_t EventLib::log_level;
event_level_t EventLib::trace_level;
event_level_t EventLib::metric_level;

Mutex EventLib::metric_mut;
std::atomic<int32_t> EventLib::metric_id{1};
Dictionary<List<int32_t,EventLib::MAX_METRICS>*> EventLib::metric_ids_from_attr(MAX_METRICS, 1.0);
Dictionary<int32_t> EventLib::metric_ids_from_name(MAX_METRICS, 1.0);
Table<EventLib::metric_t, int32_t> EventLib::metric_vals(MAX_METRICS);

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void EventLib::init (const char* eventq)
{
    /* Define Event Record */
    RecordObject::recordDefErr_t rc = RecordObject::defineRecord(rec_type, NULL, offsetof(event_t, attr) + 1, rec_def, sizeof(rec_def) / sizeof(RecordObject::fieldDef_t), 16);
    if(rc != RecordObject::SUCCESS_DEF)
    {
        throw RunTimeException("Fatal error: failed to register event record");
    }

    /* Calculate Size of Record Type */
    rec_type_size = StringLib::size(rec_type) + 1;

    /* Create Thread Global */
    trace_key = Thread::createGlobal();
    Thread::setGlobal(trace_key, (void*)ORIGIN);
    
    /* Set Default Event Level */
    log_level = INFO;
    trace_level = CRITICAL;
    metric_level = CRITICAL;

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

    /* Clean Up Metrics */
    metric_mut.lock();
    {
        metric_t metric;
        int32_t key = metric_vals.first(&metric);
        while(key != (int32_t)INVALID_KEY)
        {
            delete [] metric.name;
            delete [] metric.attr;
            key = metric_vals.next(&metric);
        }

        List<int32_t,MAX_METRICS>* id_list = NULL;
        const char* attr = metric_ids_from_attr.first(&id_list);
        while(attr != NULL)
        {
            delete id_list;
            attr = metric_ids_from_attr.next(&id_list);
        }
    }
    metric_mut.unlock();
}

/*----------------------------------------------------------------------------
 * str2lvl
 *----------------------------------------------------------------------------*/
bool EventLib::setLvl (type_t type, event_level_t lvl)
{
    switch(type)
    {
        case LOG:       log_level = lvl;    return true;
        case TRACE:     trace_level = lvl;  return true;
        case METRIC:    metric_level = lvl; return true;
        default:                            return false;
    }
}

/*----------------------------------------------------------------------------
 * str2lvl
 *----------------------------------------------------------------------------*/
event_level_t EventLib::getLvl (type_t type)
{
    switch(type)
    {
        case LOG:       return log_level;
        case TRACE:     return trace_level;
        case METRIC:    return metric_level;
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
 * type2str
 *----------------------------------------------------------------------------*/
const char* EventLib::type2str (type_t type)
{
    switch(type)
    {
        case LOG:       return "LOG";
        case TRACE:     return "TRACE"; 
        case METRIC:    return "METRIC";
        default:        return NULL;
    }    
}

/*----------------------------------------------------------------------------
 * startTrace
 *----------------------------------------------------------------------------*/
uint32_t EventLib::startTrace(uint32_t parent, const char* name, event_level_t lvl, const char* fmt, ...)
{
    event_t event;
    
    /* Return Here If Nothing to Do */
    if(lvl < trace_level) return parent;

    /* Initialize Trace */
    event.systime   = TimeLib::gettimems();
    event.tid       = Thread::getId();
    event.id        = trace_id++;;
    event.parent    = parent;
    event.flags     = START;
    event.type      = TRACE;
    event.level     = lvl;
    event.name[0]   = '\0';
    event.attr[0]   = '\0';

    /* Copy IP Address */
    StringLib::copy(event.ipv4, SockLib::sockipv4(), SockLib::IPV4_STR_LEN);

    /* Copy Name */
    StringLib::copy(event.name, name, MAX_NAME_SIZE);

    /* Build Attribute */
    va_list args;
    va_start(args, fmt);
    int vlen = vsnprintf(event.attr, MAX_ATTR_SIZE - 1, fmt, args);
    int attr_size = MAX(MIN(vlen + 1, MAX_ATTR_SIZE), 1);
    event.attr[attr_size - 1] = '\0';
    va_end(args);

    /* Send Event */
    sendEvent(&event, attr_size);

    /* Return Trace ID */
    return event.id;
}

/*----------------------------------------------------------------------------
 * stopTrace
 *----------------------------------------------------------------------------*/
void EventLib::stopTrace(uint32_t id, event_level_t lvl)
{
    event_t event;
   
    /* Return Here If Nothing to Do */
    if(lvl < trace_level) return;

    /* Initialize Trace */
    event.systime   = TimeLib::gettimems();
    event.tid       = 0;
    event.id        = id;
    event.parent    = ORIGIN;
    event.flags     = STOP;
    event.type      = TRACE;
    event.level     = lvl;
    event.name[0]   = '\0';
    event.attr[0]   = '\0';

    /* Copy IP Address */
    StringLib::copy(event.ipv4, SockLib::sockipv4(), SockLib::IPV4_STR_LEN);


    /* Send Event */
    sendEvent(&event, 1);
}

/*----------------------------------------------------------------------------
 * stashId
 *----------------------------------------------------------------------------*/
void EventLib::stashId (uint32_t id)
{
    Thread::setGlobal(trace_key, (void*)(unsigned long long)id);
}

/*----------------------------------------------------------------------------
 * grabId
 *----------------------------------------------------------------------------*/
uint32_t EventLib::grabId (void)
{
    return (uint32_t)(unsigned long long)Thread::getGlobal(trace_key);
}


/*----------------------------------------------------------------------------
 * logMsg
 *----------------------------------------------------------------------------*/
void EventLib::logMsg(const char* file_name, unsigned int line_number, event_level_t lvl, const char* fmt, ...)
{
    event_t event;

    /* Return Here If Nothing to Do */
    if(lvl < log_level) return;

    /* Initialize Log Message */
    event.systime   = TimeLib::gettimems();
    event.tid       = Thread::getId();
    event.id        = ORIGIN;
    event.parent    = ORIGIN;
    event.flags     = 0;
    event.type      = LOG;
    event.level     = lvl;

    /* Copy IP Address */
    StringLib::copy(event.ipv4, SockLib::sockipv4(), SockLib::IPV4_STR_LEN);

    /* Build Name - <Filename>:<Line Number> */
    const char* last_path_delimeter = StringLib::find(file_name, PATH_DELIMETER, false);
    const char* file_name_only = last_path_delimeter ? last_path_delimeter + 1 : file_name;
    StringLib::format(event.name, MAX_NAME_SIZE, "%s:%d", file_name_only, line_number);

    /* Build Attribute - <log message> */
    va_list args;
    va_start(args, fmt);
    int vlen = vsnprintf(event.attr, MAX_ATTR_SIZE - 1, fmt, args);
    int attr_size = MAX(MIN(vlen + 1, MAX_ATTR_SIZE), 1);
    event.attr[attr_size - 1] = '\0';
    va_end(args);

    /* Post Log Message */
    sendEvent(&event, attr_size);
}

/*----------------------------------------------------------------------------
 * registerMetric
 *----------------------------------------------------------------------------*/
int32_t EventLib::registerMetric (const char* metric_name, const char* fmt, ...)
{
    assert(metric_name);

    metric_t metric;
    bool status = true;

    /* Build Attribute */
    char attr_buf[MAX_ATTR_SIZE];
    va_list args;
    va_start(args, fmt);
    int vlen = vsnprintf(attr_buf, MAX_ATTR_SIZE - 1, fmt, args);
    int attr_size = MAX(MIN(vlen + 1, MAX_ATTR_SIZE), 1);
    attr_buf[attr_size - 1] = '\0';
    va_end(args);

    /* Initialize Metric */
    metric.id       = metric_id++;
    metric.name     = StringLib::duplicate(metric_name);
    metric.attr     = StringLib::duplicate(attr_buf);
    metric.value    = 0.0;

    /* Regsiter Metric */
    metric_mut.lock();
    {
        /* Register Metric Name */
        if(metric_ids_from_name.add(metric.name, metric.id, true))
        {
            /* Register Metric Id */
            if(metric_vals.add(metric.id, metric))
            {
                /* Register Metric Attribute */
                try
                {
                    List<int32_t,MAX_METRICS>* id_list = metric_ids_from_attr[metric.attr];
                    id_list->add(metric.id);
                }
                catch(std::out_of_range& e)
                {
                    (void)e;
                    List<int32_t,MAX_METRICS>* id_list = new List<int32_t,MAX_METRICS>;
                    id_list->add(metric.id);
                    metric_ids_from_attr.add(metric.attr, id_list, true);
                }

            }
            else
            {
                mlog(CRITICAL, "Failed to regsiter id for metric %s\n", metric.name);
                metric_ids_from_name.remove(metric.name);
                status = false;
            }
        }
        else
        {
            mlog(CRITICAL, "Failed to regsiter metric %s\n", metric.name);
            status = false;
        }
    }
    metric_mut.unlock();

    /* Check Status */
    if(!status)
    {
        metric.id = INVALID_METRIC;
        delete [] metric.name;
        delete [] metric.attr;
    }

    /* Return Metric Id */
    return metric.id;
}

/*----------------------------------------------------------------------------
 * updateMetric
 *----------------------------------------------------------------------------*/
void EventLib::updateMetric (int32_t id, double value)
{
    metric_mut.lock();
    {
        metric_t& metric = metric_vals[id];
        metric.value = value;
    }
    metric_mut.unlock();
}

/*----------------------------------------------------------------------------
 * generateMetric
 *----------------------------------------------------------------------------*/
void EventLib::generateMetric (int32_t id, event_level_t lvl)
{
    event_t event;
    metric_t metric;

    /* Return Here If Nothing to Do */
    if(lvl < metric_level) return;

    /* Get Metric */
    metric_mut.lock();
    {
        metric = metric_vals[id];
    }
    metric_mut.unlock();

    /* Initialize Log Message */
    event.systime   = TimeLib::gettimems();
    event.tid       = Thread::getId();
    event.id        = metric.id;
    event.parent    = ORIGIN;
    event.flags     = 0;
    event.type      = METRIC;
    event.level     = lvl;

    /* Copy IP Address */
    StringLib::copy(event.ipv4, SockLib::sockipv4(), SockLib::IPV4_STR_LEN);

    /* Copy Name and Attribute */
    StringLib::copy(event.name, metric.name, MAX_NAME_SIZE);
    StringLib::copy(event.attr, metric.attr, MAX_ATTR_SIZE);

    /* Post Metric */
    int attr_size = StringLib::size(metric.attr) + 1;
    sendEvent(&event, attr_size);
}

/*----------------------------------------------------------------------------
 * sendEvent
 *----------------------------------------------------------------------------*/
int EventLib::sendEvent (event_t* event, int attr_size)
{
    return RecordObject::postSerial(outq, IO_CHECK, rec_type, rec_type_size, (unsigned char*)event, offsetof(event_t, attr) + attr_size);
}
