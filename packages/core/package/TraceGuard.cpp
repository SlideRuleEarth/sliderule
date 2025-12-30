/*
 * Copyright (c) 2025, University of Washington
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

#include "TraceGuard.h"

#include <cstdarg>
#include <cstdio>

/******************************************************************************
 * TraceGuard METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
TraceGuard::TraceGuard (event_level_t lvl, uint32_t parent_trace_id, const char* name, const char* attr_fmt, ...):
    level       (lvl),
    trace_id    (0)
{
    char attr[EventLib::MAX_ATTR_STR] = {0};

    if(attr_fmt)
    {
        va_list args;
        va_start(args, attr_fmt);
        vsnprintf(attr, sizeof(attr), attr_fmt, args);
        va_end(args);
    }

    trace_id = EventLib::startTrace(parent_trace_id, name, level, "%s", attr);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
TraceGuard::~TraceGuard (void)
{
    EventLib::stopTrace(trace_id, level);
}

/*----------------------------------------------------------------------------
 * id
 *----------------------------------------------------------------------------*/
uint32_t TraceGuard::id (void) const
{
    return trace_id;
}

/*----------------------------------------------------------------------------
 * stash
 *----------------------------------------------------------------------------*/
void TraceGuard::stash (void) const
{
    EventLib::stashId(trace_id);
}
