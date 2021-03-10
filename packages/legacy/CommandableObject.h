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

#ifndef __commandable_object__
#define __commandable_object__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "List.h"
#include "Dictionary.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "StringLib.h"

/******************************************************************************
 * FORWARD DECLARATIONS
 ******************************************************************************/

class CommandProcessor;

/******************************************************************************
 * COMMANDABLE OBJECT CLASS
 ******************************************************************************/

class CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_CMD_PARAMETERS = 63;
        static const int MAX_CMD_SIZE = MAX_STR_SIZE;
        static const int MAX_CMD_HASH = 32;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef int (CommandableObject::*cmdFunc_t) (int argc, char argv[][MAX_CMD_SIZE]);

        typedef enum {
            STANDARD_CMD_SUCCESS    =  0,   // by convention returned by class command methods on success
            STANDARD_CMD_ERROR      = -1,   // by convention returned by class command methods on error
            UNKNOWN_CMD_ERROR       = -2,
            VOID_CMD_ERROR          = -3,
            NUM_PARMS_CMD_ERROR     = -4,
            NO_PARMS_CMD_ERROR      = -5,
            OBJ_NOT_FOUND_CMD_ERROR = -6,
            CMD_VERIFY_ERROR        = -7
        } cmdError_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            CommandableObject   (CommandProcessor* cmd_proc, const char* obj_name, const char* obj_type);
        virtual             ~CommandableObject  (void);

        const char*         getName             (void);
        const char*         getType             (void);
        CommandProcessor*   getProc             (void);
        int                 getCommands         (char*** cmd_names, char*** cmd_descs);
        int                 executeCommand      (const char* cmd_name, int argc, char argv[][MAX_CMD_SIZE]);
        bool                registerCommand     (const char* name, cmdFunc_t func, int numparms, const char* desc);
 
    protected:
        
        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/
        
        struct obj_cmd_entry_t 
        {
            cmdFunc_t       func;
            int             numparms;
            const char*     desc;

            obj_cmd_entry_t(cmdFunc_t _func, int _numparms, const char* _desc):
                func(_func), numparms(_numparms) 
                { desc = StringLib::duplicate(_desc); }

            ~obj_cmd_entry_t(void) 
                { if(desc) delete [] desc; }
        };
                
        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/
        
        const char*                     objName;
        const char*                     objType;
        MgDictionary<obj_cmd_entry_t*>  commands;
        CommandProcessor*               cmdProc;   
};

#endif  /* __commandable_object__ */
