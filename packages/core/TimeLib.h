/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Notes:
 *  1. All GPS times are represented as mechanical milliseconds since GPS epoch of 6 Jan 1980 00:00:00
 *  2. All GMT times are represented as UTC and are expressed in years, days, etc.  These times include leap seconds.
 */

#ifndef __time_lib__
#define __time_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"

#include <time.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define TIME_32BIT_FLOAT_MAX_VALUE  4294967296.0
#define TIME_DAYS_IN_A_YEAR         365 /* Not a leap year days */
#define TIME_SECS_IN_A_DAY          (60*60*24)
#define TIME_SECS_IN_AN_HOUR        (60*60)
#define TIME_SECS_IN_A_MINUTE       60
#define TIME_MILLISECS_IN_A_SECOND  1000
#define TIME_MILLISECS_IN_A_MINUTE  (60*TIME_MILLISECS_IN_A_SECOND)
#define TIME_MILLISECS_IN_AN_HOUR   (60*TIME_MILLISECS_IN_A_MINUTE)
#define TIME_MILLISECS_IN_A_DAY     (24*TIME_MILLISECS_IN_AN_HOUR)
#define TIME_GPS_EPOCH_START        315964800
#define TIME_GPS_TO_UNIX(x)         (x + 315964800000LL)
#define TIME_UNIX_TO_GPS(x)         (x - 315964800000LL)
#define TIME_NTP_TO_UNIX(x)         (x - 2208988800LL)
#define TIME_NIST_LIST_FILENAME     "leap-seconds.list"

/******************************************************************************
 * TIME LIBRARY CLASS
 ******************************************************************************/

class TimeLib
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            int year;
            int day;
            int hour;
            int minute;
            int second;
            int millisecond;
        } gmt_time_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         init            (void);
        static void         deinit          (void);
        static double       latchtime       (void); // system call, returns seconds
        static int64_t      gettimems       (void); // optimized, returns milliseconds since gps epoch
        static gmt_time_t   gettime         (void); // returns GMT time (includes leap seconds)
        static gmt_time_t   gps2gmttime     (int64_t ms); // returns GMT time (includes leap seconds), takes gps time as milliseconds since gps epoch
        static gmt_time_t   cds2gmttime     (int days, int msecs); // returns GMT time (includes leap seconds)
        static int64_t      gmt2gpstime     (gmt_time_t gmt_time); // returns milliseconds from gps epoch to time specified in gmt_time
        static int64_t      str2gpstime     (const char* time_str); // returns milliseconds from gps epoch to time specified in time_str
        static int          dayofyear       (int year, int month, int day_of_month);
        static int          getleapmsec     (int64_t current_time, int64_t start_time = TIME_GPS_EPOCH_START);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int64_t LocalGpsEpochMs = 315964800000;
        static const int HEARTBEAT_PERIOD_MS = 1;
        static const int HEARTBEATS_PER_SECOND = 1000; // must be consistent with HEARTBEAT_PERIOD
        static const int MAX_GPS_YEARS = 100;

        static const int GpsDaysToStartOfYear[MAX_GPS_YEARS];

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static int64_t  baseTimeMs;
        static int64_t  runningTimeUs;
        static int64_t  stepTimeUs;
        static int64_t  currentTimeMs;
        static int64_t  lastTime;
        static Timer*   heartBeat;
        static int64_t  tickFreq;
        static int16_t  leapCount;
        static int64_t* leapSeconds;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void heartbeat(void);
        static void parsenistfile(void);
};

#endif  /* __time_lib__ */
