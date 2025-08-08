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

#ifndef __geo_rtree__
#define __geo_rtree__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <ogrsf_frmts.h>
#include "OsApi.h"
#include "MathLib.h"

/* GEOS C++ API is unstable, use C API */
#include <geos_c.h>

/******************************************************************************
 * GEO RTREE CLASS
 ******************************************************************************/

class GeoRtree
{
public:

    /*--------------------------------------------------------------------
     * Typedefs
     *--------------------------------------------------------------------*/

     /*--------------------------------------------------------------------
      * Methods
      *--------------------------------------------------------------------*/

    static GEOSContextHandle_t init(void);
    static void deinit(GEOSContextHandle_t geosContext);

    explicit  GeoRtree(bool sort, uint32_t nodeCapacity = 10);
             ~GeoRtree(void);

    void query (const OGRGeometry* geo, std::vector<OGRFeature*>& resultFeatures);
    void query (const OGRGeometry* geo, GEOSContextHandle_t geosContext, std::vector<OGRFeature*>& resultFeatures);

    bool insert(OGRFeature* feature);
    void clear (void);
    bool empty (void);
    uint32_t size(void) const { return geosGeometries.size(); }


private:

    /*--------------------------------------------------------------------
     * Typedefs
     *--------------------------------------------------------------------*/
    struct FeatureIndexPair {
        OGRFeature* feature;
        uint32_t    index;

        FeatureIndexPair(OGRFeature* _feature, uint32_t _index) : feature(_feature), index(_index) {}
    };

    /*--------------------------------------------------------------------
    * Data
    *--------------------------------------------------------------------*/

    GEOSSTRtree* rtree;
    GEOSContextHandle_t            geosContext;
    std::vector<GEOSGeometry*>     geosGeometries;
    std::vector<FeatureIndexPair*> ogrFeaturePairs;
    uint32_t                       nodeCapacity;
    bool                           sort;

    /*--------------------------------------------------------------------
    * Methods
    *--------------------------------------------------------------------*/

    static void queryCallback(void* item, void* userdata);
};

#endif  /* __geo_rtree__ */
