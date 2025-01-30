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
#include "RecordObject.h"

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

        static const int MAX_NAME_SIZE = 128;
        static const uint32_t INVALID_INDEX = 0xFFFFFFFF;
        static const int DEFAULT_RECEIVED_COLUMN_CHUNK_SIZE = 2048;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        static const char* gdfRecType;
        static const RecordObject::fieldDef_t gdfRecDef[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            COLUMN_REC = 0,
            META_REC = 1,
            EOF_REC = 2
        } rec_type_t;

        typedef struct {
            uint64_t    key;
            uint32_t    type;
            uint32_t    size; // bytes
            uint32_t    encoding;
            uint32_t    num_rows;
            char        name[MAX_NAME_SIZE];
            uint8_t     data[]; // num_columns when EOF
        } gdf_rec_t;

        /*--------------------------------------------------------------------
         * Subclasses
         *--------------------------------------------------------------------*/

        struct FrameColumn: public LuaObject
        {
            static const char* OBJECT_TYPE;
            static const char* LUA_META_NAME;
            static const struct luaL_Reg LUA_META_TABLE[];

            FrameColumn(lua_State* L, const Field& _column);
            ~FrameColumn(void) override = default;
            static int luaGetData (lua_State* L);

            const Field& column;
        };

        struct FrameRunner: public LuaObject
        {
            static const char* OBJECT_TYPE;

            FrameRunner(lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]);
            ~FrameRunner(void) override = default;
            static int luaGetRunTime (lua_State* L);
            virtual bool run(GeoDataFrame* dataframe) = 0;
            void updateRunTime(double duration);

            Mutex m;
            double runtime;
        };

        struct FrameSender: public FrameRunner
        {
            static const char* LUA_META_NAME;
            static const struct luaL_Reg LUA_META_TABLE[];

            static int luaCreate (lua_State* L);
            FrameSender(lua_State* L, const char* _rspq, uint64_t _key_space, int _timeout);
            ~FrameSender(void) override;
            bool run(GeoDataFrame* dataframe) override;

            const char* rspq;
            uint64_t key_space;
            int timeout;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void                 init                (void);
        static int                  luaCreate           (lua_State* L);

        long                        length              (void) const override;
        long                        addRow              (void);
        long                        appendFromBuffer    (const char* name, const uint8_t* buffer, int size) const;
        vector<string>              getColumnNames      (void) const;
        bool                        addColumn           (const char* name, Field* column, bool free_on_delete);
        bool                        addColumn           (const char* name, uint32_t _type);
        Field*                      getColumn           (const char* name, Field::type_t _type=Field::COLUMN, bool no_throw=false) const;
        void                        addMetaData         (const char* name, Field* column);
        Field*                      getMetaData         (const char* name, Field::type_t _type=Field::FIELD, bool no_throw=false) const;

        virtual okey_t              getKey              (void) const;

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

        bool                        inError;
        long                        numRows;
        FieldDictionary             columnFields;
        FieldDictionary             metaFields;

    protected:

        /*--------------------------------------------------------------------
         * Structs
         *--------------------------------------------------------------------*/

        struct receive_info_t {
            GeoDataFrame* dataframe = NULL;
            const char* inq_name = NULL;
            const char* outq_name = NULL;
            int total_resources = 0;
            int timeout = IO_PEND;
            Cond ready_signal;
            bool ready = false;
            receive_info_t(GeoDataFrame* _dataframe, const char* _inq_name, const char* _outq_name, int _total_resources, int _timeout) {
                dataframe = _dataframe;
                inq_name = StringLib::duplicate(_inq_name);
                outq_name = StringLib::duplicate(_outq_name);
                total_resources = _total_resources;
                timeout = _timeout;
            }
            ~receive_info_t() {
                delete [] inq_name;
                delete [] outq_name;
            }
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        GeoDataFrame        (lua_State* L,
                                            const char* meta_name,
                                            const struct luaL_Reg meta_table[],
                                            const std::initializer_list<FieldDictionary::entry_t>& column_list,
                                            const std::initializer_list<FieldDictionary::entry_t>& meta_list);
        virtual         ~GeoDataFrame       (void) override;

        static void*    receiveThread       (void* parm);
        static void*    runThread           (void* parm);
        void            populateDataframe   (void);
        void            appendDataframe     (GeoDataFrame::gdf_rec_t* gdf_rec_data);
        void            sendDataframe       (const char* rspq, uint64_t key_space, int timeout) const;

        string          toJson              (void) const override;
        int             toLua               (lua_State* L) const override;
        void            fromLua             (lua_State* L, int index) override;

        static int      luaInError          (lua_State* L);
        static int      luaNumRows          (lua_State* L);
        static int      luaNumColumns       (lua_State* L);
        static int      luaExport           (lua_State* L);
        static int      luaSend             (lua_State* L);
        static int      luaReceive          (lua_State* L);
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
        Thread*                     receivePid;
        Thread*                     runPid;
        Publisher                   pubRunQ;
        Subscriber                  subRunQ;
        Cond                        runSignal;
        bool                        runComplete;
};

#endif  /* __geo_data_frame__ */
