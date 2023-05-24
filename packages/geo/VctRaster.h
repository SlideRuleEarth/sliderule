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

#ifndef __vct_raster__
#define __vct_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoRaster.h"
#include "GeoParms.h"

/******************************************************************************
 * VCT (Vector) RASTER CLASS
 ******************************************************************************/

class VctRaster: public GeoRaster
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void init    (void);
        static void deinit  (void);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                     VctRaster         (lua_State* L, GeoParms* _parms);
        virtual     ~VctRaster         (void);
        void         openGeoIndex      (double lon=0, double lat=0) override;
        virtual void getIndexFile      (std::string& file, double lon, double lat) = 0;
        virtual void getIndexBbox      (bbox_t& bbox, double lon=0, double lat=0);
        bool         findCachedRasters (OGRPoint &p) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/
        OGRLayer *layer;
        List<OGRFeature*> featuresList;
};

#endif  /* __vct_raster__ */
