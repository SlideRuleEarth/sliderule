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
