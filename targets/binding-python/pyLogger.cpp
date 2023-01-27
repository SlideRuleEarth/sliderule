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

#include <pybind11/pybind11.h>
#include "OsApi.h"
#include "core.h"
#include "pyLogger.h"

/******************************************************************************
 * NAMESPACES
 ******************************************************************************/

namespace py = pybind11;

/******************************************************************************
 * pyLogger Class
 ******************************************************************************/

/*--------------------------------------------------------------------
 * Constructor
 *--------------------------------------------------------------------*/
pyLogger::pyLogger (const long level)
{
    active = true;
    inQ = new Subscriber("eventq");
    pid = new Thread(loggerThread, this);
    EventLib::setLvl(EventLib::LOG, (event_level_t)level);
}

/*--------------------------------------------------------------------
 * Destructor
 *--------------------------------------------------------------------*/
pyLogger::~pyLogger (void)
{
    active = false;
    delete pid;
    delete inQ;
}

/*--------------------------------------------------------------------
 * log
 *--------------------------------------------------------------------*/
const char* pyLogger::log (const std::string msg, const long level)
{
    mlog((event_level_t)level, "%s", msg.c_str());
    return msg.c_str();
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * loggerThread
 *----------------------------------------------------------------------------*/
void* pyLogger::loggerThread(void* parm)
{
    pyLogger* logger = (pyLogger*)parm;

    /* Loop Forever */
    while(logger->active)
    {
        /* Receive Message */
        Subscriber::msgRef_t ref;
        int recv_status = logger->inQ->receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            /* Log Message */
            if(ref.size > 0)
            {
                try
                {
                    RecordInterface record((unsigned char*)ref.data, ref.size);
                    EventLib::event_t* event = (EventLib::event_t*)record.getRecordData();
                    if(event->type == EventLib::LOG)
                    {
                        TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(event->systime);
                        TimeLib::date_t date = TimeLib::gmt2date(gmt);
                        print2term("[%d-%02d-%02dT%02d:%02d:%02dZ] %s\n", date.year, date.month, date.day, gmt.hour, gmt.minute, gmt.second, event->attr);
                    }
                }
                catch (const RunTimeException& e)
                {
                }
            }

            /* Dereference Message */
            logger->inQ->dereference(ref);
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            /* Break Out on Failure */
            print2term("Failed queue receive on %s with error %d", logger->inQ->getName(), recv_status);
            logger->active = false; // breaks out of loop
        }
    }

    return NULL;
}
