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

#ifndef __parquet_builder__
#define __parquet_builder__

/*
 * ParquetBuilder works on batches of records.  It expects the `batch_rec_type`
 * passed into the constructor to be the type that defines each of the column headings,
 * then it expects to receive records that are arrays (or batches) of that record
 * type.  The field defined as an array is transparent to this class - it just
 * expects the record to be a single array.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "Ordering.h"
#include "RecordObject.h"
#include "ArrowParms.h"
#include "OsApi.h"
#include "MsgQ.h"

/******************************************************************************
 * FORWARD DECLARATIONS
 ******************************************************************************/

class ArrowImpl; // arrow implementation

/******************************************************************************
 * PARQUET BUILDER CLASS
 ******************************************************************************/

class ParquetBuilder: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int FILE_NAME_MAX_LEN = 128;
        static const int URL_MAX_LEN = 512;
        static const int FILE_BUFFER_RSPS_SIZE = 0x2000000; // 32MB
        static const int ROW_GROUP_SIZE = 0x4000000; // 64MB
        static const int QUEUE_BUFFER_FACTOR = 3;

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        static const char* metaRecType;
        static const RecordObject::fieldDef_t metaRecDef[];

        static const char* dataRecType;
        static const RecordObject::fieldDef_t dataRecDef[];

        static const char* remoteRecType;
        static const RecordObject::fieldDef_t remoteRecDef[];

        static const char* TMP_FILE_PREFIX;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        struct batch_t {
            Subscriber::msgRef_t    ref;
            Subscriber*             in_q;
            RecordObject*           pri_record;
            RecordObject**          anc_records;
            int                     rows;
            int                     num_anc_recs;
            int                     anc_fields;
            int                     anc_elements;
            batch_t(Subscriber::msgRef_t& _ref, Subscriber* _in_q):
                ref(_ref),
                in_q(_in_q),
                pri_record(NULL),
                anc_records(NULL),
                rows(0),
                num_anc_recs(0),
                anc_fields(0),
                anc_elements(0) {}
            ~batch_t(void) {
                in_q->dereference(ref);
                delete pri_record;
                for(int i = 0; i < num_anc_recs; i++) delete anc_records[i];
                delete [] anc_records; }
        };

        typedef List<batch_t*>  batch_list_t;

        typedef struct {
            bool                    as_geo;
            RecordObject::field_t   x_field;
            RecordObject::field_t   y_field;
        } geo_data_t;

        typedef struct {
            char    filename[FILE_NAME_MAX_LEN];
            long    size;
        } arrow_file_meta_t;

        typedef struct {
            char    filename[FILE_NAME_MAX_LEN];
            uint8_t data[FILE_BUFFER_RSPS_SIZE];
        } arrow_file_data_t;

        typedef struct {
            char    url[URL_MAX_LEN];
            long    size;
        } arrow_file_remote_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int              luaCreate       (lua_State* L);
        static void             init            (void);
        static void             deinit          (void);

        const char*             getFileName     (void);
        const char*             getRecType      (void);
        const char*             getTimeKey      (void);
        bool                    getAsGeo        (void);
        RecordObject::field_t&  getXField       (void);
        RecordObject::field_t&  getYField       (void);
        ArrowParms*             getParms        (void);
        bool                    hasAncFields    (void);
        bool                    hasAncElements  (void);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int EXPECTED_RECORDS_IN_BATCH = 256;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Thread*             builderPid;
        ArrowParms*         parms;
        bool                active;
        Subscriber*         inQ;
        const char*         recType;
        const char*         timeKey;
        batch_list_t        recordBatch;
        bool                hasAncillaryFields;
        bool                hasAncillaryElements;
        Publisher*          outQ;
        int                 rowSizeBytes;
        int                 batchRowSizeBytes;
        int                 maxRowsInGroup;
        const char*         fileName; // used locally to build file
        const char*         outputPath; // final destination of the file
        geo_data_t          geoData;

        ArrowImpl* impl; // private arrow data

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        ParquetBuilder          (lua_State* L, ArrowParms* parms,
                                                 const char* outq_name, const char* inq_name,
                                                 const char* rec_type, const char* id);
                        ~ParquetBuilder         (void);
        static void*    builderThread           (void* parm);
        bool            send2S3                 (const char* s3dst);
        bool            send2Client             (void);
};

#endif  /* __parquet_builder__ */
