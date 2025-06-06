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

#include "CommandProcessor.h"
#include "StringLib.h"
#include "EventLib.h"
#include "RecordObject.h"
#include "OsApi.h"
#include "MsgQ.h"
#include "TimeLib.h"
#include "SystemConfig.h"
#include "core.h"
#ifdef __streaming__
#include "DeviceObject.h"
#endif

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CommandProcessor::TYPE = "CommandProcessor";
const char* CommandProcessor::OBJ_DELIMETER = ":";
const char* CommandProcessor::KEY_DELIMETER = ".";
const char* CommandProcessor::COMMENT = "#";
const char* CommandProcessor::STORE = "@";
const char* CommandProcessor::SELF_KEY = "_SELF";
const char* CommandProcessor::PRIORITY_Q_SUFFIX = "_PRI";

/******************************************************************************
 * PUBLIC FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 *  Constructor -
 *----------------------------------------------------------------------------*/
CommandProcessor::CommandProcessor(const char* cmdq_name): CommandableObject(this, "", TYPE)
{
    assert(cmdq_name);

    executed_commands   = 0;
    rejected_commands   = 0;

    cmdq_publisher      = new Publisher(cmdq_name, NULL, MsgQ::CFG_DEPTH_STANDARD, MAX_CMD_SIZE);
    cmdq_subscriber     = new Subscriber(cmdq_name);

    char priq_name[MAX_STR_SIZE];
    priq_publisher      = new Publisher(StringLib::format(priq_name, MAX_STR_SIZE, "%s%s", cmdq_name, PRIORITY_Q_SUFFIX), NULL, MsgQ::CFG_DEPTH_STANDARD, MAX_CMD_SIZE);
    priq_subscriber     = new Subscriber(priq_name);

    stopwatch_time      = 0.0;

    /* Register Commands */

    registerCommand("HELP",                 (cmdFunc_t)&CommandProcessor::helpCmd,                0,   "");
    registerCommand("VERSION",              (cmdFunc_t)&CommandProcessor::versionCmd,             0,   "");
    registerCommand("QUIT",                 (cmdFunc_t)&CommandProcessor::quitCmd,                0,   "");
    registerCommand("ABORT",                (cmdFunc_t)&CommandProcessor::abortCmd,               0,   "");
    registerCommand("NEW",                  (cmdFunc_t)&CommandProcessor::newCmd,                -2,   "<class name> <object name> [<object parameters>, ...]");
    registerCommand("CLOSE",                (cmdFunc_t)&CommandProcessor::deleteCmd,              1,   "<object name>");
    registerCommand("DELETE",               (cmdFunc_t)&CommandProcessor::deleteCmd,              1,   "<object name>");
    registerCommand("MAKE_PERMANENT",       (cmdFunc_t)&CommandProcessor::permCmd,                1,   "<object name>");
    registerCommand("TYPE",                 (cmdFunc_t)&CommandProcessor::typeCmd,                1,   "<object name>");
    registerCommand("REGISTER",             (cmdFunc_t)&CommandProcessor::registerCmd,            1,   "<object name>");
    registerCommand("DEFINE",               (cmdFunc_t)&CommandProcessor::defineCmd,             -3,   "<record type> <id field> <record size> [<max fields>]");
    registerCommand("ADD_FIELD",            (cmdFunc_t)&CommandProcessor::addFieldCmd,            6,   "<record type> <field name> <field type> <offset> <size> <endian: BE|LE>");
    registerCommand("EXPORT_DEFINITION",    (cmdFunc_t)&CommandProcessor::exportDefinitionCmd,    2,   "<ALL | record type> <output stream>");
    registerCommand("WAIT",                 (cmdFunc_t)&CommandProcessor::waitCmd,                1,   "<seconds to wait>");
    registerCommand("WAIT_ON_EMPTY",        (cmdFunc_t)&CommandProcessor::waitOnEmptyCmd,        -2,   "<stream> <seconds to be empty> [<empty threshold>]");
    registerCommand("START_STOPWATCH",      (cmdFunc_t)&CommandProcessor::startStopWatchCmd,      0,   "");
    registerCommand("DISPLAY_STOPWATCH",    (cmdFunc_t)&CommandProcessor::displayStopWatchCmd,    0,   "");
    registerCommand("LOG_CMD_STATS",        (cmdFunc_t)&CommandProcessor::logCmdStatsCmd,         0,   "");
    registerCommand("EXECUTE_SCRIPT",       (cmdFunc_t)&CommandProcessor::executeScriptCmd,       1,   "<script file name>");
    registerCommand("DEVICE_LIST",          (cmdFunc_t)&CommandProcessor::listDevicesCmd,         0,   "");
    registerCommand("STREAM_LIST",          (cmdFunc_t)&CommandProcessor::listMsgQCmd,            0,   "");
    registerCommand("STREAM_QDEPTH",        (cmdFunc_t)&CommandProcessor::qdepthMsgQCmd,          1,   "<standard queue depth>");
    registerCommand("IO_TIMEOUT",           (cmdFunc_t)&CommandProcessor::setIOTimeoutCmd,        1,   "<timeout for io in seconds>");
    registerCommand("IO_MAXSIZE",           (cmdFunc_t)&CommandProcessor::setIOMaxsizeCmd,        1,   "<buffer size for io in bytes>");

    /* Start Threads */

    procActive          = true;
    procThread          = new Thread(cmdProcThread, this);
}

/*----------------------------------------------------------------------------
 *  Destructor -
 *----------------------------------------------------------------------------*/
