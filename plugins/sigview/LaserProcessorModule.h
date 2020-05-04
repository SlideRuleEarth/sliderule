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

#ifndef __laser_processor_module__
#define __laser_processor_module__

#include "atlasdefines.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

class LaserProcessorModule: public CcsdsProcessorModule
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int NUM_POLY_COEFFS    = 10;
        static const int NUM_LASER_ENERGIES = 10;
        static const int INVALID_APID       = CCSDS_NUM_APIDS;

        static const char* primaryLaserEnergyKey;
        static const char* redundantLaserEnergyKey;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Conversion Types */
        typedef struct {
            double a1;
            double b1;
            double a2;
            double b2;
        } laserConv_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        LaserProcessorModule    (CommandProcessor* cmd_proc, const char* obj_name);
                        ~LaserProcessorModule   (void);

        static  CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        double  primaryLaserEnergy;
        double  redundantLaserEnergy;

        uint16_t  hktTempApid;
        uint16_t  hktLaserApid;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        bool    processSegments (List<CcsdsSpacePacket*>& segments, int numpkts);
        double  laserConv       (const laserConv_t* c, double temp, long raw);
        double  tempConv        (const double c[NUM_POLY_COEFFS], long raw);
        int     attachApidsCmd  (int argc, char argv[][MAX_CMD_SIZE]);
};

#endif  /* __laser_processor_module__ */
