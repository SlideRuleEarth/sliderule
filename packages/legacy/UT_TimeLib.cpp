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

#include "UT_TimeLib.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char*   UT_TimeLib::TYPE               = "UT_TimeLib";
const int     UT_TimeLib::UNIX_Epoch_Start   = 1970;
const int64_t UT_TimeLib::Truth_Times[39][2] = {
  //All in milliseconds
  //UNIX          //GPS
  {315964800000,  0},              // January 06, 1980
  {347587200000,  31622400000},    // January 06, 1981
  {379123200000,  63158401000},    // January 06, 1982
  {410659200000,  94694402000},    // January 06, 1983
  {442195200000,  126230403000},   // January 06, 1984
  {473817600000,  157852803000},   // January 06, 1985
  {505353600000,  189388804000},   // January 06, 1986
  {536889600000,  220924804000},   // January 06, 1987
  {568425600000,  252460805000},   // January 06, 1988
  {600048000000,  284083205000},   // January 06, 1989
  {631584000000,  315619206000},   // January 06, 1990
  {663120000000,  347155207000},   // January 06, 1991
  {694656000000,  378691207000},   // January 06, 1992
  {726278400000,  410313608000},   // January 06, 1993
  {757814400000,  441849609000},   // January 06, 1994
  {789350400000,  473385610000},   // January 06, 1995
  {820886400000,  504921611000},   // January 06, 1996
  {852508800000,  536544011000},   // January 06, 1997
  {884044800000,  568080012000},   // January 06, 1998
  {915580800000,  599616013000},   // January 06, 1999
  {947116800000,  631152013000},   // January 06, 2000
  {978739200000,  662774413000},   // January 06, 2001
  {1010275200000, 694310413000},   // January 06, 2002
  {1041811200000, 725846413000},   // January 06, 2003
  {1073347200000, 757382413000},   // January 06, 2004
  {1104969600000, 789004813000},   // January 06, 2005
  {1136505600000, 820540814000},   // January 06, 2006
  {1168041600000, 852076814000},   // January 06, 2007
  {1199577600000, 883612814000},   // January 06, 2008
  {1231200000000, 915235215000},   // January 06, 2009
  {1262736000000, 946771215000},   // January 06, 2010
  {1294272000000, 978307215000},   // January 06, 2011
  {1325808000000, 1009843215000},  // January 06, 2012
  {1357430400000, 1041465616000},  // January 06, 2013
  {1388966400000, 1073001616000},  // January 06, 2014
  {1420502400000, 1104537616000},  // January 06, 2015
  {1452038400000, 1136073617000},  // January 06, 2016
  {1483660800000, 1167696018000},  // January 06, 2017
  {1515196800000, 1199232018000}   // January 06, 2018
};


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* UT_TimeLib::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    /* Create Message Queue Unit Test */
    return new UT_TimeLib(cmd_proc, name);
}

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
UT_TimeLib::UT_TimeLib(CommandProcessor* cmd_proc, const char* obj_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    initTruthGMT();

    /* Register Commands */
    registerCommand("CHECK_GMT_2_GPS", (cmdFunc_t)&UT_TimeLib::CheckGmt2GpsCmd,  0, "");
    registerCommand("CHECK_GPS_2_GMT", (cmdFunc_t)&UT_TimeLib::CheckGps2GmtCmd,  0, "");
    registerCommand("CHECK_GET_COUNT", (cmdFunc_t)&UT_TimeLib::CheckGetCountCmd, 0, "");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_TimeLib::~UT_TimeLib(void)
{
}

/*----------------------------------------------------------------------------
 * CheckGetCountCmd
 *  
 *  Checks that a GPS time is converted to the correct GMT time
 *----------------------------------------------------------------------------*/
int UT_TimeLib::CheckGetCountCmd(int argc, char argv[][MAX_CMD_SIZE])
{
  (void)argc;
  (void)argv;
  for (int i = 0; i < 39; i++)
  {
    int64_t leap_mseconds = Truth_Times[i][0] + TimeLib::getleapms(Truth_Times[i][0]);
    leap_mseconds = TIME_UNIX_TO_GPS(leap_mseconds);
    if (leap_mseconds != Truth_Times[i][1])
    {
      mlog(RAW, "Calculated: %lld\n", (long long)leap_mseconds);
      mlog(RAW, "Truth Time: %lld\n", (long long)Truth_Times[i][1]);
      return -1;
    }
  }
  return 0;
}

/*----------------------------------------------------------------------------
 * CheckGps2GmtCmd
 * 
 *  Checks that a GPS time is converted to the correct GMT time
 *----------------------------------------------------------------------------*/
int UT_TimeLib::CheckGps2GmtCmd(int argc, char argv[][MAX_CMD_SIZE])
{
  (void)argc;
  (void)argv;
  for (int i = 0; i < 39; ++i)
  {
    TimeLib::gmt_time_t gmt_time = TimeLib::gps2gmttime(Truth_Times[i][1]);
    bool success = true;
    if (gmt_time.millisecond != Truth_GMT[i].millisecond) success = false;
    if (gmt_time.second      != Truth_GMT[i].second)      success = false;
    if (gmt_time.minute      != Truth_GMT[i].minute)      success = false;
    if (gmt_time.hour        != Truth_GMT[i].hour)        success = false;
    if (gmt_time.day         != Truth_GMT[i].day)         success = false;
    if (gmt_time.year        != Truth_GMT[i].year)        success = false;

    if (!success)
    {
      mlog(RAW, "Calculated - Year: %d, Day: %d, Hour: %d, Minute: %d, second: %d, millisecond: %d\n", gmt_time.year, gmt_time.day, gmt_time.hour, gmt_time.minute, gmt_time.second, gmt_time.millisecond);
      mlog(RAW, "Truth      - Year: %d, Day: %d, Hour: %d, Minute: %d, second: %d, millisecond: %d\n", Truth_GMT[i].year, Truth_GMT[i].day, Truth_GMT[i].hour, Truth_GMT[i].minute, Truth_GMT[i].second, Truth_GMT[i].millisecond);
      return -1;
    }
  }
  return 0;
}

/*----------------------------------------------------------------------------
 * CheckGmt2GpsCmd
 * 
 *  Checks that a GMT time is converted to the correct GPS time
 *----------------------------------------------------------------------------*/
int UT_TimeLib::CheckGmt2GpsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
  (void)argc;
  (void)argv;
  for (int i = 0; i < 39; ++i)
  {
    int64_t gps_time = TimeLib::gmt2gpstime(Truth_GMT[i]);

    if (gps_time != Truth_Times[i][1])
    {
      mlog(RAW, "Calculated: %lld\n", (long long)gps_time);
      mlog(RAW, "Truth:      %lld\n", (long long)Truth_Times[i][1]);
      return -1;
    }
  }
  return 0;
}

/*----------------------------------------------------------------------------
 * initTruthGMT
 *----------------------------------------------------------------------------*/
void UT_TimeLib::initTruthGMT(void)
{
  for (int i = 0; i < 39; i++)
  {
    Truth_GMT[i].millisecond = 0;
    Truth_GMT[i].second      = 0;
    Truth_GMT[i].minute      = 0;
    Truth_GMT[i].hour        = 0;
    Truth_GMT[i].day         = 6;
    Truth_GMT[i].year        = 1980 + i;
  }
}
