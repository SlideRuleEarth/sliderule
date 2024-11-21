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

#ifndef __raster_sample__
#define __raster_sample__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"

/******************************************************************************
 * RASTER SAMPLE CLASS
 ******************************************************************************/

class RasterSample
{
public:
    double value;
    double time;   // gps seconds
    double verticalShift;
    uint64_t fileId;
    uint32_t flags;
    std::string bandName;

    struct zonal_t
    {
        uint32_t count;
        double min;
        double max;
        double mean;
        double median;
        double stdev;
        double mad;
    } stats;

   RasterSample(double _time, double _fileId, double _verticalShift = 0.0):
    value(0),
    time(_time),
    verticalShift(_verticalShift),
    fileId(_fileId),
    flags(0),
    stats{.count = 0,
          .min = 0.0,
          .max = 0.0,
          .mean = 0.0,
          .median = 0.0,
          .stdev = 0.0,
          .mad = 0.0}
    {
    }

    RasterSample(const RasterSample& sample): RasterSample(sample.time, sample.fileId, sample.verticalShift)
    {
        value = sample.value;
        flags = sample.flags;
        bandName = sample.bandName;
        stats = sample.stats;
    }

    std::string toString(void) const
    {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "time: %.2lf, value: %.2lf, verticalShift: %.2lf, fileId: %lu, flags: %u, stats: {count: %u, min: %.2lf, max: %.2lf, mean: %.2lf, median: %.2lf, stdev: %.2lf, mad: %.2lf}",
            time, value, verticalShift, fileId, flags, stats.count, stats.min, stats.max, stats.mean, stats.median, stats.stdev, stats.mad);
        return std::string(buffer);
    }

};

#endif  /* __raster_sample__ */
