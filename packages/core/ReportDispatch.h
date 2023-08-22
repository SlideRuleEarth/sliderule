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
        virtual bool    processRecord       (RecordObject* record, okey_t key, recVec_t* records) override;

        /* lua functions */
        static int      luaSetIndexDisplay  (lua_State* L);
        static int      luaFlushRow         (lua_State* L);
};

#endif  /* __report_dispatch__ */
