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
 INCLUDES
 ******************************************************************************/

#include "core.h"

#ifdef __arrow__
#include "arrow.h"
#endif

#ifdef __aws__
#include "aws.h"
#endif

#ifdef __ccsds__
#include "ccsds.h"
#endif

#ifdef __geo__
#include "geo.h"
#endif

#ifdef __h5__
#include "h5.h"
#endif

#ifdef __legacy__
#include "legacy.h"
#endif

#ifdef __netsvc__
#include "netsvc.h"
#endif

#ifdef __pistache__
#include "pistache.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include <signal.h>
#include <string.h>
#include <atomic>

/******************************************************************************
 TYPEDEFS
 ******************************************************************************/

typedef void (*plugin_f) (void);

typedef struct plugin_node {
    char init_func_name[MAX_STR_SIZE];
    char deinit_func_name[MAX_STR_SIZE];
    void* plugin;
    struct plugin_node* next;
} plugin_list_t;

/******************************************************************************
 FILE DATA
 ******************************************************************************/

static bool app_immediate_abort = false;
static bool app_signal_abort = false;
static plugin_list_t loaded_plugins;
#ifdef CUSTOM_ALLOCATOR
static std::atomic<uint64_t> allocCount = {0};
#endif

/******************************************************************************
 EXPORTED FUNCTIONS
 ******************************************************************************/

/*
 * __stack_chk_fail - called with -fstack-protector-all
 */
void __stack_chk_fail(void)
{
    assert(false);
}

/*
 * Override of new and delete for debugging memory allocations
 */
#ifdef CUSTOM_ALLOCATOR
void* operator new(size_t size)
{
    allocCount++;
    return malloc(size);
}

void operator delete(void* ptr)
{
    allocCount--;
    free(ptr);
}

void operator delete(void* ptr, size_t size)
{
    (void)size;
    allocCount--;
    free(ptr);
}

void displayCount(void)
{
    print2term("ALLOCATED: %ld\n", (long)allocCount);
}
#endif

/******************************************************************************
 LOCAL FUNCTIONS
 ******************************************************************************/

/*
 * console_quick_exit - Signal handler for Control-C
 */
static void console_quick_exit(int parm)
{
    (void)parm;
    if(app_immediate_abort) quick_exit(0);
    print2term("\n...Shutting down command line interface!\n");
    setinactive(); // tells core package that application is no longer active
    app_immediate_abort = true; // multiple control-c will exit immediately
}

/*
 * signal_thread - Manages all the SIGINT and SIGTERM signals
 */
static void* signal_thread (void* parm)
{
    sigset_t* signal_set = (sigset_t*)parm;

    while(true)
    {
        int sig = 0;
        int status = sigwait(signal_set, &sig);
        if (status != 0)
        {
            print2term("Fatal error (%d) ...failed to wait for signal: %s\n", status, strerror(errno));
            signal(SIGINT, console_quick_exit);
            break;
        }
        else if(app_signal_abort)
        {
            break; // exit thread for clean up
        }
        else
        {
            console_quick_exit(0);
        }
    }

    return NULL;
}

/*
 * ldplugins - Load plugins from application configuration directory
 */
static void ldplugins(void)
{
    /* Initialize Plugin List */
    loaded_plugins.next = NULL;
    plugin_list_t* curr_plugin = &loaded_plugins;

    /* Load All Plugins from Configuration Directory */
    DIR *dir;
    if((dir = opendir(CONFDIR)) != NULL)
    {
        struct dirent *ent;
        while((ent = readdir(dir)) != NULL)
        {
            /* Build Plugin Name */
            char plugin_name[MAX_STR_SIZE];
            StringLib::copy(plugin_name, ent->d_name, MAX_STR_SIZE);
            char* strterm = StringLib::find(plugin_name, ".so");
            if(strterm) *strterm = '\0';
            else continue;

            /* Load Plugin */
            print2term("Loading plug-in %s ... ", plugin_name);
            char plugin_path[MAX_STR_SIZE];
            StringLib::format(plugin_path, MAX_STR_SIZE, "%s%c%s.so", CONFDIR, PATH_DELIMETER, plugin_name);
            curr_plugin->plugin = dlopen(plugin_path, RTLD_NOW);
            if(!curr_plugin->plugin)
            {
                print2term("cannot load %s: %s\n", plugin_name, dlerror());
                continue;
            }

            /* Initialize Plugin */
            StringLib::format(curr_plugin->init_func_name, MAX_STR_SIZE, "init%s", plugin_name);
            plugin_f init = (plugin_f)dlsym(curr_plugin->plugin, curr_plugin->init_func_name);
            if(!init) print2term("cannot find initialization function %s: %s\n", curr_plugin->init_func_name, dlerror());
            else init();

            /* Save Off Deinit Function Name */
            StringLib::format(curr_plugin->deinit_func_name, MAX_STR_SIZE, "deinit%s", plugin_name);

            /* Add to Plugin List */
            curr_plugin->next = new plugin_list_t;
            curr_plugin = curr_plugin->next;
            curr_plugin->next = NULL;
        }
        closedir(dir);
    }
}

