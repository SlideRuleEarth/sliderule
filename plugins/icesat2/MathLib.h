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

#ifndef __math_lib__
#define __math_lib__

/******************************************************************************
 * MATH LIBRARY CLASS
 ******************************************************************************/

class MathLib
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            double  mean;
            double  slope;
        } lsf_t;

        typedef struct {
            double  x;
            double  y;
        } point_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         initLib         (void);
        static void         deinitLib       (void);
        static double       sum             (double* array, int size);
        static lsf_t        lsf             (point_t* array, int size);
};

#endif  /* __math_lib__ */
