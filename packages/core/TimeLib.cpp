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

#include "TimeLib.h"
#include "StringLib.h"
#include "EventLib.h"
#include "List.h"
#include "OsApi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/*
 *  Number of Days to the start of the GPS year, accounts for leap years.
 * 1980 = 0 years, 0 days
 * 1981 = 1 year, 360 days (remember GPS time starts on January 6th)
 * 1982 = 2 years, 725 days
 * etc
 */
const int TimeLib::GpsDaysToStartOfYear[TimeLib::MAX_GPS_YEARS] =
{   0,      360,    725,    1090,   1455,   1821,   2186,   2551,   2916,   3282,
    3647,   4012,   4377,   4743,   5108,   5473,   5838,   6204,   6569,   6934,
    7299,   7665,   8030,   8395,   8760,   9126,   9491,   9856,   10221,  10587,
    10952,  11317,  11682,  12048,  12413,  12778,  13143,  13509,  13874,  14239,
    14604,  14970,  15335,  15700,  16065,  16431,  16796,  17161,  17526,  17892,
    18257,  18622,  18987,  19353,  19718,  20083,  20448,  20814,  21179,  21544,
    21909,  22275,  22640,  23005,  23370,  23736,  24101,  24466,  24831,  25197,
    25562,  25927,  26292,  26658,  27023,  27388,  27753,  28119,  28484,  28849,
    29214,  29580,  29945,  30310,  30675,  31041,  31406,  31771,  32136,  32502,
    32867,  33232,  33597,  33963,  34328,  34693,  35058,  35424,  35789,  36154   };

const int TimeLib::DaysInEachMonth[TimeLib::MONTHS_IN_YEAR] = 
{// J   F   M   A   M   J   J   A   S   O   N   D
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31  };

const char* TimeLib::MonthNames[TimeLib::MONTHS_IN_YEAR] = 
{   "January",      "February",     "March",        "April",
    "May",          "June",         "July",         "August",
    "September",    "October",      "November",     "December"  };

