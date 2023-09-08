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

#ifndef __raster_object__
#define __raster_object__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <vector>
#include "LuaObject.h"
#include "GeoParms.h"
#include "RasterSample.h"

/******************************************************************************
 * RASTER OBJECT CLASS
 ******************************************************************************/

class RasterObject: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef RasterObject* (*factory_t) (lua_State* L, GeoParms* _parms);

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void      init            (void);
        static void      deinit          (void);
        static int       luaCreate       (lua_State* L);
        static bool      registerRaster  (const char* _name, factory_t create);
        virtual void     getSamples      (double lon, double lat, double height, int64_t gps, std::vector<RasterSample>& slist, void* param=NULL) = 0;
        virtual void     getSubsets      (double lon_min, double lat_min, double lon_max, double lat_max, int64_t gps, std::vector<RasterSubset>& slist, void* param=NULL) = 0;
        virtual         ~RasterObject    (void);

        inline bool hasZonalStats (void)
        {
            return parms->zonal_stats;
        }

        inline const Dictionary<uint64_t>& fileDictGet(void)
        {
            return fileDict;
        }

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    RasterObject    (lua_State* L, GeoParms* _parms);
        uint64_t    fileDictAdd     (const std::string& fileName);
        static int  luaSamples      (lua_State* L);
        static int  luaSubset       (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        GeoParms* parms;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Mutex                    factoryMut;
        static Dictionary<factory_t>    factories;
        Dictionary<uint64_t>            fileDict;
};

#endif  /* __raster_object__ */
