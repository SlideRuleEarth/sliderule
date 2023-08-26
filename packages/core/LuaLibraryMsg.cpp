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

#include "LuaLibraryMsg.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaLibraryMsg::LUA_MSGLIBNAME   = "msg";
const char* LuaLibraryMsg::LUA_PUBMETANAME  = "LuaLibraryMsg.publisher";
const char* LuaLibraryMsg::LUA_SUBMETANAME  = "LuaLibraryMsg.subscriber";
const char* LuaLibraryMsg::LUA_RECMETANAME  = "LuaLibraryMsg.record";

const struct luaL_Reg LuaLibraryMsg::msgLibsF [] = {
    {"publish",       LuaLibraryMsg::lmsg_publish},
    {"subscribe",     LuaLibraryMsg::lmsg_subscribe},
    {"create",        LuaLibraryMsg::lmsg_create},
    {"definition",    LuaLibraryMsg::lmsg_definition},
    {NULL, NULL}
};

const struct luaL_Reg LuaLibraryMsg::pubLibsM [] = {
    {"sendstring",    LuaLibraryMsg::lmsg_sendstring},
    {"sendrecord",    LuaLibraryMsg::lmsg_sendrecord},
    {"sendlog",       LuaLibraryMsg::lmsg_sendlog},
    {"numsubs",       LuaLibraryMsg::lmsg_numsubs},
    {"destroy",       LuaLibraryMsg::lmsg_deletepub},
    {"__gc",          LuaLibraryMsg::lmsg_deletepub},
    {NULL, NULL}
};

const struct luaL_Reg LuaLibraryMsg::subLibsM [] = {
    {"recvstring",    LuaLibraryMsg::lmsg_recvstring},
    {"recvrecord",    LuaLibraryMsg::lmsg_recvrecord},
    {"drain",         LuaLibraryMsg::lmsg_drain},
    {"destroy",       LuaLibraryMsg::lmsg_deletesub},
    {"__gc",          LuaLibraryMsg::lmsg_deletesub},
    {NULL, NULL}
};

const struct luaL_Reg LuaLibraryMsg::recLibsM [] = {
    {"gettype",       LuaLibraryMsg::lmsg_gettype},
    {"getvalue",      LuaLibraryMsg::lmsg_getfieldvalue},
    {"setvalue",      LuaLibraryMsg::lmsg_setfieldvalue},
    {"serialize",     LuaLibraryMsg::lmsg_serialize},
    {"deserialize",   LuaLibraryMsg::lmsg_deserialize},
    {"tabulate",      LuaLibraryMsg::lmsg_tabulate},
    {"detabulate",    LuaLibraryMsg::lmsg_detabulate},
    {"__gc",          LuaLibraryMsg::lmsg_deleterec},
    {NULL, NULL}
};

LuaLibraryMsg::recClass_t LuaLibraryMsg::prefixLookUp[0xFF];
Dictionary<LuaLibraryMsg::recClass_t> LuaLibraryMsg::typeTable;

const char* LuaLibraryMsg::REC_TYPE_ATTR = "_type";
const char* LuaLibraryMsg::REC_ID_ATTR = "_id";

/******************************************************************************
 * LIBRARY METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * lmsg_init
 *----------------------------------------------------------------------------*/
void LuaLibraryMsg::lmsg_init (void)
{
    memset(prefixLookUp, 0, sizeof(prefixLookUp));
}

/*----------------------------------------------------------------------------
 * lmsg_addtype
 *----------------------------------------------------------------------------*/
bool LuaLibraryMsg::lmsg_addtype (const char* recclass, char prefix, createRecFunc cfunc, associateRecFunc afunc)
{

    recClass_t rec_class = {
        .prefix = prefix,
        .create = cfunc,
        .associate = afunc
    };

    typeTable.add(recclass, rec_class);
    prefixLookUp[(uint8_t)prefix] = rec_class;

    return true;
}

/*----------------------------------------------------------------------------
 * luaopen_msglib
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::luaopen_msglib (lua_State *L)
{
    /* Setup Publisher */
    luaL_newmetatable(L, LUA_PUBMETANAME);  /* metatable.__index = metatable */
    lua_pushvalue(L, -1);                   /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, pubLibsM, 0);

    /* Setup Subscriber */
    luaL_newmetatable(L, LUA_SUBMETANAME);  /* metatable.__index = metatable */
    lua_pushvalue(L, -1);                   /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, subLibsM, 0);

    /* Setup Record */
    luaL_newmetatable(L, LUA_RECMETANAME);  /* metatable.__index = metatable */
    lua_pushvalue(L, -1);                   /* duplicates the metatable */
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, recLibsM, 0);

    /* Setup Functions */
    luaL_newlib(L, msgLibsF);

    return 1;
}

/*----------------------------------------------------------------------------
 * populateRecord
 *----------------------------------------------------------------------------*/
RecordObject* LuaLibraryMsg::populateRecord (const char* population_string)
{
    char rec_type[MAX_STR_SIZE];
    const char* pop_str = NULL;
    RecordObject* record = NULL;

    /* Separate Out Record Type and Population String */
    int type_len = 0;
    for(int i = 0; i < MAX_STR_SIZE && population_string[i] != '\0' && population_string[i] != ' '; i++) type_len++;
    if(type_len > 0 && type_len < MAX_STR_SIZE)
    {
        /* Copy Out Record Type */
        memcpy(rec_type, population_string, type_len);
        rec_type[type_len] = '\0';

        /* Point to Population String */
        if(population_string[type_len] != '\0')
        {
            pop_str = &population_string[type_len+1];
        }
    }

    try
    {
        /* Create record */
        uint8_t class_prefix = population_string[0];
        recClass_t rec_class = prefixLookUp[class_prefix];
        if(rec_class.create != NULL)    record = rec_class.create(&rec_type[1]); // skip prefix
        else                            record = new RecordObject(rec_type);

        /* Populate Record */
        if(pop_str)
        {
            record->populate(pop_str);
        }
    }
    catch (const RunTimeException& e)
    {
        if(record) delete record;
        mlog(ERROR, "could not locate record definition for %s: %s", population_string, e.what());
    }

    return record;
}

/*----------------------------------------------------------------------------
 * associateRecord
 *----------------------------------------------------------------------------*/
RecordObject* LuaLibraryMsg::associateRecord (const char* recclass, unsigned char* data, int size)
{
    /* Create record */
    RecordObject* record = NULL;
    try
    {
        if(recclass)
        {
            recClass_t rec_class = typeTable[recclass];
            record = rec_class.associate(data, size);
        }
        else
        {
            record = new RecordObject(data, size);
        }
    }
    catch (const RunTimeException& e)
    {
        if(record) delete record;
        mlog(ERROR, "could not locate record definition for %s: %s", recclass, e.what());
    }

    return record;
}

/******************************************************************************
 * MESSAGE QUEUE LIBRARY EXTENSION METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * lmsg_publish - var = msg.publish(<msgq name>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_publish (lua_State* L)
{
    /* Get Message Queue Name */
    const char* msgq_name = lua_tostring(L, 1);

    /* Get Message User Data */
    msgPublisherData_t* msg_data = (msgPublisherData_t*)lua_newuserdata(L, sizeof(msgPublisherData_t));
    msg_data->msgq_name = StringLib::duplicate(msgq_name);
    msg_data->pub = new Publisher(msgq_name);

    /* Return msgUserData_t to Lua */
    luaL_getmetatable(L, LUA_PUBMETANAME);
    lua_setmetatable(L, -2);    /* associates the publisher meta table with the publisher user data */
    return 1;                   /* returns msg_data which is already on stack */
}

/*----------------------------------------------------------------------------
 * lmsg_subscribe - var = msg.subscribe(<msgq name>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_subscribe (lua_State* L)
{
    /* Get Message Queue Name */
    const char* msgq_name = lua_tostring(L, 1);

    /* Get Message User Data */
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)lua_newuserdata(L, sizeof(msgSubscriberData_t));
    msg_data->msgq_name = StringLib::duplicate(msgq_name);
    msg_data->sub = new Subscriber(msgq_name);

    /* Return msgUserData_t to Lua */
    luaL_getmetatable(L, LUA_SUBMETANAME);
    lua_setmetatable(L, -2);    /* associates the subscriber meta table with the subscriber user data */
    return 1;                   /* returns msg_data which is already on stack */
}

