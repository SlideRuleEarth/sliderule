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

#ifndef __asset__
#define __asset__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Dictionary.h"
#include "List.h"
#include "LuaObject.h"

/******************************************************************************
 * ASSET INDEX CLASS
 ******************************************************************************/

class Asset: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int   RESOURCE_NAME_LENGTH = 150;
        static const char* OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            char                name[RESOURCE_NAME_LENGTH];
            Dictionary<double>  attributes{16};
        } resource_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

        virtual     ~Asset      (void);

        int         load        (resource_t& resource);
        resource_t& operator[]  (int i);

        int         size        (void);
        const char* getName     (void);
        const char* getFormat   (void);
        const char* getUrl      (void);
        const char* getIndex    (void);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*                  LuaMetaName;
        static const struct luaL_Reg        LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        const char*                         name;
        const char*                         format;
        const char*                         url;
        const char*                         index;

        List<resource_t>                    resources;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Asset       (lua_State* L, const char* _name, const char* _format, const char* _url, const char* _index);

        static int      luaInfo     (lua_State* L);
        static int      luaLoad     (lua_State* L);
};

#endif  /* __asset__ */
