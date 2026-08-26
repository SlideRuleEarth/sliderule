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

#ifndef __arrow_types__
#define __arrow_types__

#include <cstring>
#include <stdexcept>

#include "OsApi.h"

typedef struct WKBPoint {
    uint8_t     byteOrder;
    uint32_t    wkbType;
    double      x;
    double      y;
} ALIGN_PACKED wkbpoint_t;

/*----------------------------------------------------------------------------
* convertWKBToPoint
*----------------------------------------------------------------------------*/
inline wkbpoint_t convertWKBToPoint(const string& wkb_data)
{
    wkbpoint_t point;

    if(wkb_data.size() < sizeof(wkbpoint_t))
    {
        throw std::runtime_error("Invalid WKB data size.");
    }

    // Byte order is the first byte
    size_t offset = 0;
    std::memcpy(&point.byteOrder, wkb_data.data() + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);

    // Next four bytes are wkbType
    std::memcpy(&point.wkbType, wkb_data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Convert to host byte order if necessary
    if(point.byteOrder == 0)
    {
        // Big endian
        point.wkbType = be32toh(point.wkbType);
    }
    else if(point.byteOrder == 1)
    {
        // Little endian
        point.wkbType = le32toh(point.wkbType);
    }
    else throw std::runtime_error("Unknown byte order.");

    // Next eight bytes are x coordinate
    std::memcpy(&point.x, wkb_data.data() + offset, sizeof(double));
    offset += sizeof(double);

    if((point.byteOrder == 0 && __BYTE_ORDER != __BIG_ENDIAN) ||
       (point.byteOrder == 1 && __BYTE_ORDER != __LITTLE_ENDIAN))
    {
        point.x = OsApi::swaplf(point.x);
    }

    // Next eight bytes are y coordinate
    std::memcpy(&point.y, wkb_data.data() + offset, sizeof(double));
    if((point.byteOrder == 0 && __BYTE_ORDER != __BIG_ENDIAN) ||
       (point.byteOrder == 1 && __BYTE_ORDER != __LITTLE_ENDIAN))
    {
        point.y = OsApi::swaplf(point.y);
    }

    return point;
}

#endif