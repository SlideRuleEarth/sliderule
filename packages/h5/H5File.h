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

#ifndef __h5_file__
#define __h5_file__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "H5Coro.h"
#include "Asset.h"

/******************************************************************************
 * HDF5 FILE CLASS
 ******************************************************************************/

class H5File: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_NAME_STR = H5CORO_MAXIMUM_NAME_SIZE;

        static const char* ObjectType;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const char* recType;
        static const RecordObject::fieldDef_t recDef[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            char        dataset[MAX_NAME_STR];
            uint32_t    datatype; // RecordObject::valType_t
            uint32_t    elements; // number of values
            uint32_t    size; // total size in bytes
        } h5file_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate           (lua_State* L);
        static void         init                (void);

    protected:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            const char*             dataset;
            RecordObject::valType_t valtype;
            long                    col;
            long                    startrow;
            long                    numrows;
            const char*             outqname;
            H5File*                 h5file;
        } dataset_info_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            H5File              (lua_State* L, Asset* _asset, const char* _resource);
        virtual             ~H5File             (void);

        static void*        readThread          (void* parm);

        static int          luaRead             (lua_State* L);
        static int          luaTraverse         (lua_State* L);
        static int          luaInspect          (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Asset*              asset;
        const char*         resource;
        H5Coro::context_t   context;
};

#endif  /* __h5_file__ */
