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
    mlog(INFO, "Object %s deleted", objName);

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
            mlog(CRITICAL, "No function associated with command %s for object %s", cmd_name, objName);
            return VOID_CMD_ERROR;
        }
        else if(cmd->numparms > 0 && cmd->numparms != argc)
        {
            mlog(CRITICAL, "Incorrect number of parameters supplied (%d != %d) to command %s for object %s", argc, cmd->numparms, cmd_name, objName);
            return NUM_PARMS_CMD_ERROR;
        }
        else if(abs(cmd->numparms) > argc)
        {
            mlog(CRITICAL, "Not enough parameters supplied (%d < %d) to command %s for object %s", argc, cmd->numparms, cmd_name, objName);
            return NUM_PARMS_CMD_ERROR;
        }
        else if(argc > 0 && argv == NULL)
        {
            mlog(CRITICAL, "No parameters supplied when %d parameters expected for command %s for object %s", argc, cmd_name, objName);
            return NO_PARMS_CMD_ERROR;
        }
        else
        {
            try
            {
                return (this->*cmd->func)(argc, argv);
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "While executing command %s caught unhandled exception %s", cmd_name, e.what());
                return STANDARD_CMD_ERROR;
            }
            catch(...)
            {
                mlog(CRITICAL, "Caught unknown exception while executing command %s", cmd_name);
                return STANDARD_CMD_ERROR;
            }

        }
    }
    catch(RunTimeException& e)
    {
        (void)e;
        mlog(CRITICAL, "Unable to find command %s for object %s", cmd_name, objName);
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
        mlog(DEBUG, "Object %s registered command: %s", objName, name);
    }
    else
    {
        mlog(CRITICAL, "Object %s failed to register command: %s", objName, name);
        delete cmd;
    }

    /* Return Status */
    return status;
}
