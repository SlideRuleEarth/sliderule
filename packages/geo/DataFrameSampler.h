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

#ifndef __dataframe_sampler__
#define __dataframe_sampler__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoDataFrame.h"
#include "RequestFields.h"
#include "RasterObject.h"
#include "OsApi.h"

#include <set>

/******************************************************************************
 * CLASS
 ******************************************************************************/

class DataFrameSampler: public GeoDataFrame::FrameRunner
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/
        static const char* OBJECT_TYPE;
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/
        typedef RasterObject::sample_list_t sample_list_t;
        typedef RasterObject::point_info_t point_info_t;

        typedef struct {
            const char*    rkey;
            RasterObject*  robj;
        } raster_info_t;

        struct sampler_info_t {
            const char* rkey;
            RasterObject* robj;
            DataFrameSampler* obj;
            const GeoFields& geoparms;
            List<sample_list_t*> samples;
            vector<std::pair<uint64_t, const char*>> filemap;
            sampler_info_t (const char* _rkey, RasterObject* _robj, DataFrameSampler* _obj, const GeoFields& _geoparms):
                robj(_robj),
                obj(_obj),
                rkey(StringLib::duplicate(_rkey)),
                geoparms(_geoparms) {};
            ~sampler_info_t (void) {
                delete [] rkey;
                robj->releaseLuaObject(); };
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate           (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    DataFrameSampler        (lua_State* L, RequestFields* _parms);
                    ~DataFrameSampler       (void) override;

        bool        run                     (GeoDataFrame* dataframe) override;
        bool        populatePoints          (GeoDataFrame* dataframe);
        static bool populateMultiColumns    (GeoDataFrame* dataframe, sampler_info_t* sampler);
        static bool populateColumns         (GeoDataFrame* dataframe, sampler_info_t* sampler);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

         RequestFields*             parms;
         vector<point_info_t>       points;
         vector<sampler_info_t*>    samplers;
     };

#endif  /* __dataframe_sampler__*/
