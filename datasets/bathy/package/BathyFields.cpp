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

#include "BathyFields.h"
#include "StringLib.h"

/******************************************************************************
 * BATHY FIELDS NAMESPACE
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * str2classifier
 *----------------------------------------------------------------------------*/
BathyFields::classifier_t BathyFields::str2classifier (const char* str)
{
    if(StringLib::match(str, "qtrees"))             return QTREES;
    if(StringLib::match(str, "coastnet"))           return COASTNET;
    if(StringLib::match(str, "openoceans++"))       return OPENOCEANSPP;
    if(StringLib::match(str, "medianfilter"))       return MEDIANFILTER;
    if(StringLib::match(str, "cshelph"))            return CSHELPH;
    if(StringLib::match(str, "bathypathfinder"))    return BATHYPATHFINDER;
    if(StringLib::match(str, "pointnet2"))          return POINTNET;
    if(StringLib::match(str, "openoceans"))         return OPENOCEANS;
    if(StringLib::match(str, "ensemble"))           return ENSEMBLE;
    return BathyFields::INVALID_CLASSIFIER;
}

/*----------------------------------------------------------------------------
 * classifier2str
 *----------------------------------------------------------------------------*/
const char* BathyFields::classifier2str (classifier_t classifier)
{
    switch(classifier)
    {
        case QTREES:            return "qtrees";
        case COASTNET:          return "coastnet";
        case OPENOCEANSPP:      return "openoceans++";
        case MEDIANFILTER:      return "medianfilter";
        case CSHELPH:           return "cshelph";
        case BATHYPATHFINDER:   return "bathypathfinder";
        case POINTNET:          return "pointnet";
        case OPENOCEANS:        return "openoceans";
        case ENSEMBLE:          return "ensemble";
        default:                return "unknown";
    }
}
