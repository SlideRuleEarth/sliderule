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

#include "MetricDispatch.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* MetricDispatch::LuaMetaName = "MetricDispatch";
const struct luaL_Reg MetricDispatch::LuaMetaTable[] = {
    {"pbsource",    luaPlaybackSource},
    {"pbtext",      luaPlaybackText},
    {"pbname",      luaPlaybackName},
    {"keyoffset",   luaSetKeyOffset},
    {"keyrange",    luaSetKeyRange},
    {"filter",      luaAddFilter},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<data field name>, <output stream name>, [<filter ID 1>, <filter ID 2>, ... <filter ID N>])
 *----------------------------------------------------------------------------*/
int MetricDispatch::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* data_field = getLuaString(L, 1);
        const char* outq_name = getLuaString(L, 2);

        /* Populate Filter From Command */
        List<long>* id_filter = NULL;
        int num_filters = getLuaNumParms(L) - 2;
        if(num_filters > 0)
        {
            id_filter = new List<long>;
            for(int i = 0; i < num_filters; i++)
            {
                long id = getLuaInteger(L, i + 3);
                id_filter->add(id);
            }
        }

        /* Return Metric Object */
        return createLuaObject(L, new MetricDispatch(L, data_field, outq_name, id_filter));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
MetricDispatch::MetricDispatch(lua_State* L, const char* _data_field, const char* outq_name, List<long>* _id_filter):
    DispatchObject(L, LuaMetaName, LuaMetaTable)
{
    /* Define Metric Record */
    RecordObject::defineRecord(MetricRecord::rec_type, NULL, sizeof(MetricRecord::metric_t), MetricRecord::rec_def, MetricRecord::rec_elem);

    /* Initialize Settings */
    playbackSource  = false;
    playbackText    = false;
    playbackName    = false;
    keyOffset       = 0;
    minKey          = Ordering<serialBuffer_t>::INVALID_KEY;
    maxKey          = Ordering<serialBuffer_t>::INVALID_KEY;

    /* Initialize Key Count/Offset/Min/Max */
    dataField           = StringLib::duplicate(_data_field);
    idFilter            = _id_filter;
    fieldFilter         = NULL;
    outQ                = new Publisher(outq_name, freeSerialBuffer);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
MetricDispatch::~MetricDispatch(void)
{
    if(dataField)   delete [] dataField;
    if(idFilter)    delete idFilter;
    if(fieldFilter) delete fieldFilter;
    if(outQ)        delete outQ;
}

/*----------------------------------------------------------------------------
 * freeSerialBuffer  -
 *
 *   Notes: Passed to constructor of outQ
 *----------------------------------------------------------------------------*/
void MetricDispatch::freeSerialBuffer(void* obj, void* parm)
{
    (void)parm;
    delete [] (char*)obj;
}

/*----------------------------------------------------------------------------
 * processRecord
 *----------------------------------------------------------------------------*/
bool MetricDispatch::processRecord (RecordObject* record, okey_t key)
{
    /* Get Record ID and Key */
    long id = record->getRecordId();
    bool enabled = true;

    /* Check Id Filter */
    if(idFilter)
    {
        metricMutex.lock();
        {
            enabled = false;
            for(int i = 0; i < idFilter->length(); i++)
            {
                if(idFilter->get(i) == id)
                {
                    enabled = true;
                    break;
                }
            }
        }
        metricMutex.unlock();
    }

    /* Check ID Filter */
    if(enabled)
    {
        /* Check Key Filter */
        if( ((minKey == Ordering<serialBuffer_t>::INVALID_KEY) || (key >= minKey)) &&   // above minimum key
            ((maxKey == Ordering<serialBuffer_t>::INVALID_KEY) || (key <= maxKey)) )    // below maximum key
        {
            /* Apply Offset */
            if(keyOffset == Ordering<serialBuffer_t>::INVALID_KEY)
            {
                keyOffset = key;
            }

            /* Calculate Offset Key */
            key = MAX(key - keyOffset, 0);

            /* Check Field Filter */
            if(fieldFilter)
            {
                /* If field filter is supplied, then assume the record is not included unless a field in the filter dictionary
                 * matches what is supplied and the setting for that field is enabled.  If there is a match and the setting for
                 * the field is specified as disabled, then the entire record is considered disabled regardless of any other
                 * matches.  In other words, if a field filter is supplied, all fields have to match and be enabled in order
                 * to pass the record. */
                metricMutex.lock();
                {
                    enabled = true;
                    fieldValue_t* filter_value;
                    const char* field_name = fieldFilter->first(&filter_value);
                    while(enabled && field_name)
                    {
                        RecordObject::field_t field = record->getField(field_name);
                        RecordObject::valType_t field_type = record->getValueType(field);
                        if((field_type == RecordObject::INTEGER) && (filter_value->lvalue != record->getValueInteger(field)))
                        {
                            enabled = filter_value->enable;
                        }
                        else if((field_type == RecordObject::REAL) && (filter_value->dvalue != record->getValueReal(field)))
                        {
                            enabled = filter_value->enable;
                        }
                        else if((field_type == RecordObject::TEXT) && StringLib::match(filter_value->svalue, record->getValueText(field)))
                        {
                            enabled = filter_value->enable;
                        }
                        else
                        {
                            enabled = false;
                        }
                        field_name = fieldFilter->next(&filter_value);
                    }
                }
                metricMutex.unlock();
            }

            /* Capture Data */
            if(enabled)
            {
                /* Generate Data Point */
                RecordObject::field_t data_field = record->getField(dataField);
                if(data_field.type != RecordObject::INVALID_FIELD)
                {
                    /* Playback Source */
                    int size = 0;
                    unsigned char* src = NULL;
                    if(playbackSource) size = record->serialize(&src, RecordObject::ALLOCATE); // allocates memory here

                    /* Playback Text */
                    const char* text = NULL;
                    char valbuf[RecordObject::MAX_VAL_STR_SIZE];
                    if(playbackText) text = record->getValueText(data_field, valbuf);

                    /* Playback Name */
                    const char* name = NULL;
                    char nambuf[MAX_STR_SIZE];
                    if(playbackName) name = StringLib::format(nambuf, MAX_STR_SIZE, "%s.%s", record->getRecordType(), dataField);

                    /* Playback Value */
                    double value = record->getValueReal(data_field);

                    /* Index Data Point*/
                    MetricRecord metric(key, value, text, name, src, size);
                    serialBuffer_t sb; // value copy in ordering add is okay
                    sb.size = metric.serialize(&sb.buffer, RecordObject::ALLOCATE);
                    int status = MsgQ::STATE_TIMEOUT;
                    while(status == MsgQ::STATE_TIMEOUT)
                    {
                        status = outQ->postRef(sb.buffer, sb.size, SYS_TIMEOUT);
                        if(status < 0)
                        {
                            mlog(CRITICAL, "Data dropped (%d) in post of serial buffer to output queue of %s\n", status, ObjectType);
                            break;
                        }
                    }
                }
                else
                {
                    mlog(WARNING, "Unable to index into record %s with field %s\n", record->getRecordType(), dataField);
                }
            }
        }
    }

    return true;
}

/*----------------------------------------------------------------------------
 * luaPlaybackSource - :pbsource(true|false)
 *----------------------------------------------------------------------------*/
int MetricDispatch::luaPlaybackSource(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        MetricDispatch* lua_obj = (MetricDispatch*)getLuaSelf(L, 1);

        /* Configure Playback Source */
        lua_obj->playbackSource = getLuaBoolean(L, 2, false, false, &status);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring playback source: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaPlaybackSource - :pbsource(true|false)
 *----------------------------------------------------------------------------*/
int MetricDispatch::luaPlaybackText(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        MetricDispatch* lua_obj = (MetricDispatch*)getLuaSelf(L, 1);

        /* Configure Playback Source */
        lua_obj->playbackText = getLuaBoolean(L, 2, false, false, &status);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring playback test; %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaPlaybackName - :pbname(true|false)
 *----------------------------------------------------------------------------*/
int MetricDispatch::luaPlaybackName(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        MetricDispatch* lua_obj = (MetricDispatch*)getLuaSelf(L, 1);

        /* Configure Playback Source */
        lua_obj->playbackName = getLuaBoolean(L, 2, false, false, &status);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring playback name: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaSetKeyOffset - :keyoffset(<starting offset for keys>)
 *----------------------------------------------------------------------------*/
int MetricDispatch::luaSetKeyOffset(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        MetricDispatch* lua_obj = (MetricDispatch*)getLuaSelf(L, 1);

        /* Get Offset String */
        const char* offset_str = getLuaString(L, 2);

        /* Calculate Offset */
        if(StringLib::match(offset_str, "FIRST"))
        {
            lua_obj->keyOffset = Ordering<serialBuffer_t>::INVALID_KEY;
            status = true;
        }
        else if(StringLib::find(offset_str, ':'))
        {
            lua_obj->keyOffset = (okey_t)TimeLib::str2gpstime(offset_str);
            mlog(CRITICAL, "Setting key offset to: %llu\n", lua_obj->keyOffset);
            status = true;
        }
        else if(!StringLib::str2ullong(offset_str, &lua_obj->keyOffset))
        {
            mlog(CRITICAL, "Unable to set key offset to: %s\n", offset_str);
        }
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting key offset: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaSetKeyRange - :keyrange(<min key>, <max key>)
 *----------------------------------------------------------------------------*/
int MetricDispatch::luaSetKeyRange(lua_State* L)
{
    bool status = true;

    try
    {
        /* Get Self */
        MetricDispatch* lua_obj = (MetricDispatch*)getLuaSelf(L, 1);

        /* Get Offset String */
        const char* min_str = getLuaString(L, 2);
        const char* max_str = getLuaString(L, 2);

        /* Get Min */
        okey_t min_key;
        if(StringLib::match(min_str, "OPEN"))
        {
            min_key = Ordering<serialBuffer_t>::INVALID_KEY;
        }
        else if(StringLib::find(min_str, ':'))
        {
            min_key = (okey_t)TimeLib::str2gpstime(min_str);
            mlog(CRITICAL, "Setting minimum key to: %llu\n", min_key);
        }
        else if(!StringLib::str2ullong(min_str, &min_key))
        {
            mlog(CRITICAL, "Unable to set minimum key to: %s\n", min_str);
            status = false;
        }

        /* Get Max */
        okey_t max_key;
        if(StringLib::match(max_str, "OPEN"))
        {
            max_key = Ordering<serialBuffer_t>::INVALID_KEY;
        }
        else if(StringLib::find(max_str, ':'))
        {
            max_key = (okey_t)TimeLib::str2gpstime(max_str);
            mlog(CRITICAL, "Setting maximum key to: %llu\n", max_key);
        }
        else if(!StringLib::str2ullong(max_str, &max_key))
        {
            mlog(CRITICAL, "Unable to set maximum key to: %s\n", max_str);
            status = false;
        }

        /* Set Min/Max */
        if(status)
        {
            lua_obj->minKey = min_key;
            lua_obj->maxKey = max_key;
        }
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting key range: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaAddFilter - :filter(<field name>, <enable - true|false>, <filter value as string>)
 *----------------------------------------------------------------------------*/
int MetricDispatch::luaAddFilter(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        MetricDispatch* lua_obj = (MetricDispatch*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* field_name  = getLuaString(L, 2);
        bool        enable      = getLuaBoolean(L, 3);
        const char* field_val_s = getLuaString(L, 4);

        /* Get Filter Field Values as Long and Double */
        long    field_val_l = 0;
        double  field_val_d = 0.0;
        StringLib::str2long(field_val_s, &field_val_l);
        StringLib::str2double(field_val_s, &field_val_d);

        /* Add Field Filter */
        lua_obj->metricMutex.lock();
        {
            /* Create Dictionary if Necessary */
            if(lua_obj->fieldFilter == NULL)
            {
                lua_obj->fieldFilter = new MgDictionary<fieldValue_t*>(FIELD_FILTER_DICT_SIZE);
            }

            /* Add Field Filter */
            fieldValue_t* value = new fieldValue_t(enable, field_val_l, field_val_d, field_val_s);

            /* Add to Dictionary */
            status = lua_obj->fieldFilter->add(field_name, value);
            if(!status)
            {
                mlog(CRITICAL, "Failed to add filter on %s to dictionary from %s!\n", field_name, lua_obj->ObjectType);
                delete value;
            }
        }
        lua_obj->metricMutex.unlock();
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error adding filter: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
