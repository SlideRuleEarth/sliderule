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

#ifndef __h5_io__
#define __h5_io__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <hdf5.h>

/******************************************************************************
 * HDF5 I/O CLASS
 ******************************************************************************/

class H5IO
{
    public:

        static int read (const char* url, const char* datasetname, int col, size_t datatypesize, void* _data);
};

#endif  /* __h5_io__ */
