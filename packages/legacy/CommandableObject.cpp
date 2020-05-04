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

#include "CommandableObject.h"
#include "CommandProcessor.h"
#include "core.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CommandableObject::CommandableObject(CommandProcessor* cmd_proc, const char* obj_name, const char* obj_type):
    commands(MAX_CMD_HASH)
{
    assert(obj_name);
    assert(obj_type);

    /* Initialize Object */
    cmdProc = cmd_proc; // note can be null
    objName = StringLib::duplicate(obj_name);
    objType = StringLib::duplicate(obj_type);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CommandableObject::~CommandableObject(void)
{
    mlog(CRITICAL, "Object %s deleted\n", objName);

    /* Delete Name and Type */
    delete [] objName;
    delete [] objType;
}

/*----------------------------------------------------------------------------
 * getName  -
 *----------------------------------------------------------------------------*/
const char* CommandableObject::getName(void)
{
    return objName;
}

/*----------------------------------------------------------------------------
 * getType  -
 *----------------------------------------------------------------------------*/
const char* CommandableObject::getType(void)
{
    return objType;
}

/*----------------------------------------------------------------------------
 * getProc  -
 *----------------------------------------------------------------------------*/
CommandProcessor* CommandableObject::getProc(void)
{
    return cmdProc;
}

/*----------------------------------------------------------------------------
 * getCommands  -
 *----------------------------------------------------------------------------*/
int CommandableObject::getCommands(char*** cmd_names, char*** cmd_descs)
{
    int numcmds = commands.getKeys(cmd_names);
    *cmd_descs = new char* [numcmds];
    for(int i = 0; i < numcmds; i++)
    {
        obj_cmd_entry_t* cmd = (obj_cmd_entry_t*)commands[(*cmd_names)[i]];
        (*cmd_descs)[i] = StringLib::duplicate(cmd->desc);
    }
    return numcmds;
}

/*----------------------------------------------------------------------------
 * executeCommand  -
 *----------------------------------------------------------------------------*/
int CommandableObject::executeCommand(const char* cmd_name, int argc, char argv[][MAX_CMD_SIZE])
{
    try
    {
        obj_cmd_entry_t* cmd = commands[cmd_name];

        if(cmd->func == NULL)
        {
            mlog(CRITICAL, "No function associated with command %s for object %s\n", cmd_name, objName);
            return VOID_CMD_ERROR;
        }
        else if(cmd->numparms > 0 && cmd->numparms != argc)
        {
            mlog(CRITICAL, "Incorrect number of parameters supplied (%d != %d) to command %s for object %s\n", argc, cmd->numparms, cmd_name, objName);
            return NUM_PARMS_CMD_ERROR;
        }
        else if(abs(cmd->numparms) > argc)
        {
            mlog(CRITICAL, "Not enough parameters supplied (%d < %d) to command %s for object %s\n", argc, cmd->numparms, cmd_name, objName);
            return NUM_PARMS_CMD_ERROR;
        }
        else if(argc > 0 && argv == NULL)
        {
            mlog(CRITICAL, "No parameters supplied when %d parameters expected for command %s for object %s\n", argc, cmd_name, objName);
            return NO_PARMS_CMD_ERROR;
        }
        else
        {
            try
            {
                return (this->*cmd->func)(argc, argv);
            }
            catch(std::exception &e)
            {
                mlog(CRITICAL, "While executing command %s caught unhandled exception %s\n", cmd_name, e.what());
                return STANDARD_CMD_ERROR;
            }
        }
    }
    catch(std::out_of_range& e)
    {
        (void)e;
        mlog(CRITICAL, "Unable to find command %s for object %s\n", cmd_name, objName);
        return UNKNOWN_CMD_ERROR;
    }
}

/*----------------------------------------------------------------------------
 * registerCommand  -
 *----------------------------------------------------------------------------*/
bool CommandableObject::registerCommand(const char* name, CommandableObject::cmdFunc_t func, int numparms, const char* desc)
{
    bool status;

    /* Build Command Entry */
    obj_cmd_entry_t* cmd = new obj_cmd_entry_t(func, numparms, desc);

    /* Register Command */
    status = commands.add(name, cmd);

    /* Handle Success/Failure */
    if(status)
    {
        mlog(INFO, "Object %s registered command: %s\n", objName, name);
    }
    else
    {
        mlog(CRITICAL, "Object %s failed to register command: %s\n", objName, name);
        delete cmd;
    }

    /* Return Status */
    return status;
}
