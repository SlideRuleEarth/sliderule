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
