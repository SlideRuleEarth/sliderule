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

#include "GeoUserUrlRaster.h"
#include "GeoFields.h"

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoUserUrlRaster::GeoUserUrlRaster(lua_State* L, RequestFields* rqst_parms, const char* key):
    GeoRaster(L, rqst_parms, key, getNormalizedUrl(rqst_parms, key))
{
}

/*----------------------------------------------------------------------------
 * getNormalizedUrl
 *----------------------------------------------------------------------------*/
std::string GeoUserUrlRaster::getNormalizedUrl(RequestFields* rqst_parms, const char* key)
{
    if(rqst_parms == NULL)
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to create GeoUserUrlRaster, request parameters are NULL");

    const GeoFields* parms = rqst_parms->geoFields(key);
    if(parms == NULL)
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to create GeoUserUrlRaster, geo parameters are NULL");

    if(parms->url.value.empty())
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to create GeoUserUrlRaster, samples.url is empty");

    const std::string& url = parms->url.value;
    if((url.rfind("http://", 0) != 0) && (url.rfind("https://", 0) != 0))
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to create GeoUserUrlRaster, samples.url must start with http:// or https://");

    return "/vsicurl/" + url;
}
