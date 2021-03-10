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
