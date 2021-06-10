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

const char* Monitor::LuaMetaName = "Monitor";
const struct luaL_Reg Monitor::LuaMetaTable[] = {
    {"config",      luaConfig},
    {"tail",        luaTail},
    {"cat",         luaCat},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create([<type mask>], [<level>], [<output format>], [<outputq>])
 *----------------------------------------------------------------------------*/
int Monitor::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parmeters */
        uint8_t type_mask = (uint8_t)getLuaInteger(L, 1, true, (long)EventLib::LOG);
        event_level_t level = (event_level_t)getLuaInteger(L, 2, true, CRITICAL);
        format_t format = (format_t)getLuaInteger(L, 3, true, RECORD);
        const char* outq_name = getLuaString(L, 4, true, NULL);

        /* Return Dispatch Object */
        return createLuaObject(L, new Monitor(L, type_mask, level, format, outq_name));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Monitor::Monitor(lua_State* L, uint8_t type_mask, event_level_t level, format_t format, const char* outq_name):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    /* Initialize Event Monitor */
    eventTypeMask   = type_mask;
    eventLevel      = level;
    outputFormat    = format;
    eventTailArray  = NULL;
    eventTailSize   = 0;
    eventTailIndex  = 0;

    /* Initialize Output Q */
    if(outq_name)   outQ = new Publisher(outq_name);
    else            outQ = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Monitor::~Monitor(void)
{
    if(outQ) delete outQ;
    if(eventTailArray) delete [] eventTailArray;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool Monitor::processRecord (RecordObject* record, okey_t key)
{
    (void)key;

    int event_size;
    char event_buffer[MAX_EVENT_SIZE];    

    /* Pull Out Log Message */
    EventLib::event_t* event = (EventLib::event_t*)record->getRecordData();

    /* Filter Events */
    if( ((event->type & eventTypeMask) == 0) ||
        (event->level < eventLevel) )
    {
        return true;
    }

    /* Process Event */
    if(outputFormat == RECORD)
    {
        /* Post Event as Record */
        unsigned char* buffer; // reference to serial buffer
        int size = record->serialize(&buffer, RecordObject::REFERENCE);
        if(outQ) outQ->postCopy(buffer, size, IO_CHECK);
    }
    else
    {
        /* Format Event */
        if(outputFormat == CLOUD)
        {
            event_size = cloudOutput(event, event_buffer); 
        }
        else if(outputFormat == TEXT)
        {
            event_size = textOutput(event, event_buffer); 
        }
        else if(outputFormat == JSON)
        {
            event_size = jsonOutput(event, event_buffer); 
        }
        else
        {
            return false;
        }

        /* Post Event */
        if(outQ) outQ->postCopy(event_buffer, event_size, IO_CHECK);
        else     fwrite(event_buffer, 1, event_size, stdout);

        /* (Optionally) Tail Event */
        if(eventTailArray)
        {
            LocalLib::copy(&eventTailArray[eventTailIndex * MAX_EVENT_SIZE], event_buffer, event_size);
            eventTailIndex = (eventTailIndex + 1) % eventTailSize;
        }
    }

    /* Return Success */
    return true;
}

/*----------------------------------------------------------------------------
 * textOutput
 *----------------------------------------------------------------------------*/
int Monitor::textOutput (EventLib::event_t* event, char* event_buffer)
{
    char* msg = event_buffer;

    /* Populate Prefix */
    TimeLib::gmt_time_t timeinfo = TimeLib::gps2gmttime(event->systime);
    TimeLib::date_t dateinfo = TimeLib::gmt2date(timeinfo);
    double seconds = (double)timeinfo.second + ((double)timeinfo.millisecond / 1000.0);
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
    if(event->attr && event->attr[0] == '{')
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
    return msg - event_buffer + 1;;
}

/*----------------------------------------------------------------------------
 * cloudOutput
 *----------------------------------------------------------------------------*/
int Monitor::cloudOutput (EventLib::event_t* event, char* event_buffer)
{
    /* Populate Message */
    int msg_len = StringLib::formats(event_buffer, MAX_EVENT_SIZE, "%s:%s %s\n", 
                                    EventLib::lvl2str((event_level_t)event->level), event->name, event->attr);

    /* Return Size of Message */
    return msg_len + 1;
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
        Monitor* lua_obj = (Monitor*)getLuaSelf(L, 1);

        /* Set Type Mask */
        uint8_t type_mask = getLuaInteger(L, 2, true, 0, &provided);
        if(provided) lua_obj->eventTypeMask = type_mask;

        /* Set Level */
        event_level_t level = (event_level_t)getLuaInteger(L, 3, true, 0, &provided);
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
        Monitor* lua_obj = (Monitor*)getLuaSelf(L, 1);

        /* Get Tail Size */
        int tail_size = getLuaInteger(L, 2);
        if(tail_size <= 0 || tail_size > MAX_TAIL_SIZE)
        {
            throw RunTimeException(CRITICAL, "Invalid tail size: %d", tail_size);
        }

        /* Check if Tail Exists */
        if(lua_obj->eventTailArray)
        {
            throw RunTimeException(CRITICAL, "Event tail already exists");
        }

        /* Create Event Tail */
        char* event_tail = new char [tail_size * MAX_EVENT_SIZE];
        LocalLib::set(event_tail, 0, tail_size * MAX_EVENT_SIZE);
        lua_obj->eventTailArray = event_tail;
        lua_obj->eventTailSize = tail_size;

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
        Monitor* lua_obj = (Monitor*)getLuaSelf(L, 1);

        /* Get Mode */
        cat_mode_t mode = (cat_mode_t)getLuaInteger(L, 2, true, TERM);

        /* Check if Tail Exists */
        if(!lua_obj->eventTailArray)
        {
            throw RunTimeException(CRITICAL, "Event tail does not exists");
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
        int start = lua_obj->eventTailIndex;
        for(int i = 0; i < lua_obj->eventTailSize; i++)
        {
            int index = (start + i) % lua_obj->eventTailSize;
            const char* event_msg = (const char*)&lua_obj->eventTailArray[index * MAX_EVENT_SIZE];
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
                    int msg_size = StringLib::size(event_msg, MAX_EVENT_SIZE - 1) + 1;
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
