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

#include "H5Future.h"
#include "RecordObject.h"
#include "OsApi.h"

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5Future::H5Future (void)
{
    info.elements   = 0;
    info.typesize   = 0;
    info.datasize   = 0;
    info.data       = NULL;
    info.datatype   = RecordObject::INVALID_FIELD;
    info.numcols    = 0;
    info.numrows    = 0;

    complete        = false;
    valid           = false;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5Future::~H5Future (void)
{
    wait(IO_PEND);
    delete [] info.data;
}

/*----------------------------------------------------------------------------
 * wait
 *----------------------------------------------------------------------------*/
H5Future::rc_t H5Future::wait (int timeout)
{
    rc_t rc;

    sync.lock();
    {
        if(!complete)
        {
            sync.wait(0, timeout);
        }

        if      (!valid)    rc = INVALID;
        else if (!complete) rc = TIMEOUT;
        else                rc = COMPLETE;
    }
    sync.unlock();

    return rc;
}

/*----------------------------------------------------------------------------
 * finish
 *----------------------------------------------------------------------------*/
 void H5Future::finish (bool _valid)
 {
    sync.lock();
    {
        valid = _valid;
        complete = true;
        sync.signal();
    }
    sync.unlock();
 }
