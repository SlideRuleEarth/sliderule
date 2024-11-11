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

#include "LuaObject.h"
#include "MathLib.h"
#include "List.h"
#include "RequestFields.h"
#include "GeoFields.h"
#include "RasterSample.h"
#include "RasterSubset.h"
#include "RasterFileDictionary.h"

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
        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef RasterObject* (*factory_f) (lua_State* L, RequestFields* rqst_parms, const char* key);

        typedef struct
        {
            factory_f   create;
        } factory_t;

        typedef struct
        {
            MathLib::point_3d_t point;
            double              gps;
        } point_info_t;

        typedef struct
        {
            uint32_t   start;
            uint32_t   end;
        } range_t;

        typedef List<RasterSample*> sample_list_t;
        typedef struct Reader
        {
            RasterObject*                     robj;
            range_t                           range;
            const std::vector<point_info_t>&  points;
            std::vector<sample_list_t*>       samples;
            uint32_t                          ssErrors;

            explicit Reader (RasterObject* _robj, const std::vector<point_info_t>& _points);
                    ~Reader (void);
        } reader_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void          init            (void);
        static void          deinit          (void);
        static int           luaCreate       (lua_State* L);
        static bool          registerRaster  (const char* _name, factory_f create);
        virtual uint32_t     getSamples      (const MathLib::point_3d_t& point, int64_t gps, List<RasterSample*>& slist, void* param=NULL) = 0;
        virtual uint32_t     getSamples      (const std::vector<point_info_t>& points, List<sample_list_t*>& sllist, void* param=NULL);
        virtual uint32_t     getSubsets      (const MathLib::extent_t&  extent, int64_t gps, List<RasterSubset*>& slist, void* param=NULL) = 0;
        virtual uint8_t*     getPixels       (uint32_t ulx, uint32_t uly, uint32_t xsize=0, uint32_t ysize=0, void* param=NULL);
                            ~RasterObject    (void) override;

        bool hasBands (void) const
        {
            return parms->bands.length() > 0;
        }

        bool hasZonalStats (void) const
        {
            return parms->zonal_stats;
        }

        bool hasFlags (void) const
        {
            return parms->flags_file;
        }

        bool usePOItime(void) const
        {
            return parms->use_poi_time;
        }

        void stopSampling(void);

        const char* fileDictGet(uint64_t fileId)
        {
            return fileDict.get(fileId);
        }

        const std::set<uint64_t>& fileDictGetSampleIds(void)
        {
            return fileDict.getSampleIds();
        }

        RasterFileDictionary fileDictCopy(void)
        {
            return fileDict.copy();
        }

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void lockSampling(void)
        {
            samplingMut.lock();
        }

        void unlockSampling(void)
        {
            samplingMut.unlock();
        }

        bool sampling(void)
        {
            return samplingEnabled;
        };


                    RasterObject    (lua_State* L, RequestFields* rqst_parms, const char* key);
        static int  luaSamples      (lua_State* L);
        static int  luaSubsets      (lua_State* L);
        static int  luaPixels       (lua_State *L);

        static RasterObject* cppCreate(RequestFields* rqst_parms, const char* key);
        static RasterObject* cppCreate(const RasterObject* obj);

        static void getThreadsRanges(std::vector<range_t>& ranges, uint32_t num,
                                     uint32_t minPerThread, uint32_t maxNumThreads);

        void       fileDictSetSamples(List<RasterSample*>* slist);


        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        RequestFields* rqstParms;
        const GeoFields* parms;
        const char* samplerKey;
        RasterFileDictionary fileDict;

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      slist2table  (const List<RasterSubset*>& slist, uint32_t errors, lua_State* L);

        static void*    readerThread (void* parm);
        static uint32_t readSamples  (RasterObject* robj, const range_t& range,
                                      const std::vector<point_info_t>& points, std::vector<sample_list_t*>& samples);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static Mutex                    factoryMut;
        static Dictionary<factory_t>    factories;

        Mutex                           readersMut;
        Mutex                           samplingMut;
        std::atomic<bool>               samplingEnabled;  /* Used by batch getSample code */
        std::vector<reader_t*>          readers;
};

#endif  /* __raster_object__ */
