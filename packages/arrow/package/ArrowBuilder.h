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

#ifndef __arrow_builder__
#define __arrow_builder__

/*
 * ArrowBuilder works on batches of records.  It expects the `batch_rec_type`
 * passed into the constructor to be the type that defines each of the column headings,
 * then it expects to receive records that are arrays (or batches) of that record
 * type.  The field defined as an array is transparent to this class - it just
 * expects the record to be a single array.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "RecordObject.h"
#include "RequestFields.h"
#include "OsApi.h"
#include "MsgQ.h"

/******************************************************************************
 * FORWARD DECLARATIONS
 ******************************************************************************/

class ArrowBuilderImpl; // arrow implementation

/******************************************************************************
 * CLASS
 ******************************************************************************/

class ArrowBuilder: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int ROW_GROUP_SIZE = 0x4000000; // 64MB
        static const int QUEUE_BUFFER_FACTOR = 3;

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

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
            RecordObject::field_t   x_field;
            RecordObject::field_t   y_field;
        } geo_data_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int              luaCreate       (lua_State* L);
        static void             init            (void);
        static void             deinit          (void);

        static const char*      getSubField     (const char* field_name);
        const char*             getFileName     (void);
        const char*             getDataFile     (void);
        const char*             getMetadataFile (void);
        const char*             getRecType      (void);
        const char*             getTimeKey      (void);
        const char*             getXKey         (void);
        const char*             getYKey         (void);
        RecordObject::field_t&  getXField       (void);
        RecordObject::field_t&  getYField       (void);
        const ArrowFields*      getParms        (void);
        bool                    hasAncFields    (void) const;
        bool                    hasAncElements  (void) const;
        const string&           getParmsAsString(void);
        const char*             getEndpoint     (void);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int EXPECTED_RECORDS_IN_BATCH = 256;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Thread*             builderPid;
        RequestFields*      rqstParms;
        const ArrowFields&  parms;
        bool                active;
        Subscriber*         inQ;
        const char*         recType;
        const char*         timeKey;
        const char*         xKey;
        const char*         yKey;
        batch_list_t        recordBatch;
        bool                hasAncillaryFields;
        bool                hasAncillaryElements;
        Publisher*          outQ;
        int                 rowSizeBytes;
        int                 batchRowSizeBytes;
        int                 maxRowsInGroup;
        geo_data_t          geoData;
        const char*         dataFile;           // used locally to build data file
        const char*         metadataFile;       // used locally to build json metadata file
        const char*         outputMetadataPath; // final destination of the metadata file
        const string        parmsAsString;
        const char*         endpoint;
        const bool          keepLocal;

        ArrowBuilderImpl*   impl; // private arrow data

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        ArrowBuilder            (lua_State* L, RequestFields* parms,
                                                 const char* outq_name, const char* inq_name,
                                                 const char* rec_type, const char* id,
                                                 const char* _endpoint, const bool keep_local);
                        ~ArrowBuilder           (void) override;
        static void*    builderThread           (void* parm);
        static int      luaGetFileNames         (lua_State* L);
};

#endif  /* __arrow_builder__ */
