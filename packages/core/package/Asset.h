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
 * DEFINES
 ******************************************************************************/

#ifndef ASSET_STARTING_ATTRIBUTES_PER_RESOURCE
#define ASSET_STARTING_ATTRIBUTES_PER_RESOURCE  4
#endif

#ifndef ASSET_STARTING_RESOURCES_PER_INDEX
#define ASSET_STARTING_RESOURCES_PER_INDEX      16
#endif

/******************************************************************************
 * ASSET INDEX CLASS
 ******************************************************************************/

class Asset: public LuaObject
{
    public:

        /**********************************************************************
         * IO DRIVER SUBCLASS
         **********************************************************************/
        class IODriver
        {
            public:
                static IODriver*    create      (const Asset* _asset, const char* resource);
                                    IODriver    (void);
                virtual             ~IODriver   (void);
                virtual int64_t     ioRead      (uint8_t* data, int64_t size, uint64_t pos);
        };

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int    RESOURCE_NAME_LENGTH = 150;
        static const char*  OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct _resource_t {
            char                name[RESOURCE_NAME_LENGTH];
            Dictionary<double>  attributes{ASSET_STARTING_ATTRIBUTES_PER_RESOURCE};
        } resource_t;

        typedef IODriver* (*io_driver_f) (const Asset* _asset, const char* resource);

        typedef struct {
            io_driver_f factory;
        } io_driver_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate       (lua_State* L);
        static bool     registerDriver  (const char* _format, io_driver_f factory);
        static int      luaDrivers      (lua_State* L);

        IODriver*       createDriver    (const char* resource) const;

                        ~Asset          (void) override;

        int             load            (const resource_t& resource);
        resource_t&     operator[]      (int i);

        int             size            (void) const;
        const char*     getName         (void) const;
        const char*     getIdentity     (void) const;
        const char*     getDriver       (void) const;
        const char*     getPath         (void) const;
        const char*     getIndex        (void) const;
        const char*     getRegion       (void) const;
        const char*     getEndpoint     (void) const;

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char*              LUA_META_NAME;
        static const struct luaL_Reg    LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            const char*                 name;
            const char*                 identity;
            const char*                 driver;
            const char*                 path;
            const char*                 index;
            const char*                 region;
            const char*                 endpoint;
        } attributes_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Mutex                    ioDriverMut;
        static Dictionary<io_driver_t>  ioDrivers;

        attributes_t                    attributes;
        io_driver_t                     driver;

        List<resource_t>                resources;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Asset       (lua_State* L, const attributes_t& _attributes, const io_driver_t& _io_driver);

        static int      luaInfo     (lua_State* L);
        static int      luaLoad     (lua_State* L);
};

#endif  /* __asset__ */
