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
#include "EventLib.h"
#include <gdal.h>

/******************************************************************************
 * RASTER SAMPLE CLASS
 ******************************************************************************/

class RasterSample
{
public:
    double value;
    double time;   // gps seconds
    uint64_t fileId;
    uint32_t flags;

    struct
    {
        uint32_t count;
        double min;
        double max;
        double mean;
        double median;
        double stdev;
        double mad;
    } stats;

   void clear (void)
   {
       value  = 0;
       time   = 0;
       fileId = 0;
       flags  = 0;
       bzero(&stats, sizeof(stats));
   }

   RasterSample(double _value = 0, double _time = 0, double _fileId = 0, double _flags = 0):
    value(_value),
    time(_time),
    fileId(_fileId),
    flags(_flags) {}
};

class RasterSubset
{
public:
    std::shared_ptr<uint8_t[]> data;
    uint32_t     cols;
    uint32_t     rows;
    GDALDataType datatype;
    double       time;   // gps seconds
    uint64_t     fileId;

    void clear(void)
    {
        data     = NULL;
        cols     = 0;
        rows     = 0;
        datatype = GDT_Unknown;
        time     = 0;
        fileId   = 0;
   }

   RasterSubset(uint8_t* _data = NULL, double _time = 0, double _fileId = 0):
    data(_data),
    cols(0),
    rows(0),
    datatype(GDT_Unknown),
    time(_time),
    fileId(_fileId) {}

   static bool memreserve(int64_t memsize) { return updateMemPool(memsize, RESERVER); }
   static bool memrelese(int64_t memsize)  { return updateMemPool(memsize, RELEASE); }

private:
    typedef enum
    {
        RESERVER = 1,
        RELEASE = 2
    } memrequest_t;

   static bool updateMemPool(int64_t memsize, memrequest_t requestType)
   {
       static const float oneMB = 1024 * 1024;
       static const int64_t oneGB = 0x40000000;
       static const int64_t maxsize = oneGB*4;
       static int64_t poolsize = maxsize;

       bool status = false;

       if(memsize < 0) return status;   //Caller has a bug in their code

       static Mutex mutex;
       mutex.lock();
       {
           if(requestType == RESERVER)
           {
               int64_t newpoolsize = poolsize - memsize;
               if(newpoolsize >= 0)
               {
                   poolsize = newpoolsize;
                   status   = true;
                   mlog(DEBUG,"***** GET  mempoool available/requested: %.2fMB  %.2fMB ******", poolsize / oneMB, memsize / oneMB);
               }
           }
           else if(requestType == RELEASE)
           {
               poolsize += memsize;
               if(poolsize > maxsize)
               {
                   poolsize = maxsize;
               }
               status = true;
               mlog(DEBUG, "***** FREE mempoool available/requested: %.2fMB  %.2fMB ******", poolsize / oneMB, memsize / oneMB);
           }
       }
       mutex.unlock();

       if(!status && requestType == RESERVER)
       {
            mlog(DEBUG, "mempoool available/requested: %.2fMB < %.2fMB, delta: %.2fMB", poolsize/oneMB, memsize/oneMB, (poolsize-memsize)/oneMB);
       }

       return status;
   }
};

#endif  /* __raster_sample__ */
