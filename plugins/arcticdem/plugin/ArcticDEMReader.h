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

#ifndef __arcticdem_reader__
#define __arcticdem_reader__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "List.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "StringLib.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_PARM_POLYGON "poly"
#define LUA_PARM_LONGITUDE "lon"
#define LUA_PARM_LATITUDE "lat"

/******************************************************************************
 * ARCTICDEM READER
 ******************************************************************************/

class ArcticDEMReader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Tile Record */
        typedef struct {
            bool valid;
        } tile_t;

        /* Statistics */
        typedef struct {
            uint32_t tiles_read;
        } stats_t;

        /* Parameters */
        typedef struct {
            List<MathLib::coord_t>  polygon; // polygon of region of interest
        } dem_parms_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* tileRecType;
        static const RecordObject::fieldDef_t tileRecDef[];

        static const char* OBJECT_TYPE;

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);
        static void deinit      (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const dem_parms_t DefaultParms;

        bool                active;
        Thread*             readerPid;
        Asset*              asset;
        const char*         resource;
        bool                sendTerminator;
        Publisher*          outQ;
        dem_parms_t*        parms;
        stats_t             stats;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            ArcticDEMReader         (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, dem_parms_t* _parms, bool _send_terminator=true);
                            ~ArcticDEMReader        (void);

        static void*        subsettingThread        (void* parm);
        static dem_parms_t* getLuaDEMParms          (lua_State* L, int index);

        static int          luaStats                (lua_State* L);
};

#endif  /* __arcticdem_reader__ */
