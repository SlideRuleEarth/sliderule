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

#ifndef __swotl2_reader__
#define __swotl2_reader__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "List.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "StringLib.h"
#include "H5Array.h"

#include "SwotParms.h"

/******************************************************************************
 * SWOT L2 READER
 ******************************************************************************/
class SwotL2Reader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const char* varRecType;
        static const RecordObject::fieldDef_t varRecDef[];

        static const char* scanRecType;
        static const RecordObject::fieldDef_t scanRecDef[];

        static const char* geoRecType;
        static const RecordObject::fieldDef_t geoRecDef[];

        static const int MAX_GRANULE_NAME_STR = 128;
        static const int MAX_VARIABLE_NAME_STR = 128;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Variable Record */
        typedef struct {
            char                granule[MAX_GRANULE_NAME_STR];
            char                variable[MAX_VARIABLE_NAME_STR];
            uint32_t            datatype; // RecordObject::fieldType_t
            uint32_t            elements; // number of values
            uint32_t            width; // in elements
            uint32_t            size; // total size in bytes
        } var_rec_t;

        /* Scan Record */
        typedef struct {
            double              latitude;
            double              longitude;
        } scan_rec_t;

        /* Geo Record */
        typedef struct {
            scan_rec_t          scan[1];
        } geo_rec_t;

        /* Statistics */
        typedef struct {
            uint32_t            variables_read;
            uint32_t            variables_filtered;
            uint32_t            variables_sent;
            uint32_t            variables_dropped;
            uint32_t            variables_retried;
        } stats_t;

        /* Thread Information */
        typedef struct {
            SwotL2Reader*       reader;
            const char*         variable_name;
        } info_t;

        /* Region Subclass */
        struct Region
        {
            Region              (Asset* asset, const char* resource, SwotParms* parms, H5Coro::context_t* context);
            ~Region             (void);

            void cleanup        (void);
            void polyregion     (SwotParms* parms);
            void rasterregion   (SwotParms* parms);

            const int           read_timeout_ms;

            H5Array<int32_t>    lat;
            H5Array<int32_t>    lon;

            bool*               inclusion_mask;
            bool*               inclusion_ptr;

            long                first_line;
            long                num_lines;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        H5Coro::context_t       context;
        Region                  region;
        bool                    active;
        Thread**                varPid;
        Thread*                 geoPid;
        Mutex                   threadMut;
        int                     threadCount;
        int                     numComplete;
        Asset*                  asset;
        const char*             resource;
        bool                    sendTerminator;
        const int               read_timeout_ms;
        Publisher*              outQ;
        SwotParms*              parms;
        stats_t                 stats;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        SwotL2Reader        (lua_State* L, Asset* _asset, const char* _resource,
                                             const char* outq_name, SwotParms* _parms, bool _send_terminator=true);
                        ~SwotL2Reader       (void);
        static void*    varThread           (void* parm);
        static void*    geoThread           (void* parm);
        static int      luaStats            (lua_State* L);
};

#endif  /* __swotl2_reader__ */
