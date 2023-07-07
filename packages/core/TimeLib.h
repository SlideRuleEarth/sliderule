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

/*
 * There are five types of time available in SlideRule
 *
 *  (1) CPU
 *      monotonically incrementing clock with unspecified starting point
 *      microsecond precision
 *      OsApi::time(OsApi::CPU_CLK)
 *
 *  (2) SYS
 *      unix sytem time, no leap seconds, epoch of 1 Jan 1970 00:00:00
 *      microsecond precision
 *      OsApi::time(OsApi::SYS_CLK)
 *
 *  (3) GPS
 *      gps time, leap seconds, epoch of 6 Jan 1980 00:00:00
 *      millisecond precision
 *      TimeLib::gpstime()
 *
 *  (4) GMT
 *      utc time, no leap seconds
 *      millisecond precision
 *      TimeLib::gmttime()
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
#define TIME_DAYS_IN_A_YEAR         365                                 // not a leap year days
#define TIME_SECS_IN_A_DAY          (60*60*24)
#define TIME_SECS_IN_AN_HOUR        (60*60)
#define TIME_SECS_IN_A_MINUTE       60
#define TIME_MILLISECS_IN_A_MINUTE  (60*1000)
#define TIME_MILLISECS_IN_AN_HOUR   (60*TIME_MILLISECS_IN_A_MINUTE)
#define TIME_MILLISECS_IN_A_DAY     (24*TIME_MILLISECS_IN_AN_HOUR)

/******************************************************************************
 * TIME LIBRARY CLASS
 ******************************************************************************/

class TimeLib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int64_t USE_CURRENT_TIME = -1;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            int year;
            int doy; // day of year
            int hour;
            int minute;
            int second;
            int millisecond;
        } gmt_time_t;

        typedef struct {
            int year;
            int month;
            int day;
        } date_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         init            (void);
        static void         deinit          (void);
        static double       latchtime       (void); // system call, returns seconds (microsecond precision)
        static int64_t      gpstime         (void); // optimized, returns milliseconds since gps epoch
        static gmt_time_t   gmttime         (void); // returns GMT time (includes leap seconds)
        static int64_t      sys2gpstime     (int64_t sysnow); // takes system time (microseconds), returns milliseconds from gps epoch
        static int64_t      gps2systime     (int64_t gpsnow); // takes gps time (milliseconds), return system time (microseconds)
        static int64_t      gps2systimeex   (double gps_secs); // takes gps time (seconds), return extended precision system time (nanoseconds)
        static int64_t      sysex2gpstime   (int64_t sysex); // takes extended precision system time (nanoseconds), return gps time (milliseconds)
        static gmt_time_t   sys2gmttime     (int64_t sysnow); // takes system time (microseconds), returns GMT time (includes leap seconds)
        static gmt_time_t   gps2gmttime     (int64_t ms); // returns GMT time (includes leap seconds), takes gps time as milliseconds since gps epoch
        static gmt_time_t   cds2gmttime     (int days, int msecs); // returns GMT time (includes leap seconds)
        static date_t       gmt2date        (const gmt_time_t& gmt_time); // returns date (taking into account leap years)
        static int64_t      gmt2gpstime     (const gmt_time_t& gmt_time); // returns milliseconds from gps epoch to time specified in gmt_time
        static int64_t      str2gpstime     (const char* time_str); // returns milliseconds from gps epoch to time specified in time_str
        static int64_t      datetime2gps    (int year, int month=0, int day=0, int hour=0, int minute=0, int second=0, int millisecond=0);
        static int          dayofyear       (int year, int month, int day_of_month);
        static int          daysinmonth     (int year, int month);
        static const char*  getmonthname    (int month); // [1..12] --> ["January".."December"]
        static bool         gmtinrange      (const gmt_time_t& gmt_time, const gmt_time_t& gmt_start, const gmt_time_t& gmt_end); // returns true if in range

        private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int LEAP_SECS_AT_GPS_EPOCH  = 10; // seconds
        static const int GPS_EPOCH_START = 315964800; // seconds
        static const char* NIST_LIST_FILENAME;

        static const int HEARTBEAT_PERIOD_MS = 1;
        static const int HEARTBEATS_PER_SECOND = 1000; // must be consistent with HEARTBEAT_PERIOD
        static const int MAX_GPS_YEARS = 100;
        static const int MONTHS_IN_YEAR = 12;

        static const int GpsDaysToStartOfYear[MAX_GPS_YEARS];
        static const int DaysInEachMonth[MONTHS_IN_YEAR];
        static const char* MonthNames[MONTHS_IN_YEAR];

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

        static int getleapsecs (int64_t sysnow, int64_t start_secs);
        static void heartbeat (void);
        static void parsenistfile (void);

        static inline int64_t GPS_TO_SYS (int64_t gpsnow) { return (((gpsnow) + 315964800000LL) * 1000); }   // IN: milliseconds, OUT: microseconds
        static inline int64_t SYS_TO_GPS (int64_t sysnow) { return (((sysnow) - 315964800000000LL) / 1000); } // IN: microseconds, OUT: milliseconds
        static inline int64_t NTP_TO_SYS (int64_t ntpnow) { return (((ntpnow) - 2208988800LL)); } // IN: seconds, OUT: seconds
        static inline double GPS_TO_SYS_EX (double gps_secs) { return (((gps_secs) + 315964800.0)); }   // IN: seconds, OUT: seconds
};

#endif  /* __time_lib__ */
