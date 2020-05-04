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

#ifndef __metric_dispatch__
#define __metric_dispatch__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "Ordering.h"
#include "Dictionary.h"
#include "DispatchObject.h"
#include "OsApi.h"
#include "MetricRecord.h"

/******************************************************************************
 * METRIC DISPATCH CLASS
 ******************************************************************************/

class MetricDispatch: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int FIELD_FILTER_DICT_SIZE = 10;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            unsigned char*  buffer;
            int             size;
        } serialBuffer_t;

        struct fieldValue_t
        {
            bool            enable;
            long            lvalue;
            double          dvalue;
            const char*     svalue;

            fieldValue_t(bool _enable, long _lvalue, double _dvalue, const char* _svalue)
            {
                enable = _enable;
                lvalue = _lvalue;
                dvalue = _dvalue;
                svalue = StringLib::duplicate(_svalue);
            }

            ~fieldValue_t(void)
            {
                if(svalue) delete [] svalue;
            }
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*                     dataField;      // value of metric
        List<long>*                     idFilter;       // id of record, not data or key
        MgDictionary<fieldValue_t*>*    fieldFilter;    // more computationally intensive filter, matches field to value
        Publisher*                      outQ;           // output queue metrics are posted to

        bool                            playbackSource;
        bool                            playbackText;
        bool                            playbackName;
        okey_t                          keyOffset;
        okey_t                          minKey;
        okey_t                          maxKey;

        Mutex                           metricMutex;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    MetricDispatch      (lua_State* L, const char* _data_field, const char* outq_name, List<long>* _id_filter);
                    ~MetricDispatch     (void);

        static void freeSerialBuffer    (void* obj, void* parm);

        /* Overridden Methods */
        bool        processRecord       (RecordObject* record, okey_t key);

        /* Lua Methods */
        static int  luaPlaybackSource   (lua_State* L);
        static int  luaPlaybackText     (lua_State* L);
        static int  luaPlaybackName     (lua_State* L);
        static int  luaSetKeyOffset     (lua_State* L);
        static int  luaSetKeyRange      (lua_State* L);
        static int  luaAddFilter        (lua_State* L);
};

#endif  /* __metric_dispatch__ */
