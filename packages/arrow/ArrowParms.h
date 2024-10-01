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

#ifndef __arrow_parms__
#define __arrow_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "Asset.h"

#ifdef __aws__
#include "aws.h"
#endif

/******************************************************************************
 * ARROW PARAMETERS CLASS
 ******************************************************************************/

class ArrowParms: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/
        typedef enum {
            NATIVE = 0,
            FEATHER = 1,
            PARQUET = 2,
            CSV = 3,
            UNSUPPORTED = 4
        } format_t;

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        static const char* SELF;
        static const char* PATH;
        static const char* FORMAT;
        static const char* OPEN_ON_COMPLETE;
        static const char* AS_GEO;
        static const char* WITH_CHECKSUM;
        static const char* ANCILLARY;
        static const char* ASSET;
        static const char* REGION;
        static const char* CREDENTIALS;

        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        const char*     path;                           // file system path to the file (includes filename)
        format_t        format;                         // format of the file
        bool            open_on_complete;               // flag to client to open file on completion
        bool            as_geo;                         // whether to create a standard geo-based formatted file
        bool            with_checksum;                  // whether to perform checksum on file and send EOF record
        const char*     asset_name;
        const char*     region;
        vector<string>  ancillary_fields;

        #ifdef __aws__
        CredentialStore::Credential credentials;
        #endif

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        static int  luaCreate           (lua_State* L);
                    ArrowParms          (lua_State* L, int index);
                    ~ArrowParms         (void) override;
        const char* tojson              (void) const override;

    private:

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        void            cleanup             (void);
        static format_t str2outputformat    (const char* fmt_str);
        static int      luaIsNative         (lua_State* L);
        static int      luaIsFeather        (lua_State* L);
        static int      luaIsParquet        (lua_State* L);
        static int      luaIsCSV            (lua_State* L);
        static int      luaIsArrow          (lua_State* L);
        static int      luaPath             (lua_State* L);
        void            luaGetAncillary     (lua_State* L, int index, bool* provided);
        static const char*format2str        (format_t fmt);
};

#endif  /* __arrow_parms__ */
