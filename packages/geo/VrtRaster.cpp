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

#include "VrtRaster.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void VrtRaster::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void VrtRaster::deinit (void)
{
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
VrtRaster::VrtRaster(lua_State *L, const char *dem_sampling, const int sampling_radius, const bool zonal_stats):
    GeoRaster(L, dem_sampling, sampling_radius, zonal_stats)
{
    setIndexFileType(VRT);
}

/*----------------------------------------------------------------------------
 * findTIFfilesWithPoint
 *----------------------------------------------------------------------------*/
bool VrtRaster::findRasterFilesWithPoint(OGRPoint& p)
{
    bool foundFile = false;

    try
    {
        const int32_t col = static_cast<int32_t>(floor(ris.invGeot[0] + ris.invGeot[1] * p.getX() + ris.invGeot[2] * p.getY()));
        const int32_t row = static_cast<int32_t>(floor(ris.invGeot[3] + ris.invGeot[4] * p.getX() + ris.invGeot[5] * p.getY()));

        tifList->clear();

        bool validPixel = (col >= 0) && (row >= 0) && (col < ris.dset->GetRasterXSize()) && (row < ris.dset->GetRasterYSize());
        if (!validPixel) return false;

        CPLString str;
        str.Printf("Pixel_%d_%d", col, row);

        const char *mdata = ris.band->GetMetadataItem(str, "LocationInfo");
        if (mdata == NULL) return false; /* Pixel not in VRT file */

        CPLXMLNode *root = CPLParseXMLString(mdata);
        if (root == NULL) return false;  /* Pixel is in VRT file, but parser did not find its node  */

        if (root->psChild && (root->eType == CXT_Element) && EQUAL(root->pszValue, "LocationInfo"))
        {
            for (CPLXMLNode *psNode = root->psChild; psNode != NULL; psNode = psNode->psNext)
            {
                if ((psNode->eType == CXT_Element) && EQUAL(psNode->pszValue, "File") && psNode->psChild)
                {
                    char *fname = CPLUnescapeString(psNode->psChild->pszValue, NULL, CPLES_XML);
                    CHECKPTR(fname);
                    tifList->add(fname);
                    foundFile = true; /* There may be more than one file.. */
                    CPLFree(fname);
                }
            }
        }
        CPLDestroyXMLNode(root);
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error finding raster in VRT file: %s", e.what());
    }

    return foundFile;
}

