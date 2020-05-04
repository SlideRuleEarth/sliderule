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

#ifndef __ccsds_msg_processor__
#define __ccsds_msg_processor__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CommandableObject.h"
#include "MsgQ.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS MESSAGE PROCESSOR CLASS
 ******************************************************************************/

class CcsdsMsgProcessor: public CommandableObject
{
    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CcsdsMsgProcessor    (CommandProcessor* cmd_proc, const char* obj_name, const char* _type, const char* inq_name);
        virtual         ~CcsdsMsgProcessor   (void);

        virtual bool    processMsg      (unsigned char* msg, int bytes) = 0;
        virtual bool    initProcessing  (void); // this method and corresponding deinit are used for blocking functions
        virtual bool    deinitProcessing(void); // ...that would be undesirable to put in a constructor / destructor
        virtual bool    handleTimeout   (void);

        bool            isActive        (void);
        bool            isFull          (void);
        void            flush           (void);
        void            stop            (void);
        void            start           (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool            processorActive;
        Thread*         thread;

        Subscriber*     inQ;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*    processorThread (void* parm);
        
        int             drainCmd        (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __ccsds_msg_processor__ */
