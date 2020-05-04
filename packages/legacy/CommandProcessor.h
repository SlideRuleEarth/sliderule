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

#ifndef __command_processor__
#define __command_processor__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "Dictionary.h"
#include "CommandableObject.h"
#include "OsApi.h"

/******************************************************************************
 * COMMAND PROCESSOR CLASS
 ******************************************************************************/

class CommandProcessor: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;
        static const char* OBJ_DELIMETER;
        static const char* KEY_DELIMETER;
        static const char* COMMENT;
        static const char* STORE;
        static const char* SELF_KEY;
        static const char* PRIORITY_Q_SUFFIX;
        
        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef CommandableObject* (*newFunc_t) (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            CommandProcessor        (const char* cmdq_name);
                            ~CommandProcessor       (void);

        bool                postCommand             (const char* cmdstr, ...) VARG_CHECK(printf, 2, 3); // "this" is 1
        bool                postPriority            (const char* cmdstr, ...) VARG_CHECK(printf, 2, 3); // "this" is 1
        bool                executeScript           (const char* script_name);
        bool                registerHandler         (const char* handle_name, newFunc_t func, int numparms, const char* desc, bool perm=false);
        bool                registerObject          (const char* obj_name, CommandableObject* obj); // schedules it, and makes it permanent
        bool                deleteObject            (const char* obj_name); // only schedules it
        CommandableObject*  getObject               (const char* obj_name, const char* obj_type); // requires it to be permanent
        const char*         getObjectType           (const char* obj_name);
        int                 setCurrentValue         (const char* obj_name, const char* key, void* data, int size);
        int                 getCurrentValue         (const char* obj_name, const char* key, void* data, int size, int timeout_ms=IO_CHECK, bool with_delete=false);        
        
    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        // Handle Entry //
        struct handle_entry_t 
        {
            char*       name;
            newFunc_t   func;
            int         numparms;
            char*       desc;
            bool        perm;
            
            handle_entry_t(const char* _name, newFunc_t _func, int _numparms, const char* _desc, bool _perm)
                { name = StringLib::duplicate(_name);
                  func = _func;
                  numparms = _numparms;
                  desc = StringLib::duplicate(_desc); 
                  perm = _perm; }
            
            ~handle_entry_t(void)
                { if(name) delete [] name;
                  if(desc) delete [] desc; }
        };

        // Current Value Table Entry //
        struct cvt_entry_t 
        {
            void*   data;
            int     size;
            
            cvt_entry_t(void* _data, int _size)
                { data = new unsigned char [_size];
                  LocalLib::copy(data, _data, _size);
                  size = _size; }
            
            ~cvt_entry_t(void)
                { delete [] (unsigned char*)data; }
        };
        
        // Object Entry //
        struct obj_entry_t 
        {
            CommandableObject*  obj;
            bool                permanent;
        
            obj_entry_t(void) { } // uninitialized entry

            obj_entry_t(CommandableObject* _obj, bool _permanent)
                { obj = _obj;
                  permanent = _permanent; }
        };
        
        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const int                MAX_KEY_NAME = 256;

        MgDictionary<handle_entry_t*>   handlers;

        Subscriber*                     cmdq_subscriber;
        Publisher*                      cmdq_publisher;
        Subscriber*                     priq_subscriber;
        Publisher*                      priq_publisher;

        bool                            procActive;
        Thread*                         procThread;
        long                            executed_commands;
        long                            rejected_commands;

        Dictionary<obj_entry_t>         objects;
        List<obj_entry_t>               lockedObjects;
        MgDictionary<cvt_entry_t*>      currentValueTable;
        Cond                            cvtCond;
             
        double                          stopwatch_time;

        /*--------------------------------------------------------------------
         * Functions
         *--------------------------------------------------------------------*/
  
        static void*    cmdProcThread           (void* parm);
        bool            processCommand          (const char* cmdstr);

        int             helpCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
        int             versionCmd              (int argc, char argv[][MAX_CMD_SIZE]);
        int             quitCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
        int             abortCmd                (int argc, char argv[][MAX_CMD_SIZE]);
        int             newCmd                  (int argc, char argv[][MAX_CMD_SIZE]);
        int             deleteCmd               (int argc, char argv[][MAX_CMD_SIZE]);
        int             permCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
        int             typeCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
        int             registerCmd             (int argc, char argv[][MAX_CMD_SIZE]);
        int             defineCmd               (int argc, char argv[][MAX_CMD_SIZE]);
        int             addFieldCmd             (int argc, char argv[][MAX_CMD_SIZE]);
        int             exportDefinitionCmd     (int argc, char argv[][MAX_CMD_SIZE]);
        int             waitCmd                 (int argc, char argv[][MAX_CMD_SIZE]);
        int             waitOnEmptyCmd          (int argc, char argv[][MAX_CMD_SIZE]);
        int             startStopWatchCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int             displayStopWatchCmd     (int argc, char argv[][MAX_CMD_SIZE]);
        int             logCmdStatsCmd          (int argc, char argv[][MAX_CMD_SIZE]);
        int             executeScriptCmd        (int argc, char argv[][MAX_CMD_SIZE]);
        int             listDevicesCmd          (int argc, char argv[][MAX_CMD_SIZE]);
        int             listMsgQCmd             (int argc, char argv[][MAX_CMD_SIZE]);
        int             qdepthMsgQCmd           (int argc, char argv[][MAX_CMD_SIZE]);
        int             setIOTimeoutCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int             setIOMaxsizeCmd         (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __command_processor__ */
