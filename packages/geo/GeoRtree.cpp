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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoRtree.h"
#include "GdalRaster.h"
#include <algorithm>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
GEOSContextHandle_t GeoRtree::init (void)
{
    return initGEOS_r(NULL, NULL);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void GeoRtree::deinit (GEOSContextHandle_t geosContext)
{
    finishGEOS_r(geosContext);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoRtree::GeoRtree(bool _sort, uint32_t _nodeCapacity):
    rtree(NULL),
    geosContext(initGEOS_r(NULL, NULL)),
    nodeCapacity(_nodeCapacity),
    sort(_sort)
{
    rtree = GEOSSTRtree_create_r(geosContext, nodeCapacity);
    mlog(DEBUG, "Created R-tree with node capacity: %u, index sorting: %d", nodeCapacity, static_cast<int>(sort));
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoRtree::~GeoRtree(void)
{
    clear();
    finishGEOS_r(geosContext);
}

/*----------------------------------------------------------------------------
 * query
 *----------------------------------------------------------------------------*/
void GeoRtree::query(const OGRGeometry* geo, std::vector<OGRFeature*>& resultFeatures)
{
    /* Use the default GEOS context */
    query(geo, geosContext, resultFeatures);
}

/*----------------------------------------------------------------------------
 * query
 *----------------------------------------------------------------------------*/
void GeoRtree::query(const OGRGeometry* geo, GEOSContextHandle_t context,
                     std::vector<OGRFeature*>& resultFeatures)
{
    /* Convert OGRGeometry to GEOSGeometry */
    GEOSGeometry* geos = geo->exportToGEOS(context);
    if (geos == NULL)
    {
        mlog(ERROR, "Failed to convert OGRPoint to GEOSGeometry");
        return;
    }

    /* Get the envelope of the GEOS geometry for the R-tree query */
    GEOSGeometry* geosEnvelope = GEOSEnvelope_r(context, geos);
    if (geosEnvelope == NULL)
    {
        mlog(ERROR, "Failed to get envelope of GEOS point");
        GEOSGeom_destroy_r(context, geos);
        return;
    }


    /* Query the R-tree using the envelope of the point */
    std::vector<FeatureIndexPair*> resultPairs;
    GEOSSTRtree_query_r(context, rtree, geosEnvelope, queryCallback, &resultPairs);

    // mlog(DEBUG, "Found %zu features for query", resultPairs.size());

    if(sort)
    {
        std::sort(resultPairs.begin(), resultPairs.end(),
            [](const FeatureIndexPair* a, const FeatureIndexPair* b) { return a->index < b->index; });
    }

    /* Extract the features from the sorted pairs */
    for (FeatureIndexPair* pair : resultPairs)
    {
        resultFeatures.push_back(pair->feature);
    }

    /* Cleanup */
    GEOSGeom_destroy_r(context, geos);
    GEOSGeom_destroy_r(context, geosEnvelope);
}

/*----------------------------------------------------------------------------
 * insert
 *----------------------------------------------------------------------------*/
bool GeoRtree::insert(OGRFeature* feature)
{
    /* Clone the feature */
    OGRFeature* clonedFeature = feature->Clone();
    if(clonedFeature == NULL)
    {
        mlog(ERROR, "Failed to clone feature");
        return false;
    }

    /* Create the R-tree if it doesn't exist */
    if(rtree == NULL)
    {
        rtree = GEOSSTRtree_create_r(geosContext, nodeCapacity);
        if(rtree == NULL)
        {
            mlog(CRITICAL, "Failed to create R-tree");
            OGRFeature::DestroyFeature(clonedFeature);
            return false;
        }
        mlog(DEBUG, "Created R-tree with node capacity: %u, index sorting: %d", nodeCapacity, static_cast<int>(sort));
    }


    /* Create a bounding box polygon from the feature's polygon
     * This is more efficient for R-tree queries but less accurate
     */
    OGREnvelope envelope;
    clonedFeature->GetGeometryRef()->getEnvelope(&envelope);
    const OGRPolygon bbox = GdalRaster::makeRectangle(envelope.MinX, envelope.MinY, envelope.MaxX, envelope.MaxY);

    /* Convert OGRGeometry to GEOSGeometry */
    GEOSGeometry* geosBbox = bbox.exportToGEOS(geosContext);
    if(geosBbox == NULL)
    {
        mlog(CRITICAL, "Failed to convert OGRPolygon to GEOSGeometry");
        OGRFeature::DestroyFeature(clonedFeature);
        return false;
    }

    /* Insert the bounding box into the R-tree, use the feature index pair as the item */
    FeatureIndexPair* featurePair = new FeatureIndexPair(clonedFeature, ogrFeaturePairs.size());
    GEOSSTRtree_insert_r(geosContext, rtree, geosBbox, static_cast<void*>(featurePair));
    geosGeometries.push_back(geosBbox);
    ogrFeaturePairs.push_back(featurePair);

    /* Number of geos geometries should match number of ogr feature pairs */
    assert(geosGeometries.size() == ogrFeaturePairs.size());

    return true;
}

/*----------------------------------------------------------------------------
 * clear
 *----------------------------------------------------------------------------*/
void GeoRtree::clear(void)
{
    /* Cleanup GEOS geometries */
    for(GEOSGeometry* geom : geosGeometries)
    {
        GEOSGeom_destroy_r(geosContext, geom);
    }
    geosGeometries.clear();

    /* The easiest way to cleanup the R-tree is to destroy it and create a new one */
    GEOSSTRtree_destroy_r(geosContext, rtree);
    rtree = NULL;

    /* Cleanup ogr feature pairs */
    for(FeatureIndexPair* fp : ogrFeaturePairs)
    {
        OGRFeature::DestroyFeature(fp->feature);
        delete fp;
    }
    ogrFeaturePairs.clear();
}

/*----------------------------------------------------------------------------
 * empty
 *----------------------------------------------------------------------------*/
bool GeoRtree::empty(void)
{
    /* The GEOS R-tree does not have an empty method or count/size method
     * vectors are used to keep track of the geometries and features
     * inserted into the R-tree, they must be the same size
     */
    assert(geosGeometries.size() == ogrFeaturePairs.size());
    return geosGeometries.empty();
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * queryCallback
 *----------------------------------------------------------------------------*/
void GeoRtree::queryCallback(void* item, void* userdata)
{
    /* 'item' is a pointer to the FeatureIndexPair in the R-tree */
    FeatureIndexPair* featurePair = static_cast<FeatureIndexPair*>(item);

    /* Cast userdata to a vector of FeatureIndexPair* and store the pair */
    std::vector<FeatureIndexPair*>* resultPairs = static_cast<std::vector<FeatureIndexPair*>*>(userdata);
    resultPairs->push_back(featurePair);
}
