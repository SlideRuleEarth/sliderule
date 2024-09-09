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

#include "GediIODriver.h"
#include "S3CurlIODriver.h"
#include "OsApi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GediIODriver::FORMAT = "s3gedi";

/******************************************************************************
 * FILE IO DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
Asset::IODriver* GediIODriver::create (const Asset* _asset, const char* resource)
{
    return new GediIODriver(_asset, resource);
}

/*----------------------------------------------------------------------------
 * Constructor
 *
 *  Examples:
 *      /GEDI02_A.002/GEDI02_A_2023075201011_O24115_03_T08796_02_003_02_V002/GEDI02_A_2023075201011_O24115_03_T08796_02_003_02_V002.h5
 *      /GEDI01_B.002/GEDI01_B_2023075201011_O24115_04_T08796_02_005_02_V002/GEDI01_B_2023075201011_O24115_04_T08796_02_005_02_V002.h5
 *----------------------------------------------------------------------------*/
GediIODriver::GediIODriver (const Asset* _asset, const char* resource):
    S3CurlIODriver(_asset)
{
    /* Build Updated Resource Path Name */
    const int NUM_ELEMENTS = 10;
    char elements[NUM_ELEMENTS][MAX_STR_SIZE];
    char version_buffer[8];
    char resource_buffer[57];

    const int num_toks = StringLib::tokenizeLine(resource, MAX_STR_SIZE, '_', NUM_ELEMENTS, elements);
    if(num_toks < NUM_ELEMENTS) throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid gedi s3 resource: %s", resource);

    const char* product = elements[0];
    const char* level = elements[1];
    StringLib::copy(version_buffer, elements[9], 8);
    version_buffer[4] = '\0';
    const char* version = &version_buffer[1];

    StringLib::copy(resource_buffer, resource, 57);
    resource_buffer[54] = '\0';
    const char* subdirectory = resource_buffer;

    FString resourcepath("%s/%s_%s.%s/%s/%s", asset->getPath(), product, level, version, subdirectory, resource);

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
    else throw RunTimeException(CRITICAL, RTE_ERROR, "invalid S3 url: %s", resource);
    ioKey++;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GediIODriver::~GediIODriver (void) = default;
