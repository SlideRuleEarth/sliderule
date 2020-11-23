/******************************************************************************
 Filename     : SlideRule.cpp
 Purpose      :
 Design Notes :
 ******************************************************************************/

/******************************************************************************
 INCLUDES
 ******************************************************************************/

#include "core.h"
#include "ccsds.h"
#include "legacy.h"
#include "sigview.h"
#include "Shell.h"
#include "Viewer.h"
#include "Charter.h"

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <signal.h>
#include <string.h>
#include <atomic>
#include <gtk/gtk.h>

/******************************************************************************
 TYPEDEFS
 ******************************************************************************/

typedef void (*init_f) (void);

/******************************************************************************
 FILE DATA
 ******************************************************************************/

static bool app_immediate_abort = false;
static bool app_signal_abort = false;
static std::atomic<uint64_t> allocCount = {0};

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
 * console_quick_exit - Signal handler for Control-C
 */
void console_quick_exit(int parm)
{
    (void)parm;
    if(app_immediate_abort) quick_exit(0);
    printf("\n...Shutting down command line interface!\n");
    setinactive(); // tells core package that application is no longer active
    app_immediate_abort = true; // multiple control-c will exit immediately
}

/******************************************************************************
 LOCAL FUNCTIONS
 ******************************************************************************/

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
            printf("Fatal error (%d) ...failed to wait for signal: %s\n", status, strerror(errno));
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
 * lua_abort_hook - Call back for lua engine to check for termination
 */
static void lua_abort_hook (lua_State *L, lua_Debug *ar)
{
    (void)ar;
    if(!checkactive()) luaL_error(L, "Interpreter no longer active - aborting!\n");
}

/* 
 * GTK GUI Thread
 */
void* gtkThread (void* parm)
{
    (void)parm;
    gdk_threads_enter();
    {
        gtk_main();
    }
    gdk_threads_leave();
    return NULL;
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
    initccsds();
    initlegacy();
    initsigview();

    /* Initialize SigView GUI Components */
    extern CommandProcessor* cmdProc;
    cmdProc->registerHandler("VIEWER",  Viewer::createObject,   8,  "<histogram input stream> <science data stream> <time tag processor name 1, 2, and 3> <report processor name> <time processor name>");
    cmdProc->registerHandler("CHARTER", Charter::createObject, -1,  "<export stream> <max number of points to plot>");
    cmdProc->registerHandler("SHELL",   Shell::createObject,    1,  "<log stream>");
    
    /* Create GTK Main Thread */
    new Thread(gtkThread, NULL, false);

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
        LocalLib::sleep(1);
    }

    /* Free Interpreter */
    delete interpreter;
    delete [] lua_argv;

    /* Full Clean Up */
    deinitlegacy();
    deinitccsds();
    deinitcore();
    gtk_main_quit();

    /* Exit Thread Managing Signals */
    app_signal_abort = true;
    pthread_kill(signal_pid, SIGINT);
    pthread_join(signal_pid, NULL);

    /* Exit Process */
    return 0;
}
