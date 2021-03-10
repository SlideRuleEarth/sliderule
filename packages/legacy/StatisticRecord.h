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

#ifndef __statistic_record__
#define __statistic_record__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CommandableObject.h"
#include "CommandProcessor.h"
#include "core.h"

/******************************************************************************
 * STATISTIC RECORD TEMPLATE
 ******************************************************************************/
/*
 * This is a dangerous class used to get around a less than ideal design.
 * Since StatisticRecord is a CommandableObject and a RecordObject
 * it wants to behave as both.  To be a record object it needs a public
 * constructor and direct access methods.  To be a commandable object
 * it should have a private constructor and only interact through
 * the dispatching of commands.  To make this class workable as both,
 * the controlling code must either treat the object as a record object
 * only and never register it, or register it as commandable, and at that
 * point, realize the implications of concurrent access through
 * dispatched commands.
 */
template <class T>
class StatisticRecord: public CommandableObject, public RecordObject
{
    public:

        /*--------------------------------------------------------------------
         * Typedef
         *--------------------------------------------------------------------*/

        typedef enum {
            CLEAR_ONCE,
            CLEAR_ALWAYS,
            CLEAR_NEVER,
            CLEAR_UNKNOWN
        } clear_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;
        static const char* CURRENT_VALUE_KEY;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        T* rec;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual bool    post                    (void);
        virtual void    prepost                 (void);
        void            lock                    (void);
        void            unlock                  (void);
        void            setClear                (clear_t clear);

        static clear_t  str2clear               (const char* str);

    protected:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        clear_t     statClear;
        Mutex       statMut;
        Publisher*  outQ;

        bool        telemetryActive;
        int         telemetryWaitSeconds;
        Thread*     telemetryPid;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        StatisticRecord  (CommandProcessor* cmd_proc, const char* cmd_name, const char* rec_name, bool automatic_post=true);
                        ~StatisticRecord (void);

        static void*    telemetryThread     (void* parm);
        bool            stopTelemetry       (void);

        int             attachCmd           (int argc, char argv[][MAX_CMD_SIZE]);
        int             clearCmd            (int argc, char argv[][MAX_CMD_SIZE]);
        int             setRateCmd          (int argc, char argv[][MAX_CMD_SIZE]);

        static void     freePost            (void* obj, void* parm);
};

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

template <class T>
const char* StatisticRecord<T>::TYPE = "StatisticRecord";

template <class T>
const char* StatisticRecord<T>::CURRENT_VALUE_KEY = "cv";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *
 *   Notes: By default all StatisticRecord objects ARE permanent objects,
 *          meaning it is expected that the calling code will not deallocate them.
 *----------------------------------------------------------------------------*/
