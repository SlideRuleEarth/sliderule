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

#include "LaserProcessorModule.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LaserProcessorModule::primaryLaserEnergyKey = "primaryLaserEnergy";
const char* LaserProcessorModule::redundantLaserEnergyKey = "redundantLaserEnergy";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
LaserProcessorModule::LaserProcessorModule(CommandProcessor* cmd_proc, const char* obj_name):
    CcsdsProcessorModule(cmd_proc, obj_name)
{
    /* Initialize Parameters */
    hktTempApid = 0x425;
    hktLaserApid = 0x427;

    /* Post Initial Values to Current Value Table */
    cmdProc->setCurrentValue(getName(), primaryLaserEnergyKey,      (void*)&primaryLaserEnergy,     sizeof(primaryLaserEnergy));
    cmdProc->setCurrentValue(getName(), redundantLaserEnergyKey,    (void*)&redundantLaserEnergy,   sizeof(redundantLaserEnergy));

    /* Register Commands */
    registerCommand("ATTACH_HKT_APIDS", (cmdFunc_t)&LaserProcessorModule::attachApidsCmd, 2, "<hkt temp apid> <hkt laser apid>");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
LaserProcessorModule::~LaserProcessorModule(void)
{
}

/******************************************************************************
 * PUBLIC STATIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject
 *----------------------------------------------------------------------------*/
CommandableObject* LaserProcessorModule::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    return new LaserProcessorModule(cmd_proc, name);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processSegments  - Parse HKT Telemetry Housekeeping Packet
 *
 *   Notes: PRI and RED laser energies
 *----------------------------------------------------------------------------*/
bool LaserProcessorModule::processSegments(List<CcsdsSpacePacket*>& segments, int numpkts)
{
    (void)numpkts;

    /* Static Temperatures */
    static double spd_pri_temp = 0.0;
    static double spd_red_temp = 0.0;

    /* Conversion Constants */
    const laserConv_t   pri                     = {3.691932e-7, -4.932088e-4, 3.259594e-4, -3.884523e-1};
    const laserConv_t   red                     = {3.1827e-8, -4.2518e-5, 3.734135e-4, -5.5884669e-1};
    const double        spd[NUM_POLY_COEFFS]    = {154.3, -3.321869988e-2, 4.492843546e-6, -3.860600862e-10, 2.102589028e-14, -7.365820341e-19, 1.650616335e-23, -2.282373341e-28, 1.771375605e-33, -5.898713513e-39};

    /* Process Segments */
    int numsegs = segments.length();
    for(int p = 0; p < numsegs; p++)
    {
        unsigned char*  pktbuf  = segments[p]->getBuffer();
        uint16_t          apid    = segments[p]->getAPID();

        if(apid == hktTempApid) // Temperature Housekeeping Packet
        {
            spd_pri_temp = tempConv(spd, parseInt(pktbuf + 12 + (74 * 2), 2));
            spd_red_temp = tempConv(spd, parseInt(pktbuf + 12 + (81 * 2), 2));
        }
        if(apid == hktLaserApid) // Laser Energy Housekeeping Packet
        {
            double primary_energy   = 0.0;
            double redundant_energy = 0.0;
            for(int i = 0; i < NUM_LASER_ENERGIES; i++)
            {
                primary_energy      += laserConv(&pri, spd_pri_temp, parseInt(pktbuf + 12 + (i*4), 2));
                redundant_energy    += laserConv(&red, spd_red_temp, parseInt(pktbuf + 14 + (i*4), 2));
            }
            primaryLaserEnergy = primary_energy / NUM_LASER_ENERGIES;
            redundantLaserEnergy = redundant_energy / NUM_LASER_ENERGIES;

            /* Update Current Values */
            cmdProc->setCurrentValue(getName(), primaryLaserEnergyKey,      (void*)&primaryLaserEnergy,   sizeof(primaryLaserEnergy));
            cmdProc->setCurrentValue(getName(), redundantLaserEnergyKey,    (void*)&redundantLaserEnergy, sizeof(redundantLaserEnergy));
        }
    }

    return true;
}

/*----------------------------------------------------------------------------
 * laserConv  -
 *----------------------------------------------------------------------------*/
double LaserProcessorModule::laserConv(const laserConv_t* c, double temp, long raw)
{
    double iraw = (double)((int16_t)raw + 32768);
    return (((c->a1 * (double)iraw) + c->b1) * temp) + ((c->a2 * iraw) + c->b2);
}

/*----------------------------------------------------------------------------
 * tempConv  -
 *----------------------------------------------------------------------------*/
double LaserProcessorModule::tempConv(const double c[NUM_POLY_COEFFS], long raw)
{
    double iraw = (double)((int16_t)raw + 32768);
    double x = iraw;
    double t = c[0];
    for(int n = 1; n < NUM_POLY_COEFFS; n++)
    {
        t += c[n] * x;
        x *= iraw;
    }
    return t;
}

/*----------------------------------------------------------------------------
 * attachApidsCmd
 *----------------------------------------------------------------------------*/
int LaserProcessorModule::attachApidsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    hktTempApid = (uint16_t)strtol(argv[0], NULL, 0);
    hktLaserApid = (uint16_t)strtol(argv[1], NULL, 0);

    return 0;
}
