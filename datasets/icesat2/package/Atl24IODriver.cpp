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

#include "Atl24IODriver.h"
#include "S3CurlIODriver.h"
#include "OsApi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl24IODriver::FORMAT = "s3atl24";

/******************************************************************************
 * FILE IO DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
Asset::IODriver* Atl24IODriver::create (const Asset* _asset, const char* resource)
{
    return new Atl24IODriver(_asset, resource);
}

/*----------------------------------------------------------------------------
 * Constructor
 *  Example: /ATLAS/ATL24/001/2024/11/07/ATL24_20241107234251_08052501_006_01_001_01.h5
 *----------------------------------------------------------------------------*/
Atl24IODriver::Atl24IODriver (const Asset* _asset, const char* resource):
    S3CurlIODriver(_asset)
{
    /* Build Updated Resource Path Name */
    const int NUM_ELEMENTS = 7;
    char elements[NUM_ELEMENTS][MAX_STR_SIZE];

    const int num_toks = StringLib::tokenizeLine(resource, MAX_STR_SIZE, '_', NUM_ELEMENTS, elements);
    if(num_toks < NUM_ELEMENTS) throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid ATL24 resource: %s", resource);

    const char* product = elements[0];
    const char* version = elements[5];
    const char* date = elements[1];

    char year[5];
    memcpy(&year[0], &date[0], 5);
    year[4] = '\0';

    char month[3];
    memcpy(&month[0], &date[4], 3);
    month[2] = '\0';

    char day[3];
    memcpy(&day[0], &date[6], 3);
    day[2] = '\0';

    const FString resourcepath("%s/ATLAS/%s/%s/%s/%s/%s/%s", asset->getPath(), product, version, year, month, day, resource);

    /*
     * Determine ioBucket and ioKey
     */
    ioBucket = const_cast<char*>(resourcepath.c_str(true));

    /*
    * Differentiate Bucket and Key
    *  <bucket_name>/<path_to_file>/<filename>
    *  |             |
    * ioBucket      ioKey
    */
    ioKey = ioBucket;
    while(*ioKey != '\0' && *ioKey != '/') ioKey++;
    if(*ioKey == '/') *ioKey = '\0';
    else throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid S3 url: %s", resource);
    ioKey++;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl24IODriver::~Atl24IODriver (void) = default;
