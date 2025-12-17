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

#include <atomic>

#include "OsApi.h"
#include "LuaObject.h"
#include "MsgQ.h"
#include "Field.h"
#include "FieldDictionary.h"
#include "FieldColumn.h"
#include "FieldMap.h"
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
        static const char* SOURCE_ID; // column
        static const char* SOURCE_TABLE; // metadata

        static const int MAX_NAME_SIZE = 128;
        static const uint32_t INVALID_INDEX = 0xFFFFFFFF;
        static const int DEFAULT_RECEIVED_COLUMN_CHUNK_SIZE = 2048;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        static const char* gdfRecType;
        static const RecordObject::fieldDef_t gdfRecDef[];

        static const char* CRS_KEY;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef enum {
            COLUMN_REC = 0,
            META_REC = 1,
            CRS_REC = 2,
            EOF_REC = 3
        } rec_type_t;

        typedef struct {
            uint64_t    key;
            uint32_t    type;
            uint32_t    size; // bytes
            uint32_t    encoding;
            uint32_t    num_rows;
            char        name[MAX_NAME_SIZE];
            uint8_t     data[]; // also used for subrecs
        } gdf_rec_t;

        typedef struct {
            uint32_t    num_columns;
        } eof_subrec_t;

        typedef FieldMap<FieldUntypedColumn>::entry_t column_entry_t;
        typedef FieldDictionary::entry_t meta_entry_t;

        typedef enum {
            OP_NONE = 0,
            OP_MEAN = 1,
            OP_MEDIAN = 2,
            OP_MODE = 3,
            OP_SUM = 4,
            NUM_OPS = 5
        } column_op_t;

        typedef struct {
            FieldColumn<double>* column;
            GeoDataFrame::column_op_t op;
        } ancillary_t;

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

        void                        clear               (void) override;
        long                        length              (void) const override;

        long                        addRow              (void);
        long                        appendFromBuffer    (const char* name, const uint8_t* buffer, int size) const;
        vector<string>              getColumnNames      (void) const;
        bool                        addColumn           (const char* name, FieldUntypedColumn* column, bool free_on_delete);
        bool                        addNewColumn        (const char* name, uint32_t column_encoding);
        bool                        addExistingColumn   (const char* name, FieldUntypedColumn* column);
        FieldUntypedColumn*         getColumn           (const char* name, bool no_throw=false) const;
        bool                        addMetaData         (const char* name, Field* meta, bool free_on_delete);
        Field*                      getMetaData         (const char* name, Field::type_t _type=Field::FIELD, bool no_throw=false) const;
        bool                        deleteColumn        (const char* name);
        void                        populateDataframe   (void);
        const FieldUntypedColumn&   operator[]          (const char* key) const;

        virtual okey_t              getKey              (void) const;

        const FieldColumn<time8_t>* getTimeColumn       (void) const;
        const FieldColumn<double>*  getXColumn          (void) const;
        const FieldColumn<double>*  getYColumn          (void) const;
        const FieldColumn<float>*   getZColumn          (void) const;

        const string&               getTimeColumnName   (void) const;
        const string&               getXColumnName      (void) const;
        const string&               getYColumnName      (void) const;
        const string&               getZColumnName      (void) const;
        string                      getInfoAsJson       (void) const;
        const string&               getCRS              (void) const { return crs; }
        void                        setCRS              (const string& _crs) { crs = _crs; }

        bool                        waitRunComplete     (int timeout);
        void                        signalRunComplete   (void);

        const Dictionary<column_entry_t>&   getColumns  (void) const;
        const Dictionary<meta_entry_t>&     getMeta     (void) const;

        static string       loadCRSFile                 (const char* crsFile);
        static string       extractColumnName           (const string& column_description);
        static column_op_t  extractColumnOperation      (const string& column_description);
        static void         createAncillaryColumns      (Dictionary<ancillary_t>** ancillary_columns, const FieldList<string>& ancillary_fields);
        static void         populateAncillaryColumns    (Dictionary<ancillary_t>* ancillary_columns, const GeoDataFrame& df, int32_t start_index, int32_t num_elements);
        static void         addAncillaryColumns         (Dictionary<ancillary_t>* ancillary_columns, GeoDataFrame* dataframe);
        bool                appendListValues            (const char* name, RecordObject::fieldType_t _type, const void* values, long count, bool nodata);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                            inError;
        long                            numRows;
        FieldMap<FieldUntypedColumn>    columnFields;
        FieldDictionary                 metaFields;

    protected:

        /*--------------------------------------------------------------------
         * Structs
         *--------------------------------------------------------------------*/

        struct receive_info_t {
            GeoDataFrame* dataframe = NULL;
            const char* inq_name = NULL;
            const char* outq_name = NULL;
            int num_channels = 0;
            int timeout = IO_PEND;
            Cond ready_signal;
            bool ready = false;
            receive_info_t(GeoDataFrame* _dataframe, const char* _inq_name, const char* _outq_name, int _num_channels, int _timeout) {
                dataframe = _dataframe;
                inq_name = StringLib::duplicate(_inq_name);
                outq_name = StringLib::duplicate(_outq_name);
                num_channels = _num_channels;
                timeout = _timeout;
            }
            ~receive_info_t() {
                delete [] inq_name;
                delete [] outq_name;
            }
        };

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            Subscriber::msgRef_t ref;
            gdf_rec_t* rec;
        } rec_ref_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        GeoDataFrame        (lua_State* L,
                                            const char* meta_name,
                                            const struct luaL_Reg meta_table[],
                                            const std::initializer_list<FieldMap<FieldUntypedColumn>::init_entry_t>& column_list,
                                            const std::initializer_list<FieldDictionary::init_entry_t>& meta_list, const char* _crs=NULL);
        virtual         ~GeoDataFrame       (void) override;

        void            appendDataframe     (GeoDataFrame::gdf_rec_t* gdf_rec_data, int32_t source_id);
        void            sendDataframe       (const char* rspq, uint64_t key_space, int timeout) const;
        static void*    receiveThread       (void* parm);
        static void*    runThread           (void* parm);

        string          toJson              (void) const override;
        int             toLua               (lua_State* L) const override;
        void            fromLua             (lua_State* L, int index) override;

        static int      luaInError          (lua_State* L);
        static int      luaNumRows          (lua_State* L);
        static int      luaNumColumns       (lua_State* L);
        static int      luaExport           (lua_State* L);
        static int      luaSend             (lua_State* L);
        static int      luaReceive          (lua_State* L);
        static int      luaGetRowData       (lua_State* L);
        static int      luaGetColumnData    (lua_State* L);
        static int      luaGetMetaData      (lua_State* L);
        static int      luaGetCRS           (lua_State* L);
        static int      luaRun              (lua_State* L);
        static int      luaRunComplete      (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const FieldColumn<time8_t>* timeColumn;
        const FieldColumn<double>*  xColumn;
        const FieldColumn<double>*  yColumn;
        const FieldColumn<float>*   zColumn;

        string                      timeColumnName;
        string                      xColumnName;
        string                      yColumnName;
        string                      zColumnName;

        string                      crs;

        std::atomic<bool>           active;
        Thread*                     receivePid;
        Thread*                     runPid;
        Publisher                   pubRunQ;
        Subscriber                  subRunQ;
        Cond                        runSignal;
        bool                        runComplete;
};

#endif  /* __geo_data_frame__ */
