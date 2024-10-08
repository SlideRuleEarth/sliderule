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
GeoRtree::GeoRtree(uint32_t _nodeCapacity):
    rtree(NULL),
    geosContext(initGEOS_r(NULL, NULL)),
    nodeCapacity(_nodeCapacity)
{
    rtree = GEOSSTRtree_create_r(geosContext, nodeCapacity);
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
void GeoRtree::query(const OGRPoint* ogrPoint, std::vector<OGRFeature*>& resultFeatures)
{
    /* Use the default GEOS context */
    query(ogrPoint, geosContext, resultFeatures);
}

/*----------------------------------------------------------------------------
 * query
 *----------------------------------------------------------------------------*/
void GeoRtree::query(const OGRPoint* ogrPoint, GEOSContextHandle_t context,
                     std::vector<OGRFeature*>& resultFeatures)
{
    /* Convert OGRPoint to GEOSGeometry */
    GEOSGeometry* geosPoint = ogrPoint->exportToGEOS(context);
    if (geosPoint == NULL)
    {
        mlog(ERROR, "Failed to convert OGRPoint to GEOSGeometry");
        return;
    }

    /* Get the envelope of the GEOS point for the R-tree query */
    GEOSGeometry* geosEnvelope = GEOSEnvelope_r(context, geosPoint);
    if (geosEnvelope == NULL)
    {
        mlog(ERROR, "Failed to get envelope of GEOS point");
        GEOSGeom_destroy_r(context, geosPoint);
        return;
    }

    /* Query the R-tree using the envelope of the point */
    GEOSSTRtree_query_r(context, rtree, geosEnvelope, queryCallback, &resultFeatures);

    /* Cleanup */
    GEOSGeom_destroy_r(context, geosPoint);
    GEOSGeom_destroy_r(context, geosEnvelope);
}

/*----------------------------------------------------------------------------
 * insert
 *----------------------------------------------------------------------------*/
bool GeoRtree::insert(OGRFeature* feature, bool asBbox)
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
        mlog(DEBUG, "Created R-tree with node capacity: %u", nodeCapacity);
    }

    /* Get the geometry of the cloned feature */
    OGRGeometry* geometry = clonedFeature->GetGeometryRef();
    if(geometry == NULL)
    {
        OGRFeature::DestroyFeature(clonedFeature);
        mlog(ERROR, "Feature has no geometry");
        return false;
    }

    bool status = false;

    if(asBbox)
    {
        /* Create a bounding box polygon from the feature's polygon
         * This is more efficient for R-tree queries but less accurate
         */
        OGREnvelope envelope;
        clonedFeature->GetGeometryRef()->getEnvelope(&envelope);
        const OGRPolygon bbox = GdalRaster::makeRectangle(envelope.MinX, envelope.MinY, envelope.MaxX, envelope.MaxY);

        /* Convert OGRGeometry to GEOSGeometry */
        GEOSGeometry* geosBbox = bbox.exportToGEOS(geosContext);
        if(geosBbox != NULL)
        {
            /* Insert the bounding box into the R-tree, use the feature as the item */
            GEOSSTRtree_insert_r(geosContext, rtree, geosBbox, static_cast<void*>(clonedFeature));
            geosGeometries.push_back(geosBbox);
            ogrFeatures.push_back(clonedFeature);
            status = true;
        }
    }
    else
    {
        /* Use the feature's geometry */
        GEOSGeometry* geosGeometry = geometry->exportToGEOS(geosContext);
        if(geosGeometry != NULL)
        {
            /* Insert the geometry into the R-tree, use the feature as the item */
            GEOSSTRtree_insert_r(geosContext, rtree, geosGeometry, static_cast<void*>(clonedFeature));

            /* Save the geometry and feature for later cleanup */
            geosGeometries.push_back(geosGeometry);
            ogrFeatures.push_back(clonedFeature);
            status = true;
        }
    }

    if(status == false)
    {
        mlog(ERROR, "Failed to insert feature into R-tree");
        OGRFeature::DestroyFeature(clonedFeature);
    }

    /* Number of geos geometries should match number of ogr features */
    assert(geosGeometries.size() == ogrFeatures.size());

    return status;
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

    /* Cleanup ogr features list */
    for(OGRFeature* feature : ogrFeatures)
    {
        OGRFeature::DestroyFeature(feature);
    }
    ogrFeatures.clear();
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
    assert(geosGeometries.size() == ogrFeatures.size());

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
    /* 'item' is a pointer to the OGRFeature in the R-tree */
    OGRFeature* feature = static_cast<OGRFeature*>(item);
    std::vector<OGRFeature*>* resultFeatures = static_cast<std::vector<OGRFeature*>*>(userdata);
    resultFeatures->push_back(feature);
}
