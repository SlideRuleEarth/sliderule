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

#ifndef __tty_lib__
#define __tty_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"

/******************************************************************************
 * TTY LIBRARY CLASS
 ******************************************************************************/

class TTYLib
{
    public:

        static void         initLib             (void); // initializes library
        static void         deinitLib           (void); // de-initializes library
        static int          ttyopen             (const char* device, int baud, char parity);
        static void         ttyclose            (int fd);
        static int          ttywrite            (int fd, const void* buf, int size, int timeout);
        static int          ttyread             (int fd, void* buf, int size, int timeout);
};

#endif  /* __tty_lib__ */
