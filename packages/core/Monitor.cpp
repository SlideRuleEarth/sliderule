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

#include "Monitor.h"
#include "EventLib.h"
#include "TimeLib.h"
#include "RecordObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Monitor::OBJECT_TYPE = "Monitor";
const char* Monitor::LUA_META_NAME = "Monitor";
const struct luaL_Reg Monitor::LUA_META_TABLE[] = {
    {"config",      luaConfig},
    {"tail",        luaTail},
    {"cat",         luaCat},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create([<type mask>], [<level>], [<output format>], [<eventq name>])
 *----------------------------------------------------------------------------*/
int Monitor::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parmeters */
        const uint8_t type_mask = (uint8_t)getLuaInteger(L, 1, true, (long)EventLib::LOG);
        const event_level_t level = (event_level_t)getLuaInteger(L, 2, true, CRITICAL);
        const format_t format = (format_t)getLuaInteger(L, 3, true, RECORD);
        const char* eventq_name = getLuaString(L, 4, true, EVENTQ);

        /* Return Dispatch Object */
        return createLuaObject(L, new Monitor(L, type_mask, level, format, eventq_name));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processEvent
 *----------------------------------------------------------------------------*/
void Monitor::processEvent(const unsigned char* event_buf_ptr, int event_size)
{
    fwrite(event_buf_ptr, 1, event_size, stdout);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Monitor::Monitor(lua_State* L, uint8_t type_mask, event_level_t level, format_t format, const char* eventq_name):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
    /* Initialize Event Monitor Data */
    eventTypeMask   = type_mask;
    eventLevel      = level;
    outputFormat    = format;
    eventTailArray  = NULL;
    eventTailSize   = 0;
    eventTailIndex  = 0;

    /* Initialize Event Monitor Thread*/
    active = true;
    inQ = new Subscriber(eventq_name);
    pid = new Thread(monitorThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Monitor::~Monitor(void)
{
    active = false;
    delete pid;
    delete inQ;
    delete [] eventTailArray;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processEvent
 *----------------------------------------------------------------------------*/
void* Monitor::monitorThread (void* parm)
{
    Monitor* monitor = static_cast<Monitor*>(parm);

    int event_size;
    char event_buffer[MAX_EVENT_SIZE];
    unsigned char* event_buf_ptr = reinterpret_cast<unsigned char*>(&event_buffer[0]);

    /* Loop Forever */
    while(monitor->active)
    {
        /* Receive Message */
        Subscriber::msgRef_t ref;
        const int recv_status = monitor->inQ->receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            unsigned char* msg = reinterpret_cast<unsigned char*>(ref.data);
            const int len = ref.size;
            if(len > 0)
            {
                try
                {
                    /* Process Event Record */
                    RecordInterface record(msg, len);
                    if(StringLib::match(record.getRecordType(), EventLib::eventRecType))
                    {
                        /* Pull Out Log Message */
                        EventLib::event_t* event = reinterpret_cast<EventLib::event_t*>(record.getRecordData());

                        /* Filter Events */
                        if( ((event->type & monitor->eventTypeMask) == 0) ||
                            (event->level < monitor->eventLevel) )
                        {
                            throw RunTimeException(DEBUG, RTE_INFO, "event <%d.%d> filtered", event->type, event->level);
                        }

                        /* Format Event */
                        if(monitor->outputFormat == RECORD)
                        {
                            event_size = record.serialize(&event_buf_ptr, RecordObject::REFERENCE);
                            event_size = MIN(event_size, MAX_EVENT_SIZE);
                        }
                        else if(monitor->outputFormat == CLOUD)
                        {
                            event_size = cloudOutput(event, event_buffer);
                        }
                        else if(monitor->outputFormat == TEXT)
                        {
                            event_size = textOutput(event, event_buffer);
                        }
                        else if(monitor->outputFormat == JSON)
                        {
                            event_size = jsonOutput(event, event_buffer);
                        }
                        else // unsupported format
                        {
                            event_size = 0;
                        }

                        /* (Optionally) Tail Event */
                        if(monitor->eventTailArray && monitor->eventTailSize > 0)
                        {
                            memcpy(&monitor->eventTailArray[monitor->eventTailIndex * MAX_EVENT_SIZE], event_buf_ptr, event_size);
                            monitor->eventTailIndex = (monitor->eventTailIndex + 1) % monitor->eventTailSize;
                        }

                        /* Child-Class Process Event */
                        monitor->processEvent(event_buf_ptr, event_size);
                    }
                }
                catch (const RunTimeException& e)
                {
                    // pass silently
                }
            }
            else
            {
                /* Terminating Message */
                mlog(DEBUG, "Terminator received on %s, exiting monitor", monitor->inQ->getName());
                monitor->active = false; // breaks out of loop
            }

            /* Dereference Message */
            monitor->inQ->dereference(ref);
        }
        else
        {
            /* Break Out on Failure */
            mlog(CRITICAL, "Failed queue receive on %s with error %d", monitor->inQ->getName(), recv_status);
            monitor->active = false; // breaks out of loop
        }
    }

    /* Signal Completion */
    monitor->signalComplete();
    return NULL;
}

/*----------------------------------------------------------------------------
 * textOutput
 *----------------------------------------------------------------------------*/
int Monitor::textOutput (EventLib::event_t* event, char* event_buffer)
{
    char* msg = event_buffer;

    /* Populate Prefix */
    const TimeLib::gmt_time_t timeinfo = TimeLib::gps2gmttime(event->systime);
    const TimeLib::date_t dateinfo = TimeLib::gmt2date(timeinfo);
    const double seconds = (double)timeinfo.second + ((double)timeinfo.millisecond / 1000.0);
    msg += StringLib::formats(msg, MAX_EVENT_SIZE, "%04d-%02d-%02dT%02d:%02d:%.03lfZ %s:%s:%s ",
        timeinfo.year, dateinfo.month, dateinfo.day, timeinfo.hour, timeinfo.minute, seconds,
        event->ipv4, EventLib::lvl2str((event_level_t)event->level), event->name);

    /* Populate Message */
    msg += StringLib::formats(msg, MAX_EVENT_SIZE - (msg - event_buffer), "%s\n", event->attr);

    /* Return Size of Message */
    return msg - event_buffer + 1;
}

/*----------------------------------------------------------------------------
 * jsonOutput
 *----------------------------------------------------------------------------*/
int Monitor::jsonOutput (EventLib::event_t* event, char* event_buffer)
{
    char* msg = event_buffer;

    /* Populate Message */
    if(event->attr[0] == '{')
    {
        /* Attribute String with No Quotes */
        msg += StringLib::formats(msg, MAX_EVENT_SIZE,
            "{\"systime\":%ld,\"ipv4\":\"%s\",\"flags\":%d,\"type\":\"%s\",\"level\":\"%s\",\"tid\":%ld,\"id\":%ld,\"parent\":%ld,\"name\":\"%s\",\"attr\":%s}\n",
            event->systime, event->ipv4, event->flags,
            EventLib::type2str((EventLib::type_t)event->type), EventLib::lvl2str((event_level_t)event->level),
            event->tid, (long)event->id, (long)event->parent, event->name, event->attr);
    }
    else
    {
        /* Attribute String Quoted */
        msg += StringLib::formats(msg, MAX_EVENT_SIZE,
            "{\"systime\":%ld,\"ipv4\":\"%s\",\"flags\":%d,\"type\":\"%s\",\"level\":\"%s\",\"tid\":%ld,\"id\":%ld,\"parent\":%ld,\"name\":\"%s\",\"attr\":\"%s\"}\n",
            event->systime, event->ipv4, event->flags,
            EventLib::type2str((EventLib::type_t)event->type), EventLib::lvl2str((event_level_t)event->level),
            event->tid, (long)event->id, (long)event->parent, event->name, event->attr);
    }

    /* Return Size of Message */
    return msg - event_buffer + 1;
}

/*----------------------------------------------------------------------------
 * cloudOutput
 *----------------------------------------------------------------------------*/
int Monitor::cloudOutput (EventLib::event_t* event, char* event_buffer)
{
    char* msg = event_buffer;

    /* Populate Message */
    msg += StringLib::formats(msg, MAX_EVENT_SIZE, "ip=%s level=%s caller=%s msg=\"%s\"\n",
        event->ipv4, EventLib::lvl2str_lc((event_level_t)event->level), event->name, event->attr);

    /* Return Size of Message */
    return msg - event_buffer + 1;
}

/*----------------------------------------------------------------------------
 * luaConfig - :config([<type mask>], [<level>]) --> type mask, level, status
 *----------------------------------------------------------------------------*/
int Monitor::luaConfig (lua_State* L)
{
    bool status = false;
    int num_ret = 1;
    bool provided = false;

    try
    {
        /* Get Self */
        Monitor* lua_obj = dynamic_cast<Monitor*>(getLuaSelf(L, 1));

        /* Set Type Mask */
        const uint8_t type_mask = getLuaInteger(L, 2, true, 0, &provided);
        if(provided) lua_obj->eventTypeMask = type_mask;

        /* Set Level */
        const event_level_t level = (event_level_t)getLuaInteger(L, 3, true, 0, &provided);
        if(provided) lua_obj->eventLevel = level;

        /* Set Return Values */
        lua_pushinteger(L, (int)lua_obj->eventTypeMask);
        num_ret++;
        lua_pushinteger(L, (int)lua_obj->eventLevel);
        num_ret++;

        /* Set return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error configuring monitor: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaTail - :tail(<size>)
 *
 *  Note: NOT thread safe; must be called prior to attaching monitor to dispatch
 *----------------------------------------------------------------------------*/
int Monitor::luaTail (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        Monitor* lua_obj = dynamic_cast<Monitor*>(getLuaSelf(L, 1));

        /* Get Tail Size */
        const int tail_size = getLuaInteger(L, 2);
        if(tail_size <= 0 || tail_size > MAX_TAIL_SIZE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid tail size: %d", tail_size);
        }

        /* Check if Tail Exists */
        if(lua_obj->eventTailArray)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Event tail already exists");
        }

        /* Create Event Tail */
        char* event_tail = new char [tail_size * MAX_EVENT_SIZE];
        memset(event_tail, 0, tail_size * MAX_EVENT_SIZE);
        lua_obj->eventTailSize = tail_size;
        lua_obj->eventTailArray = event_tail;

        /* Set return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating tail: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaCat - :cat(<mode>)
 *----------------------------------------------------------------------------*/
int Monitor::luaCat (lua_State* L)
{
    bool status = false;
    Publisher* outq = NULL;
    int num_ret = 1;

    try
    {
        /* Get Self */
        Monitor* lua_obj = dynamic_cast<Monitor*>(getLuaSelf(L, 1));

        /* Get Mode */
        const cat_mode_t mode = (cat_mode_t)getLuaInteger(L, 2, true, TERM);

        /* Check if Tail Exists */
        if(!lua_obj->eventTailArray)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Event tail does not exists");
        }

        /* Setup Cat */
        if(mode == TERM)
        {
            // do nothing
        }
        else if(mode == LOCAL)
        {
            lua_newtable(L);
            num_ret = 2;
        }
        else if(mode == MSGQ)
        {
            const char* outq_name = getLuaString(L, 3);
            outq = new Publisher(outq_name);
        }

        /* Cat Event Messages */
        int msg_index = 0;
        const int start = lua_obj->eventTailIndex;
        for(int i = 0; i < lua_obj->eventTailSize; i++)
        {
            const int index = (start + i) % lua_obj->eventTailSize;
            const char* event_msg = static_cast<const char*>(&lua_obj->eventTailArray[index * MAX_EVENT_SIZE]);
            if(event_msg[0] != '\0')
            {
                msg_index++;
                if(mode == TERM)
                {
                    print2term("%s", event_msg);
                }
                else if(mode == LOCAL)
                {
                    lua_pushstring(L, event_msg);
                    lua_rawseti(L, -2, msg_index);
                }
                else if(mode == MSGQ)
                {
                    const int msg_size = StringLib::nsize(event_msg, MAX_EVENT_SIZE - 1) + 1;
                    outq->postCopy(event_msg, msg_size, IO_CHECK);
                }
            }
        }

        /* Cleanup Cat */
        if(mode == TERM)
        {
            // do nothing
        }
        else if(mode == LOCAL)
        {
            // do nothing
        }
        else if(mode == MSGQ)
        {
            delete outq;
        }

        /* Set return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error concatenating tail: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}
