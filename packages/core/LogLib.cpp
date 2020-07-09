/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LogLib.h"
#include "TimeLib.h"
#include "StringLib.h"

#include <cstdarg>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

okey_t LogLib::logIdPool;
Ordering<LogLib::log_t>  LogLib::logList;
Mutex LogLib::logMut;
int LogLib::logLvlCnts[RAW + 1];

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void LogLib::init(void)
{
    logIdPool = 0;
    LocalLib::set(logLvlCnts, 0, sizeof(logLvlCnts));
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void LogLib::deinit(void)
{
}

/*----------------------------------------------------------------------------
 * createLog
 *----------------------------------------------------------------------------*/
okey_t LogLib::createLog(log_lvl_t lvl, logFunc_t handler, void* parm)
{
    /* Create New Log Structure */
    log_t new_log;
    new_log.level = lvl;
    new_log.handler = handler;
    new_log.parm = parm;

    /* Add to Log List */
    logMut.lock();
    {
        new_log.id = logIdPool++;
        logList.add(new_log.id, new_log);
    }
    logMut.unlock();

    /* Return Log Id */
    return new_log.id;
}

/*----------------------------------------------------------------------------
 * deleteLog
 *----------------------------------------------------------------------------*/
bool LogLib::deleteLog(okey_t id)
{
    bool status;
    logMut.lock();
    {
        status = logList.remove(id);
    }
    logMut.unlock();
    return status;
}

/*----------------------------------------------------------------------------
 * setLevel
 *----------------------------------------------------------------------------*/
bool LogLib::setLevel(okey_t id, log_lvl_t lvl)
{
    bool status;
    logMut.lock();
    {
        try
        {
            log_t& existing_log = logList.get(id);
            existing_log.level = lvl;
            status = true;
        }
        catch(std::out_of_range& e)
        {
            (void)e;
            status = false;
        }
    }
    logMut.unlock();
    return status;
}

/*----------------------------------------------------------------------------
 * getLevel
 *----------------------------------------------------------------------------*/
log_lvl_t LogLib::getLevel(okey_t id)
{
    log_lvl_t lvl;
    logMut.lock();
    {
        try
        {
            log_t& existing_log = logList.get(id);
            lvl = existing_log.level;
        }
        catch(std::out_of_range& e)
        {
            (void)e;
            lvl = IGNORE;
        }
    }
    logMut.unlock();
    return lvl;
}

/*----------------------------------------------------------------------------
 * getLvlCnts
 *----------------------------------------------------------------------------*/
int LogLib::getLvlCnts(log_lvl_t lvl)
{
    if((int)lvl >= 0 && (int)lvl <= (int)RAW)
    {
        return logLvlCnts[lvl];
    }

    return -1;
}

/*----------------------------------------------------------------------------
 * str2lvl
 *----------------------------------------------------------------------------*/
bool LogLib::str2lvl(const char* str, log_lvl_t* lvl)
{
    log_lvl_t clvl;
    if( StringLib::match(str, "RAW") || StringLib::match(str, "raw") || StringLib::match(str, "Raw"))
    {
        clvl = RAW;
    }
    else if( StringLib::match(str, "IGNORE") || StringLib::match(str, "ignore") || StringLib::match(str, "Ignore"))
    {
        clvl = IGNORE;
    }
    else if( StringLib::match(str, "DEBUG") || StringLib::match(str, "debug") || StringLib::match(str, "Debug"))
    {
        clvl = DEBUG;
    }
    else if( StringLib::match(str, "INFO") || StringLib::match(str, "info") || StringLib::match(str, "Info"))
    {
        clvl = INFO;
    }
    else if( StringLib::match(str, "WARNING") || StringLib::match(str, "warning") || StringLib::match(str, "Warning"))
    {
        clvl = WARNING;
    }
    else if( StringLib::match(str, "ERROR") || StringLib::match(str, "error") || StringLib::match(str, "Error"))
    {
        clvl = ERROR;
    }
    else if( StringLib::match(str, "CRITICAL") || StringLib::match(str, "critical") || StringLib::match(str, "Critical"))
    {
        clvl = CRITICAL;
    }
    else
    {
        return false;
    }

    *lvl = clvl;
    return true;
}

/*----------------------------------------------------------------------------
 * logMsg
 *----------------------------------------------------------------------------*/
void LogLib::logMsg(const char* file_name, unsigned int line_number, log_lvl_t lvl, const char* format_string, ...)
{
    bool work_needed = false;

    logMut.lock();
    {
        /* Count Message */
        if((int)lvl >= 0 && (int)lvl <= (int)RAW)
        {
            logLvlCnts[lvl]++;
        }

        /* Check if any work needs to be done */
        log_t check_log;
        okey_t key = logList.first(&check_log);
        while(key != logList.INVALID_KEY)
        {
            if(check_log.level <= lvl)
            {
                work_needed = true;
                break;
            }
            key = logList.next(&check_log);
        }
    }
    logMut.unlock();

    /* Return Here If Nothing to Do */
    if (!work_needed) return;

    /* Build Formatted Log Entry String */
    char formatted_string[MAX_LOG_ENTRY_SIZE];
    va_list args;
    va_start(args, format_string);
    int vlen = vsnprintf(formatted_string, MAX_LOG_ENTRY_SIZE - 1, format_string, args);
    int msglen = MIN(vlen, MAX_LOG_ENTRY_SIZE - 1);
    va_end(args);
    if (msglen < 0) return; // nothing to do
    formatted_string[msglen] = '\0';

    /* Build Time Stamp String */
    #define TIME_STR_SIZE 25
    char timestr[TIME_STR_SIZE];
    TimeLib::gmt_time_t timeinfo = TimeLib::gettime();
    StringLib::format(timestr, TIME_STR_SIZE, "%d:%d:%d:%d:%d", timeinfo.year, timeinfo.day, timeinfo.hour, timeinfo.minute, timeinfo.second);

    /* Adjust Filename to Exclude Path */
    const char* last_path_delimeter = StringLib::find(file_name, PATH_DELIMETER, false);
    const char* file_name_only = last_path_delimeter ? last_path_delimeter + 1 : file_name;

    /* Build Log Entry Message */
    char entry_log_msg[MAX_LOG_ENTRY_SIZE];
    char* lvlstr = (char*)"";
    switch(lvl)
    {
        case IGNORE:    lvlstr = (char*)"IGNORE";      break;
        case DEBUG:     lvlstr = (char*)"DEBUG";       break;
        case INFO:      lvlstr = (char*)"INFO";        break;
        case WARNING:   lvlstr = (char*)"WARNING";     break;
        case ERROR:     lvlstr = (char*)"ERROR";       break;
        case CRITICAL:  lvlstr = (char*)"CRITICAL";    break;
        case RAW:       lvlstr = NULL;                 break;
    }

    if(lvlstr != NULL)
    {
        if(formatted_string[msglen - 1] == '\n')
        {
            StringLib::format(entry_log_msg, MAX_LOG_ENTRY_SIZE, "%s:%s:%d:%s: %s", timestr, file_name_only, line_number, lvlstr, formatted_string);
        }
        else
        {
            StringLib::format(entry_log_msg, MAX_LOG_ENTRY_SIZE, "%s:%s:%d:%s: %s\n", timestr, file_name_only, line_number, lvlstr, formatted_string);
        }
    }
    else
    {
        StringLib::format(entry_log_msg, MAX_LOG_ENTRY_SIZE, "%s", formatted_string);
    }

    logMut.lock();
    {
        /* Call All Log Handlers */
        int size = StringLib::size(entry_log_msg, MAX_LOG_ENTRY_SIZE - 1) + 1;
        log_t cur_log;
        okey_t key = logList.first(&cur_log);
        while(key != logList.INVALID_KEY)
        {
            if(cur_log.level <= lvl)
            {
                cur_log.handler(entry_log_msg, size, cur_log.parm);
            }
            key = logList.next(&cur_log);
        }
    }
    logMut.unlock();
}
