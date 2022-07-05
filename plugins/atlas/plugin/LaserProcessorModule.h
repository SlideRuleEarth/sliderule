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