/*----------------------------------------------------------------------------
 * lmsg_create - rec = msg.create(<population string>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_create (lua_State* L)
{
    /* Create Record */
    const char* population_string = lua_tostring(L, 1);
    RecordObject* record = populateRecord(population_string);
    if(record == NULL)
    {
        return luaL_error(L, "invalid record specified");
    }

    /* Create User Data */
    recUserData_t* rec_data = (recUserData_t*)lua_newuserdata(L, sizeof(recUserData_t));
    rec_data->record_str = StringLib::duplicate(population_string);
    rec_data->rec = record;

    /* Return recUserData_t to Lua */
    luaL_getmetatable(L, LUA_RECMETANAME);
    lua_setmetatable(L, -2);    /* associates the record meta table with the record user data */
    return 1;                   /* returns msg_data which is already on stack */
}

/*----------------------------------------------------------------------------
 * lmsg_definition - tbl = msg.definition(<record type>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_definition(lua_State* L)
{
    /* Get Record Type */
    const char* rectype = lua_tostring(L, 1);
    if(rectype == NULL)
    {
        return luaL_error(L, "invalid record type specified");
    }

    /* Get Record Definition */
    char** fieldnames = NULL;
    RecordObject::field_t** fields = NULL;
    int numfields = RecordObject::getRecordFields(rectype, &fieldnames, &fields);
    if(numfields > 0)
    {
        lua_newtable(L);
        LuaEngine::setAttrNum(L, "__datasize", RecordObject::getRecordDataSize(rectype));
        for(int i = 0; i < numfields; i++)
        {
            const char* flagstr = RecordObject::flags2str(fields[i]->flags);
            const char* typestr = fields[i]->exttype;
            if(fields[i]->type != RecordObject::USER)
            {
                typestr = RecordObject::ft2str(fields[i]->type);
            }

            lua_pushstring(L, fieldnames[i]);
            lua_newtable(L);
            LuaEngine::setAttrStr(L, "type", typestr);
            LuaEngine::setAttrNum(L, "offset", fields[i]->offset);
            LuaEngine::setAttrNum(L, "elements", fields[i]->elements);
            LuaEngine::setAttrStr(L, "flags", flagstr);
            lua_settable(L, -3);

            delete [] flagstr;
            delete fields[i];
            delete [] fieldnames[i];
        }
        delete [] fields;
        delete [] fieldnames;
    }
    else
    {
        lua_pushnil(L);
        return 1;
    }

    /* Return Table */
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_sendstring
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_sendstring (lua_State* L)
{
    size_t len;

    msgPublisherData_t* msg_data = (msgPublisherData_t*)luaL_checkudata(L, 1, LUA_PUBMETANAME);
    if(msg_data == NULL)
    {
        return luaL_error(L, "invalid message queue");
    }

    const char* str = lua_tolstring(L, 2, &len);        /* get argument */
    int status = msg_data->pub->postCopy(str, len);     /* post string */
    lua_pushboolean(L, status > 0);                     /* push result status */
    return 1;                                           /* number of results */
}

