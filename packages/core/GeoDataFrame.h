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

#ifndef __geo_data_frame__
#define __geo_data_frame__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "MsgQ.h"
#include "TimeLib.h"
#include "Field.h"
#include "FieldDictionary.h"
#include "FieldColumn.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class GeoDataFrame: public LuaObject, public Field
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* GDF;
        static const char* META;
        static const char* TERMINATE;

        /*--------------------------------------------------------------------
         * Subclasses
         *--------------------------------------------------------------------*/

        struct FrameColumn: public LuaObject
        {
            static const char* OBJECT_TYPE;
            static const char* LUA_META_NAME;
            static const struct luaL_Reg LUA_META_TABLE[];

            static int luaGetData (lua_State* L);

            FrameColumn(lua_State* L, const Field& _column);
            ~FrameColumn(void) override = default;

            const Field& column;
        };

        struct FrameRunner: public LuaObject
        {
            static const char* OBJECT_TYPE;

            static int luaGetRunTime (lua_State* L) {
                try
                {
                    FrameRunner* lua_obj = dynamic_cast<FrameRunner*>(getLuaSelf(L, 1));
                    lua_pushnumber(L, lua_obj->runtime);
                }
                catch(const RunTimeException& e)
                {
                    lua_pushnumber(L, 0.0);
                }
                return 1;
            };

            virtual bool run(GeoDataFrame* dataframe) = 0;

            void updateRunTime(double duration) {
                m.lock();
                {
                    runtime += duration;
                }
                m.unlock();
            };

            FrameRunner(lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]):
                LuaObject(L, OBJECT_TYPE, meta_name, meta_table),
                runtime(0.0) {
                LuaEngine::setAttrFunc(L, "runtime", luaGetRunTime);
            };

            ~FrameRunner(void) override = default;

            Mutex m;
            double runtime;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        long                        length              (void) const;
        long                        addRow              (void);
        vector<string>              getColumnNames      (void) const;
        void                        addColumnData       (const char* name, Field* column);
        Field*                      getColumnData       (const char* name, Field::type_t _type=Field::COLUMN) const;
        void                        addMetaData         (const char* name, Field* column);
        Field*                      getMetaData         (const char* name, Field::type_t _type=Field::FIELD);

        const FieldColumn<time8_t>* getTimeColumn       (void) const;
        const FieldColumn<double>*  getXColumn          (void) const;
        const FieldColumn<double>*  getYColumn          (void) const;
        const FieldColumn<double>*  getZColumn          (void) const;

        const string&               getTimeColumnName   (void) const;
        const string&               getXColumnName      (void) const;
        const string&               getYColumnName      (void) const;
        const string&               getZColumnName      (void) const;

        bool                        waitRunComplete     (int timeout);
        void                        signalRunComplete   (void);

        const Dictionary<FieldDictionary::entry_t>& getColumns(void) const;
        const Dictionary<FieldDictionary::entry_t>& getMeta(void) const;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        vector<long>                indexColumn;
        FieldDictionary             columnFields;
        FieldDictionary             metaFields;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                GeoDataFrame    (lua_State* L, 
                                 const char* meta_name,
                                 const struct luaL_Reg meta_table[],
                                 const std::initializer_list<FieldDictionary::entry_t>& column_list, 
                                 const std::initializer_list<FieldDictionary::entry_t>& meta_list);
        virtual ~GeoDataFrame   (void) override;

        static void*    runThread           (void* parm);
        void            populateGeoColumns  (void);

        string          toJson              (void) const override;
        int             toLua               (lua_State* L) const override;
        void            fromLua             (lua_State* L, int index) override;

        static int      luaExport           (lua_State* L);
        static int      luaImport           (lua_State* L);
        static int      luaGetColumnData    (lua_State* L);
        static int      luaGetMetaData      (lua_State* L);
        static int      luaRun              (lua_State* L);
        static int      luaRunComplete      (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const FieldColumn<time8_t>* timeColumn;
        const FieldColumn<double>*  xColumn;
        const FieldColumn<double>*  yColumn;
        const FieldColumn<double>*  zColumn;

        string                      timeColumnName;
        string                      xColumnName;
        string                      yColumnName;
        string                      zColumnName;

        bool                        active;
        Thread*                     pid;
        Publisher                   pubRunQ;
        Subscriber                  subRunQ;
        Cond                        runSignal;
        bool                        runComplete;
};

#endif  /* __geo_data_frame__ */
