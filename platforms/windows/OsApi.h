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

#ifndef __osapi__
#define __osapi__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <SDKDDKVer.h>

#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <Winsock2.h>
#include <direct.h>
#include <exception>
#include <stdexcept>

/******************************************************************************
 * PRAGMAS
 ******************************************************************************/

#ifdef _VS_
#pragma warning(suppress: 6031)
#endif

/******************************************************************************
 * MACROS
 ******************************************************************************/

#ifndef NULL
#define NULL        ((void *) 0)
#endif

#ifndef MIN
#define MIN(a,b)    ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)    ((a) > (b) ? (a) : (b))
#endif

#define mkdir(a,b)  _mkdir(a)

#ifndef CompileTimeAssert
#define CompileTimeAssert(Condition, Message) typedef char Message[(Condition) ? 1 : -1]
#endif

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define fileno                      _fileno

#define PATH_DELIMETER              '\\'

/* Return Codes */
#define TIMEOUT_RC                  (0)
#define INVALID_RC                  (-1)
#define SHUTDOWN_RC                 (-2)
#define TCP_ERR_RC                  (-3)
#define UDP_ERR_RC                  (-4)
#define SOCK_ERR_RC                 (-5)
#define BUFF_ERR_RC                 (-6)
#define WOULDBLOCK_RC               (-7)
#define PARM_ERR_RC                 (-8)
#define TTY_ERR_RC                  (-9)
#define ACC_ERR_RC                  (-10)

/* I/O Definitions */
#define IO_PEND                     (-1)
#define IO_CHECK                    (0)
#define IO_DEFAULT_TIMEOUT          (1000) // ms
#define IO_DEFAULT_MAXSIZE          (0x10000) // bytes
#define IO_INFINITE_CONNECTIONS     (-1)
#define IO_ALIVE_FLAG               (0x01)
#define IO_READ_FLAG                (0x02)
#define IO_WRITE_FLAG               (0x04)
#define IO_CONNECT_FLAG             (0x08)
#define IO_DISCONNECT_FLAG          (0x10)
#define SYS_TIMEOUT                 (LocalLib::getIOTimeout()) // ms
#define SYS_MAXSIZE                 (LocalLib::getIOMaxsize()) // bytes

/* Strings */
#define MAX_STR_SIZE                1024

/* Debug Logging */
#define dlog(...)                   LocalLib::print(__FILE__,__LINE__,__VA_ARGS__)

/******************************************************************************
 * THREAD CLASS
 ******************************************************************************/

class Thread
{
    public:

        typedef void* (*thread_func_t) (void* parm);

        Thread (thread_func_t function, void* parm, bool _join=true);
        ~Thread (void); // performs join

    private:

        HANDLE threadId;
        bool join;
};

/******************************************************************************
 * MUTUAL EXCLUSION (MUTEX) CLASS
 ******************************************************************************/

class Mutex
{
    public:

        Mutex (void);
        ~Mutex (void);

        void lock (void);
        void unlock (void);

    protected:

        CRITICAL_SECTION mutexId; // mutex only within same process (mutex between processes on windows requires mutex handle)
};

/******************************************************************************
 * CONDITION CLASS
 ******************************************************************************/

class Cond : public Mutex
{
    public:

        typedef enum {
            NOTIFY_ONE,
            NOTIFY_ALL
        } notify_t;

        Cond    (int num_sigs = 1);
        ~Cond   (void);

        void signal(int sig = 0, notify_t notify = NOTIFY_ALL);
        bool wait(int sig = 0, int timeout_ms = 0);

    private:

        int numSigs;
        CONDITION_VARIABLE* condId;
};

/******************************************************************************
 * SEMAPHORE CLASS
 ******************************************************************************/

class Sem
{
    public:

        Sem (void);
        ~Sem (void);

        void give (void);
        bool take (int timeout_ms=0);

    private:

        HANDLE semId;
};

/******************************************************************************
 * INVALID TIMER EXCEPTION
 ******************************************************************************/

class InvalidTimerException : public std::runtime_error
{
   public:
       InvalidTimerException() : std::runtime_error("InvalidTimerException") { }
};

/******************************************************************************
 * TIMER CLASS
 ******************************************************************************/

class Timer
{
    public:

        typedef void (*timerHandler_t) (void);

        Timer (timerHandler_t handler, int period_ms);
        ~Timer (void);        

    private:

        static int timerNum;
        static Mutex timerMut;

        HANDLE timerId;
        timerHandler_t timerHandler;
        Thread* handlerPid;
        bool alive;

        static void* _handler (void* parm);
};

/******************************************************************************
 * SOCKET LIBRARY CLASS
 ******************************************************************************/

class SockLib
{
    public:

        typedef int(*onPollHandler_t) (int* flags, void* parm); // configure R/W flags
        typedef int(*onActiveHandler_t) (int sock, int flags, void* parm);

        static const int PORT_STR_LEN = 16;
        static const int HOST_STR_LEN = 64;
        static const int SERV_STR_LEN = 64;

        static void         initLib             (void); // initializes library
        static void         deinitLib           (void); // de-initializes library
        static void         signalexit          (void); // de-initializes library
        static int          sockstream          (const char* ip_addr, int port, bool is_server, bool* block);
        static int          sockdatagram        (const char* ip_addr, int port, bool is_server, bool* block, const char* multicast_group);
        static int          socksend            (int fd, const void* buf, int size, int timeout);
        static int          sockrecv            (int fd, void* buf, int size, int timeout);
        static int          sockinfo            (int fd, char** local_ipaddr, int* local_port, char** remote_ipaddr, int* remote_port);
        static void         sockclose           (int fd);
        static int          startserver         (const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, void* parm);
        static int          startclient         (const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, void* parm);

    protected:

        static int          sockclient          (const char* ip_addr, int port, int retries);
        static int          sockserver          (const char* ip_addr, int port, int retries);
};

/******************************************************************************
 * LOCAL LIBRARY CLASS
 ******************************************************************************/

class LocalLib
{
    public:

        typedef void(*print_func_t) (const char* file_name, unsigned int line_number, const char* message);

        static const int SYS_CLK = 0; // system clock that can be converted into civil time
        static const int CPU_CLK = 1; // processor clock that only counts ticks
        static const int MAX_PRINT_MESSAGE = 256;

        static void         initLib             (void); // initializes library
        static void         deinitLib           (void); // de-initializes library
        static void         setPrint            (print_func_t _print_func);
        static void         print               (const char* file_name, unsigned int line_number, const char* format_string, ...)  __attribute__((format(printf, 3, 4)));
        static void         sleep               (int secs);
        static void*        copy                (void* dst, const void* src, int len);
        static void*        move                (void* dst, const void* src, int len);
        static void*        set                 (void* buf, int val, int len);
        static const char*  err2str             (int errnum);
        static int64_t      time                (int clkid);
        static int64_t      timeres             (int clkid); // returns the resolution of the clock
        static uint16_t     swaps               (uint16_t val);
        static uint32_t     swapl               (uint32_t val);
        static uint64_t     swapll              (uint64_t val);
        static float        swapf               (float val);
        static double       swaplf              (double val);
        static int          nproc               (void);
        static void         setIOMaxsize        (int maxsize);
        static int          getIOMaxsize        (void);
        static void         setIOTimeout        (int timeout);
        static int          getIOTimeout        (void);
        static int          performIOTimeout    (void);

    private:

        static print_func_t print_func;
        static int io_timeout;
        static int io_maxsize;
};

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

#endif  /* __osapi__ */
