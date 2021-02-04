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

#ifndef __h5api__
#define __h5api__

#ifndef H5_API_VERSION
#define H5_API_VERSION H5LITE
#endif

#if H5_API_VERSION == H5LITE
    #include "H5Lite.h"
    #define H5Api H5Lite

#elif H5_API_VERSION == H5LIB
    #include "H5Lib.h"
x    #define H5Api H5Lib

#else
    CompileTimeAssert("Invalid H5 Library Selection");

#endif

#endif  /* __h5api__ */