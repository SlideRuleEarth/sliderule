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

#include "FileIODriver.h"
#include "OsApi.h"
#include "Asset.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* FileIODriver::FORMAT = "file";

/******************************************************************************
 * FILE IO DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
Asset::IODriver* FileIODriver::create (const Asset* _asset, const char* resource)
{
    return new FileIODriver(_asset, resource);
}

/*----------------------------------------------------------------------------
 * ioRead
 *----------------------------------------------------------------------------*/
int64_t FileIODriver::ioRead (uint8_t* data, int64_t size, uint64_t pos)
{
    /* Seek to New Position */
    if(fseek(ioFile, pos, SEEK_SET) != 0)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to go to I/O position: 0x%lx", pos);
    }

    /* Read Data */
    return fread(data, 1, size, ioFile);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
FileIODriver::FileIODriver (const Asset* _asset, const char* resource):
    asset(_asset)
{
    const FString filepath("%s/%s", asset->getPath(), resource);
    ioFile = fopen(filepath.c_str(), "r");
    if(ioFile == NULL)
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "failed to open resource");
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
FileIODriver::~FileIODriver (void)
{
    if(ioFile) fclose(ioFile);
    ioFile = NULL;
}
