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

#include "CcsdsProcessorModule.h"
#include "core.h"
#include "ccsds.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsProcessorModule::TYPE = "CcsdsProcessorModule";

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *
 *   Notes: Permanent object since its purpose is to be attached to a packet processor
 *----------------------------------------------------------------------------*/
CcsdsProcessorModule::CcsdsProcessorModule(CommandProcessor* cmd_proc, const char* obj_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
}

/*----------------------------------------------------------------------------
 * Desctructor  -
 *----------------------------------------------------------------------------*/
CcsdsProcessorModule::~CcsdsProcessorModule()
{
}

/*----------------------------------------------------------------------------
 * parseInt  -
 *----------------------------------------------------------------------------*/
long CcsdsProcessorModule::parseInt(unsigned char* ptr, int size)
{
    long val = 0;

    for(int i = 0; i < size; i++)
    {
        val <<= 8;
        val |= *(ptr + i);
    }

    return val;
}

/*----------------------------------------------------------------------------
 * parseFlt  -
 *----------------------------------------------------------------------------*/
double CcsdsProcessorModule::parseFlt(unsigned char* ptr, int size)
{
    if(size == 4)
    {
        uint32_t ival = parseInt(ptr, size);
        float* fval = (float*)&ival;
        return (double)*fval;
    }
    else if(size == 8)
    {
        uint64_t ival = parseInt(ptr, size);
        double* dval = (double*)&ival;
        return *dval;
    }
    else
    {
        return 0.0;
    }
}

/*----------------------------------------------------------------------------
 * integrateAverage  -
 *----------------------------------------------------------------------------*/
double CcsdsProcessorModule::integrateAverage(uint32_t statcnt, double curr_avg, double new_val)
{
    return ((curr_avg * (double)statcnt) + new_val) / ((double)statcnt + 1.0);
}

/*----------------------------------------------------------------------------
 * integrateWeightedAverage  -
 *----------------------------------------------------------------------------*/
double CcsdsProcessorModule::integrateWeightedAverage(uint32_t curr_cnt, double curr_avg, double new_val, uint32_t new_cnt)
{
    return ((curr_avg * (double)curr_cnt) + (new_val * new_cnt)) / ((double)curr_cnt + (double)new_cnt);
}