CommandProcessor::~CommandProcessor(void)
{
    /* Stop Serialized Execution of Commands */
    procActive = false;
    delete procThread;

    /* Clean Up Regular Objects */
    obj_entry_t entry;
    const char* obj_name = objects.last(&entry);
    while(obj_name)
    {
        delete entry.obj; // calls destructor
        obj_name = objects.prev(&entry);
    }

    /* Clean Up Locked Objects */
    for(int i = 0; i < lockedObjects.length(); i++)
    {
        delete lockedObjects[i].obj;
    }

    /* Clean Up Command Queues */
    delete cmdq_publisher;
    delete cmdq_subscriber;
    delete priq_publisher;
    delete priq_subscriber;
}

/*----------------------------------------------------------------------------
 *  postCommand -
 *----------------------------------------------------------------------------*/
bool CommandProcessor::postCommand(const char* cmdstr, ...)
{
    /* Build Formatted Message String */
    va_list args;
    va_start(args, cmdstr);
    char str[MAX_CMD_SIZE + 1];
    const int vlen = vsnprintf(str, MAX_CMD_SIZE, cmdstr, args);
    const int slen = MIN(vlen, MAX_CMD_SIZE);
    va_end(args);
    str[slen] = '\0';

    /* Get Length of Command */
    if(vlen >= MAX_CMD_SIZE)
    {
        mlog(CRITICAL, "command string too long: %d, must be less than: %d", slen, MAX_CMD_SIZE);
        return false;
    }

    /* Post Command */
    if(cmdq_publisher->postCopy(static_cast<void*>(str), slen + 1) > 0)
    {
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 *  postPriority -
 *----------------------------------------------------------------------------*/
bool CommandProcessor::postPriority(const char* cmdstr, ...)
{
    /* Build Formatted Message String */
    va_list args;
    va_start(args, cmdstr);
    char str[MAX_CMD_SIZE + 1];
    const int vlen = vsnprintf(str, MAX_CMD_SIZE, cmdstr, args);
    const int slen = MIN(vlen, MAX_CMD_SIZE);
    va_end(args);
    str[slen] = '\0';

    /* Get Length of Command */
    if(slen >= MAX_CMD_SIZE)
    {
        mlog(CRITICAL, "command string too long: %d, must be less than: %d", slen, MAX_CMD_SIZE);
        return false;
    }

    /* Post Command */
    if(priq_publisher->postCopy(static_cast<void*>(str), slen + 1) > 0)
    {
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 *  executeScript -
 *
 *   Notes: shortcut here in code to check for need to recursively call executeScript
 *----------------------------------------------------------------------------*/
bool CommandProcessor::executeScript (const char* script_name)  // NOLINT(misc-no-recursion)
{
    /* Open Script */
    FILE* script = fopen(script_name, "r");
    if(script == NULL)
    {
        mlog(CRITICAL, "Unable to open script file: %s", script_name);
        return false;
    }
    else
    {
        mlog(DEBUG, "Processing file: %s", script_name);
    }

    /* Build Command List */
    vector<const char*> script_cmds;
    while(true)
    {
        char line[MAX_CMD_SIZE];
        if(StringLib::getLine(line, NULL, MAX_CMD_SIZE, script) == 0)
        {
            char toks[2][MAX_CMD_SIZE];
            const int num_toks = StringLib::tokenizeLine(line, MAX_CMD_SIZE, ' ', 2, toks);
            if(num_toks >= 2 && StringLib::match(toks[0], "EXECUTE_SCRIPT"))
            {
                const bool success = executeScript(toks[1]);
                if(!success) return false;
            }
            else if((line[0] != '\n') && line[0] != '\0')
            {
                const char* script_cmd = StringLib::duplicate(line);
                script_cmds.push_back(script_cmd);
            }
        }
        else
        {
            break;
        }
    }

    /* Post All Commands */
    for(unsigned i = 0; i < script_cmds.size(); i++)
    {
        postCommand("%s", script_cmds[i]);
        delete [] script_cmds[i];
    }

    /* Close Script and Return */
    fclose(script);
    return true;
}

/*----------------------------------------------------------------------------
 *  registerHandler -
 *
 *   Notes: not thread safe
 *----------------------------------------------------------------------------*/
bool CommandProcessor::registerHandler(const char* handle_name, newFunc_t func, int numparms, const char* desc, bool perm)
{
    handle_entry_t* handle = new handle_entry_t(handle_name, func, numparms, desc, perm);
    if(handlers.add(handle_name, handle))
    {
        mlog(DEBUG, "Registered handler: %s", handle_name);
        return true;
    }
    else
    {
        delete handle;
        mlog(CRITICAL, "Failed to register handler: %s", handle_name);
        return false;
    }

}

/*----------------------------------------------------------------------------
 *  registerObject -
 *
 *   Notes: thread safe... but after this call, callee cannot delete object
 *----------------------------------------------------------------------------*/
bool CommandProcessor::registerObject (const char* obj_name, CommandableObject* obj)
{
    if(setCurrentValue(obj_name, SELF_KEY, &obj, sizeof(obj)) > 0)
    {
        return postPriority("REGISTER %s", obj_name);
    }

    return false;
}

/*----------------------------------------------------------------------------
 *  deleteObject -
 *
 *   Notes: thread safe
 *----------------------------------------------------------------------------*/
bool CommandProcessor::deleteObject (const char* obj_name)
{
    return postPriority("DELETE %s", obj_name);
}

/*----------------------------------------------------------------------------
 * getObject  -
 *
 *   Notes: This would be inherently dangerous as a CommandableObject can be deleted at
 *          any time without warning.  Therefore only permanent objects can be
 *          retrieved by this call.
 *----------------------------------------------------------------------------*/
CommandableObject* CommandProcessor::getObject(const char* obj_name, const char* obj_type)
{
    CommandableObject* obj = NULL;
    try
    {
        obj_entry_t entry = objects[obj_name];
        if(entry.permanent && StringLib::match(entry.obj->getType(), obj_type))
        {
            obj = entry.obj;
        }
    }
    catch(const RunTimeException& e)
    {
        (void)e;
    }

    return obj;
}

/*----------------------------------------------------------------------------
 * getObjectType
 *----------------------------------------------------------------------------*/
const char* CommandProcessor::getObjectType (const char* obj_name)
{
    try
    {
        obj_entry_t entry = objects[obj_name];
        return entry.obj->getType();
    }
    catch(const RunTimeException& e)
    {
        (void)e;
        return NULL;
    }
}

/*----------------------------------------------------------------------------
 * setCurrentValue
 *----------------------------------------------------------------------------*/
int CommandProcessor::setCurrentValue(const char* obj_name, const char* key, void* data, int size)
{
    assert(key);
    assert(data);
    assert(size > 0);

    char keyname[MAX_KEY_NAME];
    StringLib::format(keyname, MAX_KEY_NAME, "%s%s%s", obj_name, KEY_DELIMETER, key);
    cvt_entry_t* cvt_entry = new cvt_entry_t(data, size);
    cvtCond.lock();
    {
        if(currentValueTable.add(keyname, cvt_entry))
        {
            cvtCond.signal(0, Cond::NOTIFY_ALL);
        }
        else
        {
            delete cvt_entry;
            size = 0;
        }
    }
    cvtCond.unlock();

    return size;
}

/*----------------------------------------------------------------------------
 * getCurrentValue
 *----------------------------------------------------------------------------*/
int CommandProcessor::getCurrentValue(const char* obj_name, const char* key, void* data, int size, int timeout_ms, bool with_delete)
{
    assert(data);

    int ret_size = 0;
    char keyname[MAX_KEY_NAME];
    StringLib::format(keyname, MAX_KEY_NAME, "%s%s%s", obj_name, KEY_DELIMETER, key);

    cvtCond.lock();
    {
        try
        {
            /* Perform Wait */
            if(timeout_ms != IO_CHECK)
            {
                while(!currentValueTable.find(keyname))
                {
                    if(!cvtCond.wait(0, timeout_ms))
                    {
                        break;
                    }
                }
            }

            /* Get CVT Entry */
            cvt_entry_t* cvt_entry = currentValueTable.get(keyname);

            if(cvt_entry->size > size)
            {
                /* Error On Buffer Too Small */
                mlog(CRITICAL, "Buffer too small to hold requested data: %d > %d", cvt_entry->size, size);
            }
            else
            {
                if(cvt_entry->size != size)
                {
                    /* Warn on Buffer Mismatch */
                    mlog(WARNING, "Buffer size mismatch when attempting to retrieve global value %s: %d != %d", keyname, cvt_entry->size, size);
                }

                /* Copy Global Data */
                memcpy(data, cvt_entry->data, size);
                ret_size = size;

                /* Remove Entry */
                if(with_delete)
                {
                    currentValueTable.remove(keyname);
                }
            }
        }
        catch(const RunTimeException& e)
        {
            /* Error on Dictionary Exception */
            mlog(WARNING, "Unable to find global data %s: %s", keyname, e.what());
        }
    }
    cvtCond.unlock();

    return ret_size;
}

/*----------------------------------------------------------------------------
 * cmdProcThread  -
 *----------------------------------------------------------------------------*/
void* CommandProcessor::cmdProcThread (void* parm)
{
    CommandProcessor* cp = static_cast<CommandProcessor*>(parm);

    char* cmdstr = new char[MAX_CMD_SIZE];
    char* pristr = new char[MAX_CMD_SIZE];

    while(cp->procActive)
    {
        /* Get Next Command (or timeout) */
        const int cmdlen = cp->cmdq_subscriber->receiveCopy(cmdstr, MAX_CMD_SIZE, SYS_TIMEOUT);
        if(cmdlen < MAX_CMD_SIZE)   cmdstr[cmdlen] = '\0'; // guarantees null termination
        else                        cmdstr[MAX_CMD_SIZE-1] = '\0';

        /* Drain Priority Queue */
        int prilen;
        while((prilen = cp->priq_subscriber->receiveCopy(pristr, MAX_CMD_SIZE, IO_CHECK)) > 0)
        {
            if(prilen < MAX_CMD_SIZE)   pristr[prilen] = '\0'; // guarantees null termination
            else                        pristr[MAX_CMD_SIZE-1] = '\0';

            if(cp->processCommand(pristr))  cp->executed_commands++;
            else                            cp->rejected_commands++;
        }

        /* Execute Next Command */
        if(cmdlen > 0)
        {
            if(cp->processCommand(cmdstr))  cp->executed_commands++;
            else                            cp->rejected_commands++;
        }
        /* Handle Queue Error */
        else if(cmdlen != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "receive failed with status: %d", cmdlen);
        }
    }

    delete [] cmdstr;
    delete [] pristr;

    return NULL;
}

/*----------------------------------------------------------------------------
 * processCommand  -
 *----------------------------------------------------------------------------*/
bool CommandProcessor::processCommand (const char* cmdstr)
{
    bool status = false;

    /* Check String */
    const int cmdlen = StringLib::size(cmdstr) + 1;
    if(cmdlen <= 0)
    {
        mlog(CRITICAL, "Invalid command string, unable to construct command!");
        return false;
    }

    /* Parse Command into Tokens */
    char(*toks)[MAX_CMD_SIZE] = new (char[MAX_CMD_PARAMETERS + 1][MAX_CMD_SIZE]);
    mlog(DEBUG, "Received command: %s", cmdstr);
    const int totaltoks = StringLib::tokenizeLine(cmdstr, cmdlen, ' ', MAX_CMD_PARAMETERS + 1, toks);
    if(totaltoks > MAX_CMD_PARAMETERS) // unable to determine in more parameters supplied
    {
        mlog(CRITICAL, "Command has too many parameters %d, unable to execute!", totaltoks);
        delete [] toks;
        return false;
    }

    /* Calculate Number of Tokens */
    int tok_index = 0;
    for(tok_index = 0; tok_index < totaltoks; tok_index++)
    {
        if(toks[tok_index][0] == COMMENT[0] || toks[tok_index][0] == STORE[0])
        {
            break;
        }
    }
    const int numtoks = tok_index;

    /* Check for Store */
    const char* store_key = NULL;
    if(tok_index < totaltoks)
    {
        if(toks[tok_index][0] == STORE[0])
        {
            if(toks[tok_index][1] != '\0')
            {
                store_key = &toks[tok_index][1];
            }
        }
    }

    /* Further Process Command (it has enough tokens to be a command) */
    if(numtoks > 0)
    {
        /* Establish Parameters (argc, argv) */
        char* cp_cmd_str = toks[0];
        char (*argv)[MAX_CMD_SIZE] = &toks[1];
        const int argc = numtoks - 1; // command name does not count

        /* Reconstruct Command String (without comments) */
        char echoed_cmd[MAX_CMD_SIZE];
        StringLib::copy(echoed_cmd, cp_cmd_str, MAX_CMD_SIZE);
        for(int i = 0; i < argc; i++)
        {
            StringLib::concat(echoed_cmd, " ", MAX_CMD_SIZE);
            StringLib::concat(echoed_cmd, argv[i], MAX_CMD_SIZE);
        }

        /* Initialize Object and Command */
        CommandableObject* obj = this;
        const char* cmd = cp_cmd_str;

        /* Get Object and Command */
        #define OBJ_CMD_TOKS 2
        char objtoks[OBJ_CMD_TOKS][MAX_CMD_SIZE];
        const int numobjtoks = StringLib::tokenizeLine(cp_cmd_str, StringLib::size(cp_cmd_str), OBJ_DELIMETER[0], OBJ_CMD_TOKS, objtoks);
        if(numobjtoks > 1) // <object name>::<command name>
        {
            try
            {
                obj = objects[objtoks[0]].obj;
                cmd = objtoks[1];
            }
            catch(const RunTimeException& e)
            {
                (void)e;
            }
        }

        /* Execute Object's Command */
        int cmd_status = obj->executeCommand(cmd, argc, argv);
        if(cmd_status < 0)
        {
            mlog(CRITICAL, "command %s failed execution with status %d", echoed_cmd, cmd_status);
        }
        else
        {
            mlog(DEBUG, "command %s successfully executed.", echoed_cmd);
            status = true;
        }

        /* Post Status */
        if(store_key) setCurrentValue(getName(), store_key, &cmd_status, sizeof(cmd_status));
    }

    /* Free Toks */
    delete [] toks;

    /* Return Status */
    return status;
}

/******************************************************************************
 * COMMANDS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * helpCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::helpCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    /* Set Defaults */

    bool built_in_commands = false;
    bool registered_handlers = false;
    bool registered_objects = false;
    bool registered_records = false;
    bool registered_streams = false;

    char* obj_name = NULL;
    const char* rec_name = NULL;
    const char* str_name = NULL;

    /* Parse Parameters */

    if(argc != 0)
    {
        built_in_commands = false;
        registered_handlers = false;
        registered_objects = false;
        registered_records = false;
        registered_streams = false;

        for(int i = 0; i < argc; i++)
        {
            if(StringLib::match(argv[i], "ALL"))
            {
                built_in_commands = true;
                registered_handlers = true;
                registered_objects = true;
                registered_records = true;
                registered_streams = true;
            }
            else if(StringLib::match(argv[i], "BI")) built_in_commands = true;
            else if(StringLib::match(argv[i], "RH")) registered_handlers = true;
            else if(StringLib::match(argv[i], "RO")) registered_objects = true;
            else if(StringLib::match(argv[i], "RR")) registered_records = true;
            else if(StringLib::match(argv[i], "RS")) registered_streams = true;
            else if(StringLib::match(argv[i], "O"))
            {
                if(i < argc - 1)
                {
                    obj_name = argv[i + 1];
                    i++;
                }
                else
                {
                    mlog(CRITICAL, "Must supply object name!");
                    return -1;
                }
            }
            else if(StringLib::match(argv[i], "R"))
            {
                if(i < argc - 1)
                {
                    rec_name = argv[i + 1];
                    i++;
                }
                else
                {
                    mlog(CRITICAL, "Must supply record name!");
                    return -1;
                }
            }
            else if(StringLib::match(argv[i], "S"))
            {
                if(i < argc - 1)
                {
                    str_name = argv[i + 1];
                    i++;
                }
                else
                {
                    mlog(CRITICAL, "Must supply stream name!");
                    return -1;
                }
            }
        }
    }

    /* Display Help*/
    print2term("HELP [<OPTIONS> ...]\n");
    print2term("\tALL: all available help\n");
    print2term("\tBI: built-in commands\n");
    print2term("\tRH: registered handlers\n");
    print2term("\tRO: registered objects\n");
    print2term("\tRR: registered records\n");
    print2term("\tRS: registered streams\n");
    print2term("\tO <object name>: object information\n");
    print2term("\tR <record type>: record information\n");
    print2term("\tS <stream name>: stream information\n");

    if(built_in_commands)
    {
        print2term("\n-------------- Built-In Commands ---------------\n");
        try
        {
            obj_cmd_entry_t* cmd = NULL;
            const char* cmd_name = commands.first(&cmd);
            while(cmd_name)
            {
                print2term("%-32s %s\n", cmd_name, cmd->desc);
                cmd_name = commands.next(&cmd);
            }
        }
        catch(...)
        {
            print2term("!!! Failed to display entire list of commands !!!\n");
        }
    }

    if(registered_handlers)
    {
        print2term("\n-------------- Registered Handlers ---------------\n");
        try
        {
            handle_entry_t* handle = NULL;
            const char* handle_name = handlers.first(&handle);
            while(handle_name)
            {
                print2term("%-32s %s\n", handle->name, handle->desc);
                handle_name = handlers.next(&handle);
            }
        }
        catch(...)
        {
            print2term("!!! Failed to display entire list of handlers !!!\n");
        }
    }

    if(registered_objects)
    {
        print2term("\n-------------- Registered Objects ---------------\n");
        char** objnames = NULL;
        const int numobjs = objects.getKeys(&objnames);
        for(int i = 0; i < numobjs; i++)
        {
            obj_entry_t entry = objects[objnames[i]];   // NOLINT(clang-analyzer-core.CallAndMessage)
            print2term("%s %s (%s)\n", objnames[i], entry.permanent ? "*" : "", entry.obj->getType());
            delete [] objnames[i];
        }
        delete [] objnames;
    }

    if(registered_records)
    {
        print2term("\n-------------- Registered Records ---------------\n");
        char** rectypes = NULL;
        const int numrecs = RecordObject::getRecords(&rectypes);
        for(int i = 0; i < numrecs; i++)
        {
            print2term("%s\n", rectypes[i]);
            delete [] rectypes[i];
        }
        delete [] rectypes;
    }

    if(registered_streams)
    {
        print2term("\n-------------- Registered Streams ---------------\n");
        const int num_msgqs = MsgQ::numQ();
        if(num_msgqs > 0)
        {
            MsgQ::queueDisplay_t* msgQs = new MsgQ::queueDisplay_t[num_msgqs];
            const int numq = MsgQ::listQ(msgQs, num_msgqs);
            for(int i = 0; i < numq; i++)
            {
                print2term("%-40s %8d %9s %d\n",
                    msgQs[i].name, msgQs[i].len, msgQs[i].state,
                    msgQs[i].subscriptions);
            }
            delete [] msgQs;
        }
    }

    if(obj_name != NULL)
    {
        try
        {
            obj_entry_t entry = objects[obj_name];
            print2term("\n-------------- %s %s (%s) ---------------\n", obj_name, entry.permanent ? "*" : "", entry.obj->getType());
            char** cmdnames = NULL;
            char** cmddescs = NULL;
            const int numcmds = entry.obj->getCommands(&cmdnames, &cmddescs);
            for(int j = 0; j < numcmds; j++)
            {
                print2term("%-32s %s\n", cmdnames[j], cmddescs[j]);
                delete [] cmdnames[j];
                delete [] cmddescs[j];
            }
            delete [] cmdnames;
            delete [] cmddescs;
        }
        catch(const RunTimeException& e)
        {
            (void)e;
            print2term("Object %s not found\n", obj_name);
        }
    }

    if(rec_name != NULL)
    {
        print2term("\n-------------- %s ---------------\n", rec_name);
        if(RecordObject::isRecord(rec_name))
        {
            RecordObject* rec = new RecordObject(rec_name);
            char** fieldnames = NULL;
            const int numfields = rec->getFieldNames(&fieldnames);
            for(int i = 0; i < numfields; i++)
            {
                const RecordObject::field_t field = rec->getField(fieldnames[i]);
                print2term("%-32s %-16s %-8d %-8d   %02X\n", fieldnames[i], RecordObject::vt2str(rec->getValueType(field)), (int)field.offset, (int)field.elements, field.flags);
                delete [] fieldnames[i];
            }
            delete [] fieldnames;
        }
    }

    if(str_name != NULL)
    {
        print2term("\n-------------- %s ---------------\n", str_name);
        const int num_msgqs = MsgQ::numQ();
        if(num_msgqs > 0)
        {
            MsgQ::queueDisplay_t* msgQs = new MsgQ::queueDisplay_t[num_msgqs];
            const int numq = MsgQ::listQ(msgQs, num_msgqs);
            for(int i = 0; i < numq; i++)
            {
                if(StringLib::match(str_name, msgQs[i].name))
                {
                    print2term("%8d %9s %d\n",
                        msgQs[i].len, msgQs[i].state,
                        msgQs[i].subscriptions);
                }
            }
            delete [] msgQs;
        }
    }

    print2term("\n\n");

    return 0;
}

/*----------------------------------------------------------------------------
 * versionCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::versionCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argv;
    (void)argc;

    print2term("SlideRule Application Version: %s\n\n", LIBID);

    return 0;
}

/*----------------------------------------------------------------------------
 * quitCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::quitCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argv;
    (void)argc;

    setinactive();

    return 0;
}

/*----------------------------------------------------------------------------
 * abortCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::abortCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argv;
    (void)argc;

    quick_exit(0);

    return 0;
}

/*----------------------------------------------------------------------------
 * newCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::newCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    /* Pull Out Parameters */
    const char* class_name = argv[0];
    const char* obj_name = argv[1];

    /* Check for Existing Object */
    if(objects.find(obj_name))
    {
        mlog(CRITICAL, "Object called %s already exists", obj_name);
        return -1;
    }

    try
    {
        const int minargs = 2;

        /* Look Up Handler */
        handle_entry_t* handle = handlers[class_name];

        /* Check Parameters */
        if(handle->numparms > 0 && handle->numparms != argc - minargs)
        {
            mlog(CRITICAL, "Incorrect number of parameters passed to new command: %d != %d", handle->numparms, argc - minargs);
            return -1;
        }
        else if(handle->numparms < 0 && abs(handle->numparms) > argc - minargs)
        {
            mlog(CRITICAL, "Insufficient number of parameters passed to new command: %d > %d", handle->numparms, argc - minargs);
            return -1;
        }

        /* Create and Register Object */
        CommandableObject* obj = handle->func(this, obj_name, argc - minargs, &argv[minargs]);
        if(obj)
        {
            const obj_entry_t entry(obj, handle->perm);
            const bool registered = objects.add(obj_name, entry, true);
            if(registered)
            {
                mlog(DEBUG, "Object %s created and registered", obj_name);
            }
            else
            {
                delete obj;
                mlog(CRITICAL, "Object %s was not able to be registered!", obj_name);
                return -1;
            }
        }
        else
        {
            mlog(CRITICAL, "Object %s not able to be created!", obj_name);
            return -1;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Unable to find registered handler for %s: %s", class_name, e.what());
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * deleteCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::deleteCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* obj_name = argv[0];
    try
    {
        const obj_entry_t entry = objects[obj_name];

        // Only delete non-permanent objects
        // otherwise leave alive, but still remove
        // from the object table.  This means
        // closing (i.e. destroying) a permanent object
        // leaves it alive in the configuration it was last in,
        // effectively locking its configuration
        if(!entry.permanent)
        {
            try
            {
                delete entry.obj;
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "Caught exception during deletion of object %s --> %s", obj_name, e.what());
            }
            catch(...)
            {
                mlog(CRITICAL, "Caught unknown exception during deletion of object %s", obj_name);
            }
        }
        else
        {
            lockedObjects.add(entry);
            mlog(DEBUG, "Locking permanent object %s as a result of request to delete!", obj_name);
        }

        // Remove object from object list
        objects.remove(obj_name);
    }
    catch(const RunTimeException& e)
    {
        (void)e;
        mlog(CRITICAL, "Attempted to delete non-existent object: %s", obj_name);
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * permCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::permCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* obj_name = argv[0];
    try
    {
        obj_entry_t& entry = objects[obj_name];
        entry.permanent = true;
    }
    catch(const RunTimeException& e)
    {
        (void)e;
        mlog(CRITICAL, "Failed to make object %s permanent!", obj_name);
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * typeCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::typeCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* obj_name = argv[0];
    try
    {
        CommandableObject* cmd_obj = objects[obj_name].obj;
        print2term("%s: %s\n", obj_name, cmd_obj->getType());
    }
    catch(const RunTimeException& e)
    {
        (void)e;
        mlog(ERROR, "Object %s not registered, unable to provide type!", obj_name);
        return -1;
    }


    return 0;
}

/*----------------------------------------------------------------------------
 * registerCmd  -
 *
 *   Notes:   Not fail safe... but it provides a reasonably safe way to synchronize
 *            object registration.  Always registered as permanent.
 *----------------------------------------------------------------------------*/
int CommandProcessor::registerCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* obj_name = argv[0];

    /* Check for Existing Object */
    if(objects.find(obj_name))
    {
        mlog(CRITICAL, "Object called %s already exists", obj_name);
        return -1;
    }

    /* Get CVT Key */
    CommandableObject* obj;
    if(getCurrentValue(obj_name, SELF_KEY, &obj, sizeof(obj), IO_CHECK, true) <= 0)
    {
        mlog(CRITICAL, "Unable to find registry for object %s", obj_name);
        return -1;
    }

    /* Register */
    const obj_entry_t entry(obj, true);
    const bool registered = objects.add(obj_name, entry, true);
    if(registered)
    {
        mlog(DEBUG, "Object %s now registered", obj_name);
    }
    else
    {
        mlog(CRITICAL, "Object %s was not able to be registered!", obj_name);
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * defineCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::defineCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    const char* rec_type = StringLib::StringLib::checkNullStr(argv[0]);
    const char* id_field = StringLib::StringLib::checkNullStr(argv[1]);
    const char* size_str = argv[2];
    const char* max_str = NULL; // argv[3]

    /* Check Rec Type */
    if(rec_type == NULL)
    {
        mlog(CRITICAL, "Must supply a record type");
        return -1;
    }

    /* Get Size */
    long size = 0;
    if(!StringLib::str2long(size_str, &size))
    {
        mlog(CRITICAL, "Invalid size supplied: %s", size_str);
        return -1;
    }
    else if(size <= 0)
    {
        mlog(CRITICAL, "Invalid size supplied: %ld", size);
        return -1;
    }

    /* Get Max Fields */
    long max_fields = 64;
    if(argc > 3)
    {
        max_str = argv[3];
        if(!StringLib::str2long(max_str, &max_fields))
        {
            mlog(CRITICAL, "Invalid max fields supplied: %s", max_str);
            return -1;
        }
        else if(max_fields <= 0)
        {
            mlog(CRITICAL, "Invalid max field value supplied: %ld", max_fields);
            return -1;
        }
    }

    /* Define Record */
    const RecordObject::recordDefErr_t status = RecordObject::defineRecord(rec_type, id_field, size, NULL, 0, max_fields);
    if(status == RecordObject::DUPLICATE_DEF)
    {
        mlog(WARNING, "Attempting to define record that is already defined: %s", rec_type);
        return 0; // this may occur as a part of normal operation... if a command needs to know if a record already exists, it should check it directly
    }
    else if(status != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define record %s: %d", rec_type, (int)status);
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * addFieldCmd  -
 *
 *   Notes: size and offset are bytes except for bitfields, where it is bits
 *----------------------------------------------------------------------------*/
int CommandProcessor::addFieldCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argc;

    const char*                 rec_type    = StringLib::StringLib::checkNullStr(argv[0]);
    const char*                 field_name  = StringLib::StringLib::checkNullStr(argv[1]);
    const RecordObject::fieldType_t field_type = RecordObject::str2ft(argv[2]);
    const char*                 offset_str  = argv[3];
    const char*                 size_str    = argv[4];
    const char*                 flags_str   = argv[5];

    /* Check Field Type */
    if(field_type == RecordObject::INVALID_FIELD)
    {
        mlog(CRITICAL, "Invalid field type supplied");
        return -1;
    }

    /* Get Offset */
    long offset = 0;
    if(!StringLib::str2long(offset_str, &offset))
    {
        mlog(CRITICAL, "Invalid offset supplied: %s", offset_str);
        return -1;
    }
    else if(offset < 0)
    {
        mlog(CRITICAL, "Invalid offset supplied: %ld", offset);
        return -1;
    }

    /* Get Size */
    long size = 0;
    if(!StringLib::str2long(size_str, &size))
    {
        mlog(CRITICAL, "Invalid size supplied: %s", size_str);
        return -1;
    }
    else if(size <= 0)
    {
        mlog(CRITICAL, "Invalid size supplied: %ld", size);
        return -1;
    }

    /* Get Flags */
    const unsigned int flags = RecordObject::str2flags(flags_str);

    /* Define Field */
    const RecordObject::recordDefErr_t status = RecordObject::defineField(rec_type, field_name, field_type, offset, size, NULL, flags);
    if(status == RecordObject::DUPLICATE_DEF)
    {
        mlog(WARNING, "Attempting to define field %s that is already defined for record %s", field_name, rec_type);
        return 0; // this may occur as a part of normal operation and should not signal an error; directly check if field exists if that is needed
    }
    else if(status == RecordObject::NOTFOUND_DEF)
    {
        mlog(CRITICAL, "Record type %s not found, unable to define field %s", rec_type, field_name);
        return -1;
    }
    else if(status != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to add field %s to %s: %d", field_name, rec_type, (int)status);
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * exportDefinitionCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::exportDefinitionCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argc;

    int status = 0;

    const char* rec_type = argv[0];
    const char* qname = StringLib::StringLib::checkNullStr(argv[1]);

    if(qname == NULL)
    {
        mlog(CRITICAL, "Must supply an output stream!");
        return -1;
    }

    Publisher* cmdq_out = new Publisher(qname);

    if(StringLib::match("ALL", rec_type))
    {
        char** rectypes = NULL;
        const int numrecs = RecordObject::getRecords(&rectypes);
        for(int i = 0; i < numrecs; i++)
        {
            const char* id_field = RecordObject::getRecordIdField(rectypes[i]);
            const int data_size = RecordObject::getRecordDataSize(rectypes[i]);
            const int max_fields = RecordObject::getRecordMaxFields(rectypes[i]);
            int post_status = cmdq_out->postString("DEFINE %s %s %d %d\n", rectypes[i], id_field != NULL ? id_field : "NA", data_size, max_fields);
            if(post_status <= 0)
            {
                mlog(CRITICAL, "Failed to post definition for %s on stream %s", rectypes[i], qname);
                status = -1;
            }
            else
            {
                char** fieldnames = NULL;
                RecordObject::field_t** fields = NULL;
                const int numfields = RecordObject::getRecordFields(rectypes[i], &fieldnames, &fields);
                for(int k = 0; k < numfields; k++)
                {
                    const char* flags_str = RecordObject::flags2str(fields[k]->flags);
                    if(fields[k]->type == RecordObject::BITFIELD)   post_status = cmdq_out->postString("ADD_FIELD %s %s %s %d %d %s\n", rectypes[i], fieldnames[k], RecordObject::ft2str(fields[k]->type), fields[k]->offset, fields[k]->elements, flags_str);
                    else                                            post_status = cmdq_out->postString("ADD_FIELD %s %s %s %d %d %s\n", rectypes[i], fieldnames[k], RecordObject::ft2str(fields[k]->type), fields[k]->offset / 8, fields[k]->elements, flags_str);
                    if(post_status <= 0)
                    {
                        mlog(CRITICAL, "Failed to post field definition %s for %s on stream %s... aborting", fieldnames[k], rectypes[i], qname);
                        status = -1;
                    }
                    delete [] flags_str;
                    delete [] fieldnames[k];
                    delete fields[k];
                }
                delete [] fieldnames;
                delete [] fields;
            }
            delete [] rectypes[i];
        }
        delete [] rectypes;
    }
    else if(RecordObject::isRecord(rec_type))
    {
        const char* id_field = RecordObject::getRecordIdField(rec_type);
        const int data_size = RecordObject::getRecordDataSize(rec_type);
        const int max_fields = RecordObject::getRecordMaxFields(rec_type);
        int post_status = cmdq_out->postString("DEFINE %s %s %d %d\n", rec_type, id_field != NULL ? id_field : "NA", data_size, max_fields);
        if(post_status <= 0)
        {
            mlog(CRITICAL, "Failed to post definition for %s on stream %s", rec_type, qname);
            status = -1;
        }
        else
        {
            char** fieldnames = NULL;
            RecordObject::field_t** fields = NULL;
            const int numfields = RecordObject::getRecordFields(rec_type, &fieldnames, &fields);
            for(int k = 0; k < numfields; k++)
            {
                const char* flags_str = RecordObject::flags2str(fields[k]->flags);
                if(fields[k]->type == RecordObject::BITFIELD)   post_status = cmdq_out->postString("ADD_FIELD %s %s %s %d %d %s\n", rec_type, fieldnames[k], RecordObject::ft2str(fields[k]->type), fields[k]->offset, fields[k]->elements, flags_str);
                else                                            post_status = cmdq_out->postString("ADD_FIELD %s %s %s %d %d %s\n", rec_type, fieldnames[k], RecordObject::ft2str(fields[k]->type), fields[k]->offset / 8, fields[k]->elements, flags_str);
                if(post_status <= 0)
                {
                    mlog(CRITICAL, "Failed to post field definition %s for %s on stream %s... aborting", fieldnames[k], rec_type, qname);
                    status = -1;
                }
                delete flags_str;
                delete [] fieldnames[k];
                delete fields[k];
            }
            delete [] fieldnames;
            delete [] fields;
        }
    }
    else
    {
        mlog(CRITICAL, "Record type %s not defined", rec_type);
        status = -1;
    }

    delete cmdq_out;

    return status;
}

/*----------------------------------------------------------------------------
 * waitCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::waitCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argc;

    long secs;

    if(!StringLib::str2long(argv[0], &secs))
    {
        mlog(CRITICAL, "Invalid wait time supplied, must be a number: %s", argv[0]);
        return -1;
    }
    else if(secs <= 0)
    {
        mlog(CRITICAL, "Invalid wait time supplied, must be a positive number: %ld", secs);
        return -1;
    }

    OsApi::sleep(secs);

    return 0;
}

/*----------------------------------------------------------------------------
 * waitOnEmptyCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::waitOnEmptyCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    char*   qname       = argv[0];
    char*   wait_str    = argv[1];
    char*   thresh_str  = NULL; // optionally argv[2]

    long wait = 0;
    if(!StringLib::str2long(wait_str, &wait))
    {
        mlog(CRITICAL, "Invalid wait supplied: %s", wait_str);
        return -1;
    }

    long thresh = 0;
    if(argc > 2)
    {
        thresh_str = argv[2];
        if(!StringLib::str2long(thresh_str, &thresh))
        {
            mlog(CRITICAL, "Invalid threshold supplied: %s", thresh_str);
            return -1;
        }
    }

    if(!MsgQ::existQ(qname))
    {
        mlog(CRITICAL, "MsgQ %s does not exist", qname);
        return -1;
    }

    MsgQ* q = new MsgQ(qname);

    int q_empty_count = 0;
    while(true)
    {
        const int q_count = q->getCount();
        if(q_count <= thresh)   q_empty_count++;
        else                    q_empty_count = 0;

        if(q_empty_count > wait)
        {
            break;
        }

        mlog(CRITICAL, "Waiting... %s is %d of %ld seconds empty (%d)", qname, q_empty_count, wait, q_count);
        OsApi::sleep(1);
    }

    delete q;

    return 0;
}

/*----------------------------------------------------------------------------
 * startStopWatchCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::startStopWatchCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    stopwatch_time = TimeLib::latchtime();

    return 0;
}

/*----------------------------------------------------------------------------
 * displayStopWatchCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::displayStopWatchCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static, readability-make-member-function-const)
{
    (void)argc;
    (void)argv;

    mlog(CRITICAL, "STOPWATCH = %.2lf", TimeLib::latchtime() - stopwatch_time);

    return 0;
}

/*----------------------------------------------------------------------------
 * logCmdStatsCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::logCmdStatsCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static, readability-make-member-function-const)
{
    (void)argc;
    (void)argv;

    print2term("Total Commands Executed: %ld\n", executed_commands);
    print2term("Total Commands Rejected: %ld\n", rejected_commands);

    return 0;
}
/*----------------------------------------------------------------------------
 * executeScriptCmd  -
 *----------------------------------------------------------------------------*/
int CommandProcessor::executeScriptCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* script_name = argv[0];

    if(executeScript(script_name) == false) return -1;

    return 0;
}

/*----------------------------------------------------------------------------
 * listDevicesCmd
 *----------------------------------------------------------------------------*/
int CommandProcessor::listDevicesCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argc;
    (void)argv;

#ifdef __streaming__
    const char* device_list_str = DeviceObject::getDeviceList();
    print2term("%s", device_list_str);
    delete [] device_list_str;
#endif

    return 0;
}

/*----------------------------------------------------------------------------
 * listMsgQCmd
 *----------------------------------------------------------------------------*/
int CommandProcessor::listMsgQCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argv;
    (void)argc;

    const int num_msgqs = MsgQ::numQ();
    if(num_msgqs > 0)
    {
        MsgQ::queueDisplay_t* msgQs = new MsgQ::queueDisplay_t[num_msgqs];
        const int numq = MsgQ::listQ(msgQs, num_msgqs);
        print2term("\n");
        for(int i = 0; i < numq; i++)
        {
            print2term("MSGQ: %40s %8d %9s %d\n",
                msgQs[i].name, msgQs[i].len, msgQs[i].state,
                msgQs[i].subscriptions);
        }
        print2term("\n");
        delete [] msgQs;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * qdepthMsgQCmd
 *----------------------------------------------------------------------------*/
int CommandProcessor::qdepthMsgQCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argc;

    const char* depth_str = argv[0];

    long depth = 0;
    if(!StringLib::str2long(depth_str, &depth))
    {
        mlog(CRITICAL, "Invalid depth supplied: %s", depth_str);
        return -1;
    }

    SystemConfig::settings().msgQDepth = depth;

    return 0;
}

/*----------------------------------------------------------------------------
 * setIOTimeoutCmd
 *----------------------------------------------------------------------------*/
int CommandProcessor::setIOTimeoutCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argc;

    const char* timeout_str = argv[0];

    long timeout = 0;
    if(StringLib::match(timeout_str, "PEND"))
    {
        timeout = IO_PEND;
    }
    else if(StringLib::match(timeout_str, "CHECK"))
    {
        timeout = IO_CHECK;
    }
    else if(!StringLib::str2long(timeout_str, &timeout))
    {
        mlog(CRITICAL, "Invalid timeout supplied: %s", timeout_str);
        return -1;
    }
    else if(timeout < -1)
    {
        mlog(CRITICAL, "Undefined behavior setting timeout to be less than -1");
        return -1;
    }

    OsApi::setIOTimeout(timeout);

    return 0;
}

/*----------------------------------------------------------------------------
 * setIOMaxsizeCmd
 *----------------------------------------------------------------------------*/
int CommandProcessor::setIOMaxsizeCmd (int argc, char argv[][MAX_CMD_SIZE]) // NOLINT(readability-convert-member-functions-to-static)
{
    (void)argc;

    const char* maxsize_str = argv[0];

    long maxsize;
    if(!StringLib::str2long(maxsize_str, &maxsize))
    {
        mlog(CRITICAL, "Invalid maxsize supplied: %s", maxsize_str);
        return -1;
    }
    else if(maxsize < 1)
    {
        mlog(CRITICAL, "Undefined behavior setting maxsize to be less than 1");
        return -1;
    }

    OsApi::setIOMaxsize(maxsize);

    return 0;
}