int64_t  TimeLib::baseTimeMs = 0;
int64_t  TimeLib::runningTimeUs = 0;
int64_t  TimeLib::stepTimeUs = 0;
int64_t  TimeLib::currentTimeMs = 0;
int64_t  TimeLib::lastTime = 0;
Timer*   TimeLib::heartBeat = NULL;
int64_t  TimeLib::tickFreq = 1;
int16_t  TimeLib::leapCount = 0;
int64_t* TimeLib::leapSeconds = NULL;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void TimeLib::init(void)
{
    /* Setup Base Time */
    parsenistfile();
    lastTime = LocalLib::time(LocalLib::SYS_CLK);
    baseTimeMs = lastTime / 1000;
    baseTimeMs += getleapms(baseTimeMs);
    baseTimeMs -= LocalGpsEpochMs;
    currentTimeMs = baseTimeMs;
    runningTimeUs = 0;
    stepTimeUs = 1000;

    /* Initialize Tick Frequency */
    tickFreq = LocalLib::timeres(LocalLib::CPU_CLK);

    /* Start Heart Beat */
    try
    {
#ifdef TIME_USE_HEARTBEAT
        heartBeat = new Timer(TimeLib::heartbeat, HEARTBEAT_PERIOD_MS);
#else
        heartBeat = NULL;
#endif
    }
    catch(RunTimeException& e)
    {
        mlog(CRITICAL, "Fatal error: unable to start heart beat timer: %s", e.what());
        assert(false);
    }
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void TimeLib::deinit(void)
{
    if(heartBeat) delete heartBeat;
}

/*----------------------------------------------------------------------------
 * latchtime
 *----------------------------------------------------------------------------*/
double TimeLib::latchtime(void)
{
    return (double)LocalLib::time(LocalLib::CPU_CLK) / (double)tickFreq;
}

/*----------------------------------------------------------------------------
 * gettimems
 * 
 *  grabs the current number of ms that have elapsed since gps time epoch
 *----------------------------------------------------------------------------*/
int64_t TimeLib::gettimems(int64_t now)
{
    if(now == USE_CURRENT_TIME)
    {
        #ifdef TIME_USE_HEARTBEAT
            return currentTimeMs;
        #else
            int64_t sysnow = LocalLib::time(LocalLib::SYS_CLK);
            sysnow /= 1000; // to milliseconds
            sysnow += getleapms(sysnow);
            sysnow -= LocalGpsEpochMs; // to gps epoch
            return sysnow;
        #endif
    }
    else
    {
        return now + getleapms(now) - LocalGpsEpochMs;
    }
}

/*----------------------------------------------------------------------------
 * gettime
 * 
 *  grabs the current GMT system time
 *----------------------------------------------------------------------------*/
TimeLib::gmt_time_t TimeLib::gettime(int64_t now)
{
#ifdef _USE_WINDOWS_TIME_
    SYSTEMTIME systime;
    GetSystemTime(&systime);
    gmt_time_t gmttime;
    gmttime.year = systime.wYear;
    gmttime.day = dayofyear(systime.wYear, systime.wMonth, systime.wDay);
    gmttime.hour = systime.wHour;
    gmttime.minute = systime.wMinute;
    gmttime.second = systime.wSecond;
    gmttime.millisecond = systime.wMilliseconds;
    return gmttime;
#else
    return gps2gmttime(gettimems(now));
#endif
}

/*----------------------------------------------------------------------------
 * gps2gmttime
 * 
 *  converts mechanical milliseconds since GPS epoch to GMT time
 *----------------------------------------------------------------------------*/
TimeLib::gmt_time_t TimeLib::gps2gmttime(int64_t ms)
{
    return cds2gmttime((int)(ms / TIME_MILLISECS_IN_A_DAY), (int)(ms % TIME_MILLISECS_IN_A_DAY));
}

/*----------------------------------------------------------------------------
 * cds2gmttime
 * 
 *  converts from CDS (GPS epoch) format to internal structure representation of GMT
 *----------------------------------------------------------------------------*/
TimeLib::gmt_time_t TimeLib::cds2gmttime(int days, int msecs)
{
    int     gps_days   = days;
    int64_t gps_msecs  = msecs;

    /* Calculate Leap Seconds */
    int64_t unix_msecs = (gps_days * (int64_t)TIME_MILLISECS_IN_A_DAY) + gps_msecs;
    unix_msecs         = TIME_GPS_TO_UNIX(unix_msecs);
    gps_msecs         -= getleapms(unix_msecs);

    if(gps_msecs > 0)
    {
        gps_days += (int)(gps_msecs / TIME_MILLISECS_IN_A_DAY);
        gps_msecs = gps_msecs % TIME_MILLISECS_IN_A_DAY;
    }
    else if(gps_msecs < 0)
    {
        gps_days -= (int)(((-gps_msecs) / TIME_MILLISECS_IN_A_DAY) + 1);
        gps_msecs = TIME_MILLISECS_IN_A_DAY - ((-gps_msecs) % TIME_MILLISECS_IN_A_DAY);
    }

    /* Initialize Data */
    int years        = 0;
    int year_days    = 0;
    int hours        = 0;
    int minutes      = 0;
    int secs         = 0;

    /* Must handle first two years before doing modulo math on year index */
    if( gps_days < GpsDaysToStartOfYear[1] )
    {
        years = 1980;
        year_days = (int)gps_days + 6;
    }
    else if( gps_days < GpsDaysToStartOfYear[2] )
    {
        years  = 1981;
        year_days = (int)gps_days - GpsDaysToStartOfYear[1];
    }
    else
    {
        /* Now modulo math for year index is safe - will be at least 1 */
        unsigned int year_index = (unsigned int)(gps_days/TIME_DAYS_IN_A_YEAR);

        /* Since we use index+1 make sure the index is not too big */
        if( year_index < (MAX_GPS_YEARS - 1) )
        {
            int gps_days_at_start_of_next_year = GpsDaysToStartOfYear[year_index+1];
            int gps_days_at_start_of_this_year = GpsDaysToStartOfYear[year_index];
            int gps_days_at_start_of_prev_year = GpsDaysToStartOfYear[year_index-1];

            if( gps_days >=  gps_days_at_start_of_next_year )
            {
                years = year_index + 1 + 1980;
                year_days = gps_days - gps_days_at_start_of_next_year;
            }
            else if( gps_days >=  gps_days_at_start_of_this_year )
            {
                years = year_index + 1980;
                year_days = gps_days - gps_days_at_start_of_this_year;
            }
            else
            {
                years = year_index - 1 + 1980;
                year_days = gps_days - gps_days_at_start_of_prev_year;
            }
        }
    }

    hours       = (int)(gps_msecs/TIME_MILLISECS_IN_AN_HOUR);
    gps_msecs  %= TIME_MILLISECS_IN_AN_HOUR;

    minutes     = (int)(gps_msecs/TIME_MILLISECS_IN_A_MINUTE);
    gps_msecs  %= TIME_MILLISECS_IN_A_MINUTE;

    secs        = (int)(gps_msecs/TIME_MILLISECS_IN_A_SECOND);
    gps_msecs  %= TIME_MILLISECS_IN_A_SECOND;

    /* Populate GMT Structure */
    gmt_time_t gmt;
    gmt.year        = years;
    gmt.doy         = year_days;
    gmt.hour        = hours;
    gmt.minute      = minutes;
    gmt.second      = secs;
    gmt.millisecond = (int)gps_msecs;

    return gmt;
}

/*----------------------------------------------------------------------------
 * gmt2date
 * 
 *  returns date
 *----------------------------------------------------------------------------*/
TimeLib::date_t TimeLib::gmt2date (gmt_time_t gmt_time)
{
    TimeLib::date_t date;

    /* Determine If Leap Year */
    bool is_leap_year = false;
    if(gmt_time.year % 4 == 0)
    {
        is_leap_year = true;
        if(gmt_time.year % 100 == 0)
        {
            is_leap_year = false;
            if(gmt_time.year % 400 == 0)
            {
                is_leap_year = true;
            }
        }
    }

    /* Determine Month */
    int month = 0, day = 0, preceding_day = 0;
    while(month < MONTHS_IN_YEAR)
    {
        /* Check for Leap Year */
        int leap_day = 0;
        if(is_leap_year && month == 1) leap_day = 1;

        /* Go to Next Month */
        preceding_day = day;
        day += DaysInEachMonth[month] + leap_day;
        month++;

        /* Check Month */
        if(gmt_time.doy <= day)
        {
            break;
        }
    }

    /* Set Date */
    date.year = gmt_time.year;
    date.month = month;
    date.day = gmt_time.doy - preceding_day;

    /* Return Date */
    return date;
}

/*----------------------------------------------------------------------------
 * gmt2gpstime
 * 
 *  returns gps time in milliseconds 
 *----------------------------------------------------------------------------*/
int64_t TimeLib::gmt2gpstime (gmt_time_t gmt_time)
{
    /* Check GMT Time Passed In */
    if( (gmt_time.year < 1980 || gmt_time.year > (1980 + MAX_GPS_YEARS)) ||
        (gmt_time.doy < 0 || gmt_time.doy > 365) ||
        (gmt_time.hour < 0 || gmt_time.hour > 24) ||
        (gmt_time.minute < 0 || gmt_time.minute > 60) ||
        (gmt_time.second < 0 || gmt_time.second > 60) ||
        (gmt_time.millisecond < 0 || gmt_time.millisecond > 1000) ||
        (gmt_time.year == 1980 && gmt_time.doy < 6) )
    {
        mlog(CRITICAL, "Invalid time supplied in GMT structure: %d:%d:%d:%d:%d:%d",
                gmt_time.year, gmt_time.doy, gmt_time.hour, gmt_time.minute, gmt_time.second, gmt_time.millisecond);
        return 0;
    }

    /* Find number of seconds */
    int64_t gps_seconds = 0;
    int years = gmt_time.year - 1980;
    if(years == 0)
    {
        gps_seconds = (gmt_time.doy - 6) * TIME_SECS_IN_A_DAY;
    }
    else if(years > 0 && years < MAX_GPS_YEARS)
    {
        gps_seconds = (GpsDaysToStartOfYear[years] + gmt_time.doy) * TIME_SECS_IN_A_DAY;
    }

    /* Adjust Seconds for Time within Day */
    gps_seconds  += gmt_time.hour * TIME_SECS_IN_AN_HOUR;
    gps_seconds  += gmt_time.minute * TIME_SECS_IN_A_MINUTE;
    gps_seconds  += gmt_time.second;

    /* Calculate Milliseconds Since Epoch */
    int64_t gps_msecs = (gps_seconds * TIME_MILLISECS_IN_A_SECOND) + gmt_time.millisecond;

    /* Calculate and add Leap Seconds */
    int64_t unix_mseconds = TIME_GPS_TO_UNIX(gps_msecs);
    gps_msecs  += getleapms(unix_mseconds);

    return gps_msecs;
}

/*----------------------------------------------------------------------------
 * str2gpstime
 *----------------------------------------------------------------------------*/
int64_t TimeLib::str2gpstime (const char* time_str)
{
    const int max_time_size = 256;
    char time_buf[max_time_size];
    const int max_token_count = 6;
    char* token_ptr[max_token_count];

    /* Determine format of time string and copy into buffer */
    int i;
    int tok = 1;
    int colon_count = 0;
    token_ptr[0] = &time_buf[0];
    for(i = 0; i < (max_time_size - 1) && time_str[i] != '\0'; i++)
    {
        time_buf[i] = time_str[i];
        if(time_str[i] == ':')
        {
            colon_count++;
            if(colon_count < max_token_count)
            {
                time_buf[i] = '\0';
                token_ptr[tok++] = &time_buf[i+1];
            }
        }
    }
    time_buf[i] = '\0';

    /* Process String Input */
    bool status         = false;
    long year           = 0;
    long month          = 0;
    long day            = 0;
    long day_of_year    = 0;
    long hour           = 0;
    long minute         = 0;
    double second       = 0;

    /* <year>:<month>:<day of month>:<hour in day>:<minute in hour>:<second in minute> */
    if(colon_count == 5)
    {
        if(StringLib::str2long(token_ptr[0], &year))
        {
            if(StringLib::str2long(token_ptr[1], &month))
            {
                if(month > 0 && month <= 12)
                {
                    if(StringLib::str2long(token_ptr[2], &day))
                    {
                        if(StringLib::str2long(token_ptr[3], &hour))
                        {
                            if(StringLib::str2long(token_ptr[4], &minute))
                            {
                                if(StringLib::str2double(token_ptr[5], &second))
                                {
                                    status = true;
                                    day_of_year = dayofyear(year, month, day);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    /* <year>:<day of year>:<hour in day>:<minute in hour>:<second in minute> */
    else if(colon_count == 4)
    {
        if(StringLib::str2long(token_ptr[0], &year))
        {
            if(StringLib::str2long(token_ptr[1], &day_of_year))
            {
                if(StringLib::str2long(token_ptr[2], &hour))
                {
                    if(StringLib::str2long(token_ptr[3], &minute))
                    {
                        if(StringLib::str2double(token_ptr[4], &second))
                        {
                            status = true;
                        }
                    }
                }
            }
        }
    }

    /* Check Success and Return GMT Structure */
    if(status == false)
    {
        mlog(CRITICAL, "Unable to parse supplied time string: %s", time_str);
        return 0;
    }
    else
    {
        gmt_time_t gmt_time;
        gmt_time.year = year;
        gmt_time.doy = day_of_year;
        gmt_time.hour = hour;
        gmt_time.minute = minute;
        gmt_time.second = (int)second;
        gmt_time.millisecond = (int)(((long)(second * 1000))%1000);
        return gmt2gpstime(gmt_time);
    }
}

/*----------------------------------------------------------------------------
 * dayofyear
 *----------------------------------------------------------------------------*/
int TimeLib::dayofyear(int year, int month, int day_of_month)
{
    /* Leap Year...       J   F   M   A   M   J   J   A   S   O   N   D */
    int month2days[12] = { 31, 28, 31, 30, 31, 30, 31, 30, 30, 31, 30, 31 };
    if (year % 4 == 0 && year % 100 != 0) month2days[1] = 29;
    else if (year % 4 == 0 && year % 100 == 0 && year % 400 == 0) month2days[1] = 29;
    int day_of_year = 0;
    for (int m = 0; m < (month - 1); m++) day_of_year += month2days[m];
    day_of_year += day_of_month;
    return day_of_year;
}

/*----------------------------------------------------------------------------
 * getleapms
 *----------------------------------------------------------------------------*/
int TimeLib::getleapms(int64_t current_time, int64_t start_time)
{
    int start_index = leapCount;
    int current_index = 0;

    /* Find the index of the last leap second before the current time */
    for (int i = leapCount - 1; i > 0; i--)
    {
        if (current_time > leapSeconds[i])
        {
            current_index = i;
            break;
        }
    }

    /* If not GPS_EPOCH_START, find the index of the supplied epoch*/
    if (start_time == TIME_GPS_EPOCH_START)
    {
       start_index = 10;
    }
    else
    {
       for (int i = 0; i < leapCount; i++)
       {
            if (start_time < leapSeconds[i])
            {
                start_index = i;
                break;
            }
        }
    }

    /* Leap seconds elapsed between start time and current time */
    return ((current_index - start_index) + 1) * TIME_MILLISECS_IN_A_SECOND;
}

/*----------------------------------------------------------------------------
 * getleapms
 *----------------------------------------------------------------------------*/
const char* TimeLib::getMonthName (int month)
{
    int month_index = month - 1;
    if(month_index >= 0 && month_index < MONTHS_IN_YEAR)
    {
        return MonthNames[month_index];
    }
    else
    {
        return NULL;
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * handler - 1KHz Signal Handler for LocalLib Time Keeping
 *----------------------------------------------------------------------------*/
void TimeLib::heartbeat(void)
{
    static int counter = 0;
    runningTimeUs += stepTimeUs;
    if(counter++ == HEARTBEATS_PER_SECOND)
    {
        counter = 0;

        /* Calculate Microseconds per Second */
        int64_t now = LocalLib::time(LocalLib::SYS_CLK);
        int64_t usec_per_sec = now - lastTime;
        if(usec_per_sec > 500000 && usec_per_sec < 1500000)
        {
            /* Calculate New Step Time */
            stepTimeUs = usec_per_sec / HEARTBEATS_PER_SECOND;
        }
        else
        {
            /* Detect Gross Adjustment */
            mlog(CRITICAL, "Gross adjustment detected in step time: %lld", (long long)usec_per_sec);
            baseTimeMs = now / 1000;
            baseTimeMs += getleapms(baseTimeMs);
            baseTimeMs -= LocalGpsEpochMs; // moves time up to GPS epoch
            runningTimeUs = 0;
            stepTimeUs = 1000;
        }

        /* Save Off Last Time Spec */
        lastTime = now;
    }

    /* Set Current Time */
    currentTimeMs = baseTimeMs + (runningTimeUs / 1000);
}

/*----------------------------------------------------------------------------
 * parsenistfile - parses leap-seconds.list file from NIST
 *----------------------------------------------------------------------------*/
void TimeLib::parsenistfile(void)
{
    List<int64_t> ntp_leap_seconds;

    /* Read contents of file and store in temporary array */
    SafeString leap_second_file_name;
    leap_second_file_name += CONFDIR;
    leap_second_file_name.appendChar(PATH_DELIMETER);
    leap_second_file_name += TIME_NIST_LIST_FILENAME;
    FILE* fd = fopen( leap_second_file_name.getString(), "r" );
    if( fd != NULL )
    {
        char line[MAX_STR_SIZE];
        while (fgets(line, MAX_STR_SIZE, fd) != NULL)
        {
            /* If the line starts with a '#', then it is a comment */
            if (line[0] != '#')
            {
                /* Trim everything after the first space */
                char leap_second_str[1][MAX_STR_SIZE];
                int num_vals = StringLib::tokenizeLine(line, MAX_STR_SIZE, ' ', 1, leap_second_str);
                if(num_vals > 0)
                {
                    /* Convert to long and assign to array */
                    long long leap_second;
                    if(StringLib::str2llong(leap_second_str[0], &leap_second, 10))
                    {
                        int64_t ls = (int64_t)leap_second;
                        ntp_leap_seconds.add(ls);
                    }
                    else
                    {
                        mlog(CRITICAL, "Failed to parse leap second: %s", leap_second_str[0]);
                    }
                }
            }
        }

        fclose( fd );
    }
    else
    {
        mlog(CRITICAL, "Fatal error: Unable to open leap-seconds.list");
    }

    /* Allocate Leap Second Array */
    leapCount = ntp_leap_seconds.length();
    leapSeconds = new int64_t[leapCount];
    assert(leapCount); // fail if no leap seconds read

    /* Populate Leap Second Array - Convert from NTP to UNIX epoch */
    for (int i = 0; i < leapCount; i++)
    {
        leapSeconds[i]  = (int64_t)TIME_NTP_TO_UNIX(ntp_leap_seconds[i]);
        leapSeconds[i] *= TIME_MILLISECS_IN_A_SECOND;
    }
}