template <class T>
StatisticRecord<T>::StatisticRecord(CommandProcessor* cmd_proc, const char* cmd_name, const char* rec_name, bool automatic_post):
    CommandableObject(cmd_proc, cmd_name, StatisticRecord<T>::TYPE),
    RecordObject(rec_name)
{
    /* Set Record to Data Allocated by RecordObject */
    rec = (T*)recordData;

    /* Initialize Parameters */
    statClear = CLEAR_NEVER;
    outQ = NULL;

    /* Start Telemetry Thread */
    telemetryActive = automatic_post;
    telemetryWaitSeconds = 1; // one second wait
    telemetryPid = new Thread(telemetryThread, this);

    /* Register Commands */
    registerCommand("ATTACH",    (cmdFunc_t)&StatisticRecord<T>::attachCmd,  1, "<qname>");
    registerCommand("CLEAR",     (cmdFunc_t)&StatisticRecord<T>::clearCmd,   1, "<ONCE|ALWAYS|NEVER>");
    registerCommand("SET_RATE",  (cmdFunc_t)&StatisticRecord<T>::setRateCmd, 1, "<wait time in seconds>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
template <class T>
StatisticRecord<T>::~StatisticRecord()
{
    stopTelemetry();
    if(outQ != NULL) delete outQ;
}

/*----------------------------------------------------------------------------
 * post  -
 *----------------------------------------------------------------------------*/
template <class T>
void StatisticRecord<T>::prepost(void)
{
    // do nothing by default
}

/*----------------------------------------------------------------------------
 * post  -
 *----------------------------------------------------------------------------*/
template <class T>
bool StatisticRecord<T>::post(void)
{
    unsigned char* buffer = NULL; // pointer to serialized buffer
    int size = 0; // size of serialized buffer

    lock();
    {
        /* Set Current Value Table */
        if(cmdProc) cmdProc->setCurrentValue(getName(), CURRENT_VALUE_KEY, rec, sizeof(T));

        /* Serialize Statistic */
        if(outQ != NULL)
        {
            size = serialize(&buffer, RecordObject::ALLOCATE);
        }

        /* Clear Statistic */
        if(statClear != CLEAR_NEVER)
        {
            LocalLib::set(rec, 0, sizeof(T));
        }

        if(statClear == CLEAR_ONCE)
        {
            statClear = CLEAR_NEVER;
        }
    }
    unlock();

    /* Post Statistic */
    bool post_status = true;
    if(buffer != NULL && size > 0)
    {
        /* Post to Queue */
        if(outQ->postRef(buffer, size, telemetryWaitSeconds * 1000) <= 0)
        {
            delete [] buffer;
            post_status = false;
        }
    }

    return post_status;
}

/*----------------------------------------------------------------------------
 * lock  -
 *----------------------------------------------------------------------------*/
template <class T>
void StatisticRecord<T>::lock(void)
{
    statMut.lock();
}

/*----------------------------------------------------------------------------
 * unlock  -
 *----------------------------------------------------------------------------*/
template <class T>
void StatisticRecord<T>::unlock(void)
{
    statMut.unlock();
}

/*----------------------------------------------------------------------------
 * setClear  -
 *----------------------------------------------------------------------------*/
template <class T>
void StatisticRecord<T>::setClear(clear_t clear)
{
    statClear = clear;
}

/*----------------------------------------------------------------------------
 * str2clear
 *----------------------------------------------------------------------------*/
template <class T>
typename StatisticRecord<T>::clear_t StatisticRecord<T>::str2clear(const char* str)
{
    if(StringLib::match(str, "ONCE") || StringLib::match(str, "once"))
    {
        return CLEAR_ONCE;
    }
    else if(StringLib::match(str, "ALWAYS") || StringLib::match(str, "always"))
    {
        return CLEAR_ALWAYS;
    }
    else if(StringLib::match(str, "NEVER") || StringLib::match(str, "never"))
    {
        return CLEAR_NEVER;
    }
    else
    {
        return CLEAR_UNKNOWN;
    }
}

/*----------------------------------------------------------------------------
 * telemetryThread
 *----------------------------------------------------------------------------*/
template <class T>
void* StatisticRecord<T>::telemetryThread(void* parm)
{
    StatisticRecord<T>* procstat = (StatisticRecord<T>*)parm;

    int wait_counter = procstat->telemetryWaitSeconds;

    while(procstat->telemetryActive)
    {
        LocalLib::sleep(1);
        if(procstat->telemetryWaitSeconds != 0)
        {
            if(--wait_counter == 0)
            {
                wait_counter = procstat->telemetryWaitSeconds;
                procstat->prepost();
                bool status = procstat->post();
                if(status != true)
                {
                    mlog(DEBUG, "Unable to post %s telemetry!\n", procstat->getName());
                }
            }
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * stopTelemetry
 *----------------------------------------------------------------------------*/
template <class T>
bool StatisticRecord<T>::stopTelemetry(void)
{
    telemetryActive = false;
    if(telemetryPid) delete telemetryPid;
    telemetryPid = NULL;
    return true;
}

/*----------------------------------------------------------------------------
 * attachCmd
 *----------------------------------------------------------------------------*/
template <class T>
int StatisticRecord<T>::attachCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    if(outQ != NULL)
    {
        mlog(CRITICAL, "Statistic output already attached to %s\n", outQ->getName());
        return -1;
    }

    const char* name = StringLib::checkNullStr(argv[0]);
    if(name)
    {
        outQ = new Publisher(name, freePost);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * attachCmd
 *----------------------------------------------------------------------------*/
template <class T>
int StatisticRecord<T>::clearCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    clear_t clear = StatisticRecord<T>::str2clear(argv[0]);
    if(clear == CLEAR_UNKNOWN)
    {
        mlog(CRITICAL, "Invalid parameter passed to clear command: %s\n", argv[0]);
        return -1;
    }

    statClear = clear;
    post(); // sends what is currently in stats
    post(); // sends all zeros

    return 0;
}

/*----------------------------------------------------------------------------
 * setRateCmd
 *----------------------------------------------------------------------------*/
template <class T>
int StatisticRecord<T>::setRateCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* wait_str = argv[0];

    long wait_seconds = 0;
    if(!StringLib::str2long(wait_str, &wait_seconds))
    {
        mlog(CRITICAL, "Invalid wait time supplied: %s\n", wait_str);
        return -1;
    }
    else if(wait_seconds <= 0)
    {
        mlog(CRITICAL, "Wait time must be greater than zero: %ld\n", wait_seconds);
        return -1;
    }
    else
    {
        telemetryWaitSeconds = wait_seconds;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * freePost
 *----------------------------------------------------------------------------*/
template <class T>
void StatisticRecord<T>::freePost(void* obj, void* parm)
{
    (void)parm;
    unsigned char* mem = (unsigned char*)obj;
    delete [] mem;
}

#endif  /* __statistics_record__ */
