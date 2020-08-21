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

#ifndef __report_dispatch__
#define __report_dispatch__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "DispatchObject.h"
#include "RecordObject.h"
#include "Dictionary.h"
#include "Ordering.h"
#include "File.h"

/******************************************************************************
 * REPORT DISPATCH
 ******************************************************************************/

class ReportDispatch: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            CSV,
            JSON,
            INVALID_FORMAT
        } format_t;

        typedef enum {
            INT_DISPLAY,
            GMT_DISPLAY,
            INVALID_DISPLAY
        } indexDisplay_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int              luaCreate       (lua_State* L);
        static format_t         str2format      (const char* str);
        static const char*      format2str      (format_t _type);
        static indexDisplay_t   str2display     (const char* str);
        static const char*      display2str     (indexDisplay_t _display);

    private:

        /*--------------------------------------------------------------------
         * Report Entry Structure
         *--------------------------------------------------------------------*/

        struct entry_t
        {
            okey_t      index;
            const char* name;
            const char* value;

            entry_t(okey_t _index, const char* _name, const char* _value)
            {
                index = _index;
                name = StringLib::duplicate(_name);
                value = StringLib::duplicate(_value);
            }

            ~entry_t(void)
            {
                if(name) delete [] name;
                if(value) delete [] value;
            }
        };

        /*--------------------------------------------------------------------
         * Report File Class
         *--------------------------------------------------------------------*/

        class ReportFile: public File
        {
            public:

                        ReportFile          (lua_State* L, const char* _filename, format_t _format);
                int     writeFileHeader     (void); // overload
                int     writeFileData       (void);

                static const int            MAX_INDEX_STR_SIZE = 256;
                format_t                    format;
                MgDictionary<const char*, true> values; // indexed by data point names
                okey_t                      index;
                bool                        headerInProgress;
                indexDisplay_t              indexDisplay;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        ReportFile              report;
        okey_t                  lastIndex;
        bool                    fixedHeader;
        bool                    writeHeader;
        bool                    reportError;
        Mutex                   reportMut;
        MgOrdering<entry_t*>*   entries;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        ReportDispatch      (lua_State* L, const char* _filename, format_t _format, int buffer=0, const char** columns=NULL, int num_columns=0);
        virtual         ~ReportDispatch     (void);

        static int      postEntry           (void* data, int size, void* parm);
        bool            flushRow            (void);

        /* overridden methods */
        virtual bool    processRecord       (RecordObject* record, okey_t key);

        /* lua functions */
        static int      luaSetIndexDisplay  (lua_State* L);
        static int      luaFlushRow         (lua_State* L);
};

#endif  /* __report_dispatch__ */
