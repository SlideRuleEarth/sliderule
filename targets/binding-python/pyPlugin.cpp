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

#include <pybind11/pybind11.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include "StringLib.h"
#include "pyPlugin.h"

/******************************************************************************
 * NAMESPACES
 ******************************************************************************/

namespace py = pybind11;

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

typedef void (*init_f) (void);

/******************************************************************************
 * pyPlugin Class
 ******************************************************************************/

/*--------------------------------------------------------------------
 * Constructor
 *--------------------------------------------------------------------*/
pyPlugin::pyPlugin (const std::string &_plugin)
{
    /* Get Plugin Name */
    char plugin_buf[MAX_STR_SIZE];
    StringLib::copy(plugin_buf, _plugin.c_str(), _plugin.length());
    char* plugin_name = StringLib::find(plugin_buf, '/', false) + 1;
    char* plugin_ext = StringLib::find(plugin_buf, '.', true);
    *plugin_ext = '\0';

    /* Load Plugin */
    void* plugin = dlopen(_plugin.c_str(), RTLD_NOW);
    if(plugin)
    {
        /* Call plugin initialization function */
        char init_func[MAX_STR_SIZE];
        StringLib::format(init_func, MAX_STR_SIZE, "init%s", plugin_name);
        init_f init = (init_f)dlsym(plugin, init_func);
        if(init)
        {
            init();
        }
        else
        {
            throw RunTimeException(CRITICAL, "cannot find initialization function %s: %s\n", init_func, dlerror());
        }
    }
    else
    {
        throw RunTimeException(CRITICAL, "cannot load %s: %s\n", plugin_name, dlerror());
    }
}

/*--------------------------------------------------------------------
 * Destructor
 *--------------------------------------------------------------------*/
pyPlugin::~pyPlugin (void)
{
}
