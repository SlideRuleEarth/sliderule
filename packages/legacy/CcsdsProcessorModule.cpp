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