/*
 * ulplugins - Unload plugins
 */
static void ulplugins(void)
{
    plugin_list_t* curr_plugin = &loaded_plugins;
    while(curr_plugin->next != NULL)
    {
        /* Deinitialize */
        plugin_f deinit = (plugin_f)dlsym(curr_plugin->plugin, curr_plugin->deinit_func_name);
        if(!deinit) print2term("cannot find deinitialization function %s: %s\n", curr_plugin->deinit_func_name, dlerror());
        else deinit();

        /* Close */
        dlclose(curr_plugin->plugin);

        /* Goto Next */
        curr_plugin = curr_plugin->next;
    }
}

/*
 * lua_abort_hook - Call back for lua engine to check for termination
 */
static void lua_abort_hook (lua_State *L, lua_Debug *ar)
{
    (void)ar;
    if(!checkactive()) luaL_error(L, "Interpreter no longer active - aborting!\n");
}

/******************************************************************************
 MAIN
 ******************************************************************************/

int main (int argc, char* argv[])
{
    /* Block SIGINT and SIGTERM for all future threads created */
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    sigaddset(&signal_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signal_set, NULL);

    /* Create dedicated signal thread to handle SIGINT and SIGTERM */
    pthread_t signal_pid;
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&signal_pid, &pthread_attr, &signal_thread, (void *) &signal_set);

    /* Initialize Built-In Packages */
    initcore();

    #ifdef __arrow__
        initarrow();
    #endif

    #ifdef __aws__
        initaws();
    #endif

    #ifdef __ccsds__
        initccsds();
    #endif

    #ifdef __geo__
        initgeo();
    #endif

    #ifdef __h5__
        inith5();
    #endif

    #ifdef __legacy__
        initlegacy();
    #endif

    #ifdef __netsvc__
        initnetsvc();
    #endif

    #ifdef __pistache__
        initpistache();
    #endif

    /* Load Plugins */
    ldplugins();

    /* Get Interpreter Arguments */
    int lua_argc = argc; // "-i" is plus one, executable is minus one
    char(*lua_argv)[LuaEngine::MAX_LUA_ARG] = new char[lua_argc][LuaEngine::MAX_LUA_ARG];
    StringLib::copy(lua_argv[0], "-i", LuaEngine::MAX_LUA_ARG);
    for(int i = 1; i < argc; i++) StringLib::copy(lua_argv[i], argv[i], LuaEngine::MAX_LUA_ARG);

    /* Create Lua Engine */
    LuaEngine* interpreter = new LuaEngine("sliderule", lua_argc, lua_argv, ORIGIN, lua_abort_hook);

    /* Run Application */
    while(checkactive())
    {
        #ifdef CUSTOM_ALLOCATOR
            static int secmod = 0;
            if(secmod++ % 10 == 0)
            {
                displayCount();
            }
        #endif
        LocalLib::sleep(1);
    }

    /* Free Interpreter */
    delete interpreter;
    delete [] lua_argv;

    /* Unload Plugins */
    ulplugins();

    /* Clean Up Built-In Packages */
    #ifdef __pistache__
        deinitpistache();
    #endif

    #ifdef __netsvc__
        deinitnetsvc();
    #endif

    #ifdef __legacy__
        deinitlegacy();
    #endif

    #ifdef __h5__
        deinith5();
    #endif

    #ifdef __geo__
        deinitgeo();
    #endif

    #ifdef __ccsds__
        deinitccsds();
    #endif

    #ifdef __aws__
        deinitaws();
    #endif

    #ifdef __arrow__
        deinitarrow();
    #endif

    int errors = geterrors();
    deinitcore();

    /* Exit Thread Managing Signals */
    app_signal_abort = true;
    pthread_kill(signal_pid, SIGINT);
    pthread_join(signal_pid, NULL);

    /* Exit Process */
    return errors;
}