/*----------------------------------------------------------------------------
 * lmsg_sendrecord - <record | population string>
 *
 *  Note that there are different types of records.  The BASE class record can be sent
 *  without any explicit specification.  Other record types require a special character
 *  to be prefixed to the population string.  Currently the following record
 *  types are explicitly supported:
 *      BASE  - <population string>
 *      CCSDS - /<population string>
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_sendrecord (lua_State* L)
{
    msgPublisherData_t* msg_data = (msgPublisherData_t*)luaL_checkudata(L, 1, LUA_PUBMETANAME);
    if(msg_data == NULL)
    {
        return luaL_error(L, "invalid message queue");
    }

    RecordObject* record = NULL;
    bool allocated = false;

    if(lua_isuserdata(L, 2)) // user data record
    {
        LuaLibraryMsg::recUserData_t* rec_data = (LuaLibraryMsg::recUserData_t*)luaL_checkudata(L, 2, LuaLibraryMsg::LUA_RECMETANAME);
        record = rec_data->rec;
        if(record == NULL)
        {
            return luaL_error(L, "nill record supplied.");
        }
    }
    else // population string
    {
        const char* population_string = lua_tostring(L, 2);  /* get argument */
        record = populateRecord(population_string);
        allocated = true;
        if(record == NULL)
        {
            return luaL_error(L, "invalid record retrieved.");
        }
    }

    /* Post Serialized Record */
    int status = 0; // initial status returned is false
    unsigned char* buffer; // reference to serial buffer
    int size = record->serialize(&buffer, RecordObject::REFERENCE);
    if(size > 0) status = msg_data->pub->postCopy(buffer, size);
    if(status <= 0) // if size check fails above, then status will remain zero
    {
        mlog(CRITICAL, "Failed to post record %s to %s with error code %d", record->getRecordType(), msg_data->pub->getName(), status);
    }

    /* Clean Up and Return Status */
    if(allocated) delete record;
    lua_pushboolean(L, status > 0);
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_sendlog - .sendlog(<lvl>, <msg>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_sendlog (lua_State* L)
{
    /* Get Publisher */
    msgPublisherData_t* msg_data = (msgPublisherData_t*)luaL_checkudata(L, 1, LUA_PUBMETANAME);
    if(msg_data == NULL)
    {
        return luaL_error(L, "invalid message queue");
    }

    /* Get Event Level */
    event_level_t lvl = INVALID_EVENT_LEVEL;
    if(lua_isinteger(L, 2))
    {
        lvl = (event_level_t)lua_tointeger(L, 2);
    }

    /* Check Event Level */
    if(lvl == INVALID_EVENT_LEVEL)
    {
        mlog(CRITICAL, "Invalid event level: %d", lvl);
        return 0;
    }

    /* Get Message */
    size_t attr_size = 0;
    const char* attr = lua_tolstring(L, 3, &attr_size);
    if(attr_size <= 0)
    {
        mlog(CRITICAL, "Invalid length of message: %ld", attr_size);
        return 0;
    }

    /* Construct Log Record */
    EventLib::event_t event;
    memset(&event, 0, sizeof(event));
    event.systime = TimeLib::gpstime();
    event.tid     = Thread::getId();
    event.id      = ORIGIN;
    event.parent  = ORIGIN;
    event.flags   = 0;
    event.type    = EventLib::LOG;
    event.level   = lvl;
    StringLib::copy(event.ipv4, SockLib::sockipv4(), SockLib::IPV4_STR_LEN);
    StringLib::copy(event.name, "sendlog", EventLib::MAX_NAME_SIZE);
    StringLib::copy(event.attr, attr, attr_size + 1);

    /* Post Record */
    int rec_size = offsetof(EventLib::event_t, attr) + attr_size + 1;
    RecordObject record(EventLib::rec_type, rec_size);
    memcpy(record.getRecordData(), &event, rec_size);
    uint8_t* rec_buf = NULL;
    int rec_bytes = record.serialize(&rec_buf, RecordObject::REFERENCE);
    int bytes_sent = msg_data->pub->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT);

    /* Return Results */
    lua_pushboolean(L, bytes_sent > 0);
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_numsubs- .numsubs() --> number of subscribers on message queue
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_numsubs (lua_State* L)
{
    /* Get Publisher */
    msgPublisherData_t* msg_data = (msgPublisherData_t*)luaL_checkudata(L, 1, LUA_PUBMETANAME);
    if(msg_data)
    {
        /* Get Number of Subscriptions */
        int subscriptions = msg_data->pub->getSubCnt();

        /* Return Results */
        lua_pushinteger(L, subscriptions);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_deletepub
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_deletepub (lua_State* L)
{
    msgPublisherData_t* msg_data = (msgPublisherData_t*)luaL_checkudata(L, 1, LUA_PUBMETANAME);

    if(msg_data)
    {
        if(msg_data->msgq_name)
        {
            delete [] msg_data->msgq_name;
            msg_data->msgq_name = NULL;
        }

        if(msg_data->pub)
        {
            delete msg_data->pub;
            msg_data->pub = NULL;
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * lmsg_recvstring
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_recvstring (lua_State* L)
{
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)luaL_checkudata(L, 1, LUA_SUBMETANAME);
    if(msg_data)
    {
        int timeoutms = (int)lua_tointeger(L, 2);
        char str[MAX_STR_SIZE];
        int strlen = msg_data->sub->receiveCopy(str, MAX_STR_SIZE - 1, timeoutms);
        if(strlen > 0)
        {
            lua_pushlstring(L, str, strlen);
        }
        else
        {
            lua_pushnil(L);
        }
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_recvrecord
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_recvrecord (lua_State* L)
{
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)luaL_checkudata(L, 1, LUA_SUBMETANAME);
    if(msg_data == NULL)
    {
        return luaL_error(L, "invalid message queue");
    }

    int timeoutms = (int)lua_tointeger(L, 2);
    const char* recclass = NULL;
    if(lua_isstring(L, 3))
    {
        recclass = (const char*)lua_tostring(L, 3);
    }

    bool terminator = false;

    Subscriber::msgRef_t ref;
    int status = msg_data->sub->receiveRef(ref, timeoutms);
    if(status > 0)
    {
        /* Create record */
        RecordObject* record = NULL;
        if(ref.size > 0)
        {
            try
            {
                if(recclass)
                {
                    recClass_t rec_class = typeTable[recclass];
                    record = rec_class.associate((unsigned char*)ref.data, ref.size);
                }
                else
                {
                    record = new RecordObject((unsigned char*)ref.data, ref.size);
                }
            }
            catch (const RunTimeException& e)
            {
                mlog(ERROR, "could not locate record definition for %s: %s", recclass, e.what());
            }
        }
        else
        {
            terminator = true;
        }

        /* Free Reference */
        msg_data->sub->dereference(ref);

        /* Assign Record to User Data */
        if(record)
        {
            LuaLibraryMsg::recUserData_t* rec_data = (LuaLibraryMsg::recUserData_t*)lua_newuserdata(L, sizeof(LuaLibraryMsg::recUserData_t));
            rec_data->record_str = NULL;
            rec_data->rec = record;
            luaL_getmetatable(L, LUA_RECMETANAME);
            lua_setmetatable(L, -2); // associates the record meta table with the record user data
        }
        else
        {
            mlog(WARNING, "Unable to create record object: %s", recclass);
            lua_pushnil(L); // for record
        }
    }
    else
    {
        mlog(CRITICAL, "Failed (%d) to receive record on message queue %s", status, msg_data->sub->getName());
        lua_pushnil(L); // for record
    }

    /* Status terminator */
    lua_pushboolean(L, terminator);
    return 2;
}

/*----------------------------------------------------------------------------
 * lmsg_drain
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_drain (lua_State* L)
{
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)luaL_checkudata(L, 1, LUA_SUBMETANAME);

    if(msg_data)
    {
        msg_data->sub->drain();
        lua_pushboolean(L, true);
    }
    else
    {
        lua_pushboolean(L, false);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_deletesub
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_deletesub (lua_State* L)
{
    msgSubscriberData_t* msg_data = (msgSubscriberData_t*)luaL_checkudata(L, 1, LUA_SUBMETANAME);

    if(msg_data)
    {
        if(msg_data->msgq_name)
        {
            delete [] msg_data->msgq_name;
            msg_data->msgq_name = NULL;
        }

        if(msg_data->sub)
        {
            delete msg_data->sub;
            msg_data->sub = NULL;
        }
    }

    return 0;
}

/******************************************************************************
 * RECORD LIBRARY EXTENSION METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * lmsg_gettype - val = rec:gettype()
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_gettype (lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    if(rec_data == NULL)
    {
        return luaL_error(L, "invalid record");
    }
    else if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    const char* rectype = rec_data->rec->getRecordType();
    lua_pushstring(L, rectype);

    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_getfieldvalue - val = rec:getvalue(<field name>)
 *
 *  the field name can include an array element, e.g. field[element]
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_getfieldvalue (lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    const char* fldname = lua_tostring(L, 2); // get field name

    if(rec_data == NULL)
    {
        return luaL_error(L, "invalid record");
    }
    else if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    RecordObject::field_t field = rec_data->rec->getField(fldname);
    RecordObject::valType_t valtype = rec_data->rec->getValueType(field);

    if(valtype == RecordObject::TEXT)
    {
        char valbuf[RecordObject::MAX_VAL_STR_SIZE];
        const char* val = rec_data->rec->getValueText(field, valbuf);
        lua_pushstring(L, val);
    }
    else if(valtype == RecordObject::REAL)
    {
        double val = rec_data->rec->getValueReal(field);
        lua_pushnumber(L, val);
    }
    else if(valtype == RecordObject::INTEGER)
    {
        long val = rec_data->rec->getValueInteger(field);
        lua_pushnumber(L, (double)val);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_setfieldvalue - rec:setvalue(<field name>, <value>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_setfieldvalue (lua_State* L)
{
    bool status = true;
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    const char* fldname = lua_tostring(L, 2);  /* get field name */

    if(rec_data == NULL)
    {
        return luaL_error(L, "invalid record");
    }
    else if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    RecordObject::field_t field = rec_data->rec->getField(fldname);
    RecordObject::valType_t valtype = rec_data->rec->getValueType(field);
    if(valtype == RecordObject::TEXT)
    {
        const char* val = lua_tostring(L, 3);
        rec_data->rec->setValueText(field, val);
    }
    else if(valtype == RecordObject::REAL)
    {
        double val = lua_tonumber(L, 3);
        rec_data->rec->setValueReal(field, val);
    }
    else if(valtype == RecordObject::INTEGER)
    {
        long val = (long)lua_tointeger(L, 3);
        rec_data->rec->setValueInteger(field, val);
    }
    else
    {
        status = false;
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_serialize - rec:serialize() --> string
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_serialize(lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    if(rec_data == NULL)
    {
        return luaL_error(L, "invalid record");
    }
    else if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    unsigned char* buffer = NULL;
    int bytes = rec_data->rec->serialize(&buffer); // memory allocated
    lua_pushlstring(L, (const char*)buffer, bytes);
    delete [] buffer; // memory freed

    return 1; // number of items returned (record string)
}

/*----------------------------------------------------------------------------
 * lmsg_deserialize - rec:deserialize(string)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_deserialize(lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    if(rec_data == NULL)
    {
        return luaL_error(L, "invalid record");
    }
    else if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    size_t lbuf = 0;
    unsigned char* buffer = (unsigned char*)lua_tolstring(L, 2, &lbuf);

    bool status = rec_data->rec->deserialize(buffer, lbuf);
    lua_pushboolean(L, status);

    return 1; // number of items returned (status of deserialization)
}

/*----------------------------------------------------------------------------
 * lmsg_tabulate - rec:tabulate()
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_tabulate(lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    if(rec_data == NULL)
    {
        return luaL_error(L, "invalid record");
    }
    else if(rec_data->rec == NULL)
    {
        return luaL_error(L, "record does not exist");
    }

    /* Create Table for Record */
    lua_newtable(L);
    LuaEngine::setAttrStr(L, REC_TYPE_ATTR, rec_data->rec->getRecordType());
    LuaEngine::setAttrInt(L, REC_ID_ATTR, rec_data->rec->getRecordId());

    /* Add Fields to Table */
    char** fieldnames = NULL;
    int numfields = rec_data->rec->getFieldNames(&fieldnames);
    for(int i = 0; i < numfields; i++)
    {
        RecordObject::field_t field = rec_data->rec->getField(fieldnames[i]);
        switch(rec_data->rec->getValueType(field))
        {
            case RecordObject::TEXT:
            {
                LuaEngine::setAttrStr(L, fieldnames[i], rec_data->rec->getValueText(field));
                break;
            }
            case RecordObject::REAL:
            {
                if(field.elements == 1)
                {
                    LuaEngine::setAttrNum(L, fieldnames[i], rec_data->rec->getValueReal(field));
                }
                else
                {
                    int num_elements = field.elements;
                    if(num_elements <= 0)
                    {
                        num_elements = (rec_data->rec->getAllocatedDataSize() - (field.offset / 8)) / RecordObject::FIELD_TYPE_BYTES[field.type];
                    }
                    lua_pushstring(L, fieldnames[i]);
                    lua_newtable(L);
                    for(int e = 0; e < num_elements; e++)
                    {
                        lua_pushnumber(L, rec_data->rec->getValueReal(field, e));
                        lua_rawseti(L, -2, e+1);
                    }
                    lua_settable(L, -3);
                }
                break;
            }
            case RecordObject::INTEGER:
            {
                if(field.elements == 1)
                {
                    LuaEngine::setAttrInt(L, fieldnames[i], rec_data->rec->getValueInteger(field));
                }
                else
                {
                    int num_elements = field.elements;
                    if(num_elements <= 0)
                    {
                        num_elements = (rec_data->rec->getAllocatedDataSize() - (field.offset / 8)) / RecordObject::FIELD_TYPE_BYTES[field.type];
                    }
                    lua_pushstring(L, fieldnames[i]);
                    lua_newtable(L);
                    for(int e = 0; e < num_elements; e++)
                    {
                        lua_pushnumber(L, rec_data->rec->getValueInteger(field, e));
                        lua_rawseti(L, -2, e+1);
                    }
                    lua_settable(L, -3);
                }
                break;
            }
            default: break;
        }
        delete [] fieldnames[i];
    }
    delete [] fieldnames;

    /* Return Table */
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_detabulate - rec:detabulate(<record table>, <record class>)
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_detabulate(lua_State* L)
{
    /* Check Parameters */
    if(lua_type(L, 1) != LUA_TTABLE)
    {
        return luaL_error(L, "must supply table");
    }

    /* Get Record Class */
    const char* recclass = NULL;
    if(lua_isstring(L, 2))
    {
        recclass = lua_tostring(L, 2);
    }

    /* Get Record Type */
    lua_getfield(L, 1, REC_TYPE_ATTR);
    const char* rec_type = lua_tostring(L, 1);
    lua_pop(L, 1);
    if(!rec_type)
    {
        return luaL_error(L, "table must have type attribute");
    }

    /* Create record */
    RecordObject* record = NULL;
    try
    {
        if(recclass)
        {
            recClass_t rec_class = typeTable[recclass];
            record = rec_class.create(rec_type);
        }
        else
        {
            record = new RecordObject(rec_type);
        }
    }
    catch (const RunTimeException& e)
    {
        return luaL_error(L, "could not locate record definition %s", rec_type);
    }

    /* Populate Fields from Table */
    char** fieldnames = NULL;
    int numfields = record->getFieldNames(&fieldnames);
    for(int i = 0; i < numfields; i++)
    {
        RecordObject::field_t field = record->getField(fieldnames[i]);
        lua_getfield(L, 1, fieldnames[i]);
        switch(record->getValueType(field))
        {
            case RecordObject::TEXT:
            {
                if(lua_isstring(L, 1))
                {
                    const char* val = lua_tostring(L, 1);
                    record->setValueText(field, val);
                }
                break;
            }
            case RecordObject::REAL:
            {
                if(field.elements <= 1)
                {
                    if(lua_isnumber(L, 1))
                    {
                        double val = lua_tonumber(L, 1);
                        record->setValueReal(field, val);
                    }
                }
                else
                {
                    if(lua_type(L, 1) != LUA_TTABLE)
                    {
                        for(int e = 0; e < field.elements; e++)
                        {
                            lua_rawgeti(L, 1, e+1);
                            if(lua_isnumber(L, 1))
                            {
                                double val = lua_tonumber(L, 1);
                                record->setValueReal(field, val, e);
                            }
                            lua_pop(L, 1);
                        }
                    }

                }
                break;
            }
            case RecordObject::INTEGER:
            {
                if(field.elements <= 1)
                {
                    if(lua_isnumber(L, 1))
                    {
                        long val = lua_tointeger(L, 1);
                        record->setValueInteger(field, val);
                    }
                }
                else
                {
                    if(lua_type(L, 1) != LUA_TTABLE)
                    {
                        for(int e = 0; e < field.elements; e++)
                        {
                            lua_rawgeti(L, 1, e+1);
                            if(lua_isnumber(L, 1))
                            {
                                long val = lua_tointeger(L, 1);
                                record->setValueInteger(field, val, e);
                            }
                            lua_pop(L, 1);
                        }
                    }
                }
                break;
            }
            default: break;
        }
        lua_pop(L, 1);
        delete [] fieldnames[i];
    }
    delete [] fieldnames;

    /* Assign Record to User Data */
    LuaLibraryMsg::recUserData_t* rec_data = (LuaLibraryMsg::recUserData_t*)lua_newuserdata(L, sizeof(LuaLibraryMsg::recUserData_t));
    rec_data->record_str = NULL;
    rec_data->rec = record;
    luaL_getmetatable(L, LUA_RECMETANAME);
    lua_setmetatable(L, -2); // associates the record meta table with the record user data

    /* Return Record User Data */
    return 1;
}

/*----------------------------------------------------------------------------
 * lmsg_deleterec
 *----------------------------------------------------------------------------*/
int LuaLibraryMsg::lmsg_deleterec (lua_State* L)
{
    recUserData_t* rec_data = (recUserData_t*)luaL_checkudata(L, 1, LUA_RECMETANAME);
    if(rec_data)
    {
        if(rec_data->record_str) delete [] rec_data->record_str;
        if(rec_data->rec) delete rec_data->rec;
    }
    return 0;
}
