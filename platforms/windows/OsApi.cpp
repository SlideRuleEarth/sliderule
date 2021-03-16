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

#include "OsApi.h"

#include <string.h>
#include <assert.h>
#include <time.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib") 
#pragma comment (lib, "AdvApi32.lib")

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 *  Windows Thread Wrapper Structure
 *----------------------------------------------------------------------------*/
typedef struct {
    Thread::thread_func_t function;
    void* parm;
} win_thread_wrapper_t;

/*----------------------------------------------------------------------------
 *  Windows API Thread Procedure
 *----------------------------------------------------------------------------*/
DWORD WINAPI ThreadProc(_In_ LPVOID lpParameter)
{
    win_thread_wrapper_t* info = (win_thread_wrapper_t*)lpParameter;

    (*info->function)(info->parm);

    delete info;

    return 0; // In C++ this is equivalent to ExitThread()
}

/******************************************************************************
 * THREAD
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Thread::Thread(thread_func_t function, void* parm, bool _join)
{
    DWORD thread_num; // unused

    win_thread_wrapper_t* info = new win_thread_wrapper_t;
    info->function = function;
    info->parm = parm;

    join = _join;
    threadId = CreateThread(NULL,               // default security attributes
                            0,                  // use default stack size  
                            ThreadProc,         // thread function name
                            info,               // argument to thread function 
                            0,                  // use default creation flags 
                            &thread_num);       // returns the thread identifier

    assert(threadId);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Thread::~Thread()
{
    // NOTE - by using this call to wait for the thread to terminate
    // it assumes that the thread is not associated with any graphical
    // user interface elements.  If it is, then the GUI can send it a message
    // which will cause a deadlock in the thread making this call
    DWORD wait = 0;
    if (join) wait = INFINITE;
    WaitForSingleObject(threadId, wait);
}

/******************************************************************************
 * MUTEX
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Mutex::Mutex()
{
    InitializeCriticalSectionAndSpinCount(&mutexId, 0x00000400);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Mutex::~Mutex()
{
    DeleteCriticalSection(&mutexId);
}

/*----------------------------------------------------------------------------
 * lock
 *----------------------------------------------------------------------------*/
void Mutex::lock(void)
{
    EnterCriticalSection(&mutexId);
}

/*----------------------------------------------------------------------------
 * unlock
 *----------------------------------------------------------------------------*/
void Mutex::unlock(void)
{
    LeaveCriticalSection(&mutexId);
}

/******************************************************************************
 * CONDITION
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Cond::Cond(int num_sigs): Mutex()
{
    assert(num_sigs > 0);
    numSigs = num_sigs;
    condId = new CONDITION_VARIABLE[numSigs];
    for (int i = 0; i < numSigs; i++)
    {
        InitializeConditionVariable(&condId[i]);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Cond::~Cond()
{
    delete[] condId;
}

/*----------------------------------------------------------------------------
 * signal
 *----------------------------------------------------------------------------*/
void Cond::signal(int sig, notify_t notify)
{
    assert(sig >= 0 && sig < numSigs);
    if (notify == NOTIFY_ALL)   WakeAllConditionVariable(&condId[sig]);
    else                        WakeConditionVariable(&condId[sig]);
}

/*----------------------------------------------------------------------------
 * wait
 *----------------------------------------------------------------------------*/
bool Cond::wait(int sig, int timeout_ms)
{
    assert(sig >= 0 && sig < numSigs);
    BOOL status = SleepConditionVariableCS(&condId[sig], &mutexId, timeout_ms);

    /* Return Status */
    if (status != 0)
    {
        return true;
    }
    else
    {
        DWORD error_num = GetLastError();
        if (error_num != ERROR_TIMEOUT)
        {
            /* This is a fatal error and represents a bug in the code.
             * All non-timeout scenarios should be handled above. */
            assert(error_num == ERROR_TIMEOUT);
        }
        return false;
    }
}

/******************************************************************************
 * SEMAPHORE
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Sem::Sem()
{
    semId = CreateSemaphore(
                NULL,       // default security attributes
                0,          // initial count
                1,          // maximum count
                NULL);      // unnamed semaphore

    assert(semId);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Sem::~Sem()
{
    CloseHandle(semId);
}

/*----------------------------------------------------------------------------
 * give
 *----------------------------------------------------------------------------*/
void Sem::give(void)
{
    assert(ReleaseSemaphore(semId,      // handle to semaphore
                            1,          // increase count by one
                            NULL) != 0);// not interested in previous count
}

/*----------------------------------------------------------------------------
 * take
 *----------------------------------------------------------------------------*/
bool Sem::take(int timeout_ms)
{
    DWORD dwWaitResult = WaitForSingleObject(
                            semId,          // handle to semaphore
                            timeout_ms);    // time-out interval in milliseconds

    switch (dwWaitResult)
    {
        case WAIT_OBJECT_0:     return true;
        case WAIT_TIMEOUT:      return false;
        case WAIT_ABANDONED:    return false;
        case WAIT_FAILED:       return false;
        default:                return false;
    }
}

/******************************************************************************
 * TIMER
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Static Data
 *----------------------------------------------------------------------------*/
int Timer::timerNum = 0;
Mutex Timer::timerMut;

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Timer::Timer(timerHandler_t handler, int period_ms)
{
    /* Save Handler */
    timerHandler = handler;
    handlerPid = NULL;
    alive = true;

    /* Create Timer */
    char timerName[128];
    timerMut.lock();
    {
        snprintf(timerName, 128, "timer_%d", timerNum);
        timerNum++;
    }
    timerMut.unlock();

    timerId = CreateWaitableTimer(NULL, false, timerName);
    if (timerId == NULL) throw InvalidTimerException();

    /* Start Handler Thread */
    handlerPid = new Thread(_handler, this);
    
    /* Set Timer */
    LARGE_INTEGER liDueTime;
    liDueTime.QuadPart = -1 * period_ms * 10000LL;
    SetWaitableTimer(timerId, &liDueTime, 1, NULL, NULL, false);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Timer::~Timer()
{
    assert(CloseHandle(timerId) != 0);
    alive = false;
    delete handlerPid;    
}

/*----------------------------------------------------------------------------
 * handler
 *----------------------------------------------------------------------------*/
void* Timer::_handler(void* parm)
{
    Timer* t = (Timer*)parm;
    while(t->alive)
    {
        int status = WaitForSingleObject(t->timerId, INFINITE);
        assert(status == WAIT_OBJECT_0);
        t->timerHandler();
    }
    return NULL;
}

/******************************************************************************
 * SOCKET LIBRARY
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * initLib
 *----------------------------------------------------------------------------*/
void SockLib::initLib(void)
{
    WSAStartup(MAKEWORD(2,2), &wsaData);
}

/*----------------------------------------------------------------------------
 * deinitLib
 *----------------------------------------------------------------------------*/
void SockLib::deinitLib(void)
{
    WSACleanup();
}

/*----------------------------------------------------------------------------
 * signalexit
 *----------------------------------------------------------------------------*/
void SockLib::signalexit(void)
{
}

/*----------------------------------------------------------------------------
 * sockstream
 *----------------------------------------------------------------------------*/
int SockLib::sockstream(const char* ip_addr, int port, bool is_server, bool* block)
{
    (void)block;

    if(is_server)
    {
        return sockserver(ip_addr, port, 1);
    }
    else
    {
        return sockclient(ip_addr, port, 1);
    }
}   

/*----------------------------------------------------------------------------
 * sockdatagram
 *----------------------------------------------------------------------------*/
int SockLib::sockdatagram(const char* ip_addr, int port, bool is_server, bool* block, const char* multicast_group)
{
    // STILL TODO

    (void)ip_addr;
    (void)port;
    (void)is_server;
    (void)block;
    (void)multicast_group;

    return INVALID_RC;    
}   

/*----------------------------------------------------------------------------
 * socksend
 *----------------------------------------------------------------------------*/
int SockLib::socksend (int fd, const void* buf, int size, int timeout)
{
    int num_bytes_sent = 0;
    const char* bufptr = (const char*)buf;
    while (num_bytes_sent < size)
    {
        int iResult = send((SOCKET)fd, &bufptr[num_bytes_sent], size - num_bytes_sent, timeout);

        if (iResult == SOCKET_ERROR)
        {
            dlog("Send failed with error: %d", WSAGetLastError());
            return INVALID_RC;
        }
        else
        {
            num_bytes_sent += iResult;
        }
    }

    return num_bytes_sent;
}

/*----------------------------------------------------------------------------
 * sockrecv
 *----------------------------------------------------------------------------*/
int SockLib::sockrecv (int fd, void* buf, int size, int timeout)
{
    int iResult = recv((SOCKET)fd, (char*)buf, size, timeout);
    
    if(iResult == 0)
    {
        dlog("Socket connection closed...");
    }
    else
    {
        dlog("Recv failed with error: %d", WSAGetLastError());
    }

    return iResult;
}

/*----------------------------------------------------------------------------
 * sockinfo
 *----------------------------------------------------------------------------*/
int SockLib::sockinfo(int fd, char** local_ipaddr, int* local_port, char** remote_ipaddr, int* remote_port)
{
    // STILL TODO

    (void)fd;
    (void)local_ipaddr;
    (void)local_port;
    (void)remote_ipaddr;
    (void)remote_port;
    
    return 0;
}

/*----------------------------------------------------------------------------
 * sockclose
 *----------------------------------------------------------------------------*/
void SockLib::sockclose (int fd)
{
    closesocket((SOCKET)fd);
    shutdown((SOCKET)fd, SD_SEND);
}

/*----------------------------------------------------------------------------
 * startserver
 *----------------------------------------------------------------------------*/
int SockLib::startserver(const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, void* parm)
{
    // STILL TODO

    (void)ip_addr;
    (void)port;
    (void)max_num_connections;
    (void)on_poll;
    (void)on_act;
    (void)parm;

    return 0;
}

/*----------------------------------------------------------------------------
 * startclient
 *----------------------------------------------------------------------------*/
int SockLib::startclient(const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, void* parm)
{
    // STILL TODO

    (void)ip_addr;
    (void)port;
    (void)max_num_connections;
    (void)on_poll;
    (void)on_act;
    (void)parm;

    return 0;
}

/*----------------------------------------------------------------------------
 * sockclient
 *----------------------------------------------------------------------------*/
int SockLib::sockclient(const char* ip_addr, int port, int retries)
{
    (void)retries;

    struct addrinfo *result = NULL,
                    *ptr = NULL,
                     hints;
    int iResult;

    char portstr[10];
    memset(portstr, 0, 10);
    snprintf(portstr, 10, "%d", port);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(ip_addr, portstr, &hints, &result);
    if (iResult != 0)
    {
        dlog("getaddrinfo failed: %d", iResult);
        return INVALID_RC;
    }

    // Allocate Socket
    SOCKET ConnectSocket = INVALID_SOCKET;

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) 
        {
            dlog("socket failed with error: %ld", WSAGetLastError());
            return INVALID_RC;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    // Free the results returned by getaddrinfo
    freeaddrinfo(result);

    // Check status of connection
    if (ConnectSocket == INVALID_SOCKET) 
    {
        dlog("unable to connect to %s:%d!", ip_addr, port);
        return INVALID_RC;
    }

    // Return socket handle
    return (int)ConnectSocket;
}

/*----------------------------------------------------------------------------
 * sockserver
 *----------------------------------------------------------------------------*/
int SockLib::sockserver(const char* ip_addr, int port, int retries)
{
    (void)retries;

    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    int iResult;

    char portstr[10];
    memset(portstr, 0, 10);
    snprintf(portstr, 10, "%d", port);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, portstr, &hints, &result);
    if (iResult != 0)
    {
        dlog("getaddrinfo failed: %d", iResult);
        return INVALID_RC;
    }

    // Create a SOCKET for connecting to server
    SOCKET ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) 
    {
        dlog("socket failed with error: %ld", WSAGetLastError());
        freeaddrinfo(result);
        return INVALID_RC;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) 
    {
        dlog("bind failed with error: %d", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        return INVALID_RC;
    }

    // Free the results returned by getaddrinfo
    freeaddrinfo(result);

    // Listen for connection
    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) 
    {
        dlog("listen failed with error: %d", WSAGetLastError());
        closesocket(ListenSocket);
        return INVALID_RC;
    }

    // Accept a client socket
    SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) 
    {
        dlog("accept failed with error: %d", WSAGetLastError());
        closesocket(ListenSocket);
        return INVALID_RC;
    }

    // No longer need server socket
    closesocket(ListenSocket);

    // Return client socket
    return (int)ClientSocket;
}

/******************************************************************************
 * LOCAL LIBRARY
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Static DAta
 *----------------------------------------------------------------------------*/
WSADATA wsaData;
LocalLib::print_func_t LocalLib::print_func = NULL;
int LocalLib::io_timeout = IO_DEFAULT_TIMEOUT;
int LocalLib::io_timeout = IO_DEFAULT_MAXSIZE;

/*----------------------------------------------------------------------------
 * initLib
 *----------------------------------------------------------------------------*/
void LocalLib::initLib(void)
{
}

/*----------------------------------------------------------------------------
 * deinitLib
 *----------------------------------------------------------------------------*/
void LocalLib::deinitLib(void)
{
}

/*----------------------------------------------------------------------------
 * setPrint
 *----------------------------------------------------------------------------*/
void LocalLib::setPrint(print_func_t _print_func)
{
    assert(_print_func); // do not allow function to be set to NULL
    print_func = _print_func;
}

/*----------------------------------------------------------------------------
 * print
 *----------------------------------------------------------------------------*/
void LocalLib::print(const char* file_name, unsigned int line_number, const char* format_string, ...)
{
    if (print_func)
    {
        /* Allocate Message String on Stack */
        char message[MAX_PRINT_MESSAGE+1];

        /* Build Formatted Message String */
        va_list args;
        va_start(args, format_string);
        int msglen = MIN(vsnprintf(message, MAX_PRINT_MESSAGE, format_string, args), MAX_PRINT_MESSAGE);
        va_end(args);
        message[msglen] = '\0';

        /* Call Call Back */
        print_func(file_name, line_number, message);
    }
}

/*----------------------------------------------------------------------------
 * sleep
 *----------------------------------------------------------------------------*/
void LocalLib::sleep(int secs)
{
    ::Sleep(secs * 1000);
}

/*----------------------------------------------------------------------------
 * copy
 *----------------------------------------------------------------------------*/
void* LocalLib::copy(void* dst, const void* src, int len)
{
    return memcpy(dst, src, len);
}

/*----------------------------------------------------------------------------
 * move
 *----------------------------------------------------------------------------*/
void* LocalLib::move(void* dst, const void* src, int len)
{
    return memmove(dst, src, len);
}

/*----------------------------------------------------------------------------
 * set
 *----------------------------------------------------------------------------*/
void* LocalLib::set(void* buf, int val, int len)
{
    return memset(buf, val, len);
}

/*----------------------------------------------------------------------------
 * err2str
 *----------------------------------------------------------------------------*/
const char* LocalLib::err2str(int errnum)
{
    return strerror(errnum);
}

/*----------------------------------------------------------------------------
 * time
 * 
 *  SYS_CLK returns microseconds since unix epoch
 *  CPU_CLK returns monotonically incrementing time normalized number
 *----------------------------------------------------------------------------*/
int64_t LocalLib::time(int clkid)
{
    int64_t now;

    if (clkid == SYS_CLK)
    {
        #define FILETIME_SECONDS_TO_UNIX_EPOCH 11644473600LL // number of seconds from windows epoch to unix epoch
        FILETIME os_time; // file time structure of upper and lower words of 100ns units
        GetSystemTimeAsFileTime(&os_time); // get current window file time
        now  = os_time.dwLowDateTime / 10; // take into microseconds
        now += (os_time.dwHighDateTime / 10) * 0x100000000LL; // take into microseconds and then scale up for upper 32 bits
        now -= FILETIME_SECONDS_TO_UNIX_EPOCH * 1000000; // multiple by number of microseconds in a second
        return now; // us
    }
    else if (clkid == CPU_CLK)
    {
        LARGE_INTEGER os_ticks;
        QueryPerformanceCounter(&os_ticks);
        now = os_ticks.QuadPart;
        return now;
    }
    else
    {
        return 0;
    }
}

/*----------------------------------------------------------------------------
 * timeres
 *
 *  Returns: resolution of specified clock in number of ticks per second 
 *----------------------------------------------------------------------------*/
int64_t LocalLib::timeres(int clkid)
{
    if (clkid == SYS_CLK)
    {
        return 1000000; // microseconds
    }
    else if (clkid == CPU_CLK)
    {
        LARGE_INTEGER os_freq;
        QueryPerformanceFrequency(&os_freq);
        return os_freq.QuadPart;
    }
    else
    {
        return 0;
    }
}

/*----------------------------------------------------------------------------
 * swaps
 *----------------------------------------------------------------------------*/
uint16_t LocalLib::swaps(uint16_t val)
{
    return _byteswap_ushort(val);
}

/*----------------------------------------------------------------------------
 * swapl
 *----------------------------------------------------------------------------*/
uint32_t LocalLib::swapl(uint32_t val)
{
    return _byteswap_ulong(val);
}

/*----------------------------------------------------------------------------
 * swapll
 *----------------------------------------------------------------------------*/
uint64_t LocalLib::swapll(uint64_t val)
{
    return _byteswap_uint64(val);
}

/*----------------------------------------------------------------------------
 * swapf
 *----------------------------------------------------------------------------*/
float LocalLib::swapf(float val)
{
    float rtrndata = 0.0;
    uint8_t* dataptr = (uint8_t*)&rtrndata;
    uint32_t* tempf = (uint32_t*)&val;
    *(uint32_t*)(dataptr) = _byteswap_ulong(*tempf);
    return rtrndata;
}

/*----------------------------------------------------------------------------
 * swaplf
 *----------------------------------------------------------------------------*/
double LocalLib::swaplf(double val)
{
    double rtrndata = 0.0;
    uint8_t* dataptr = (uint8_t*)&rtrndata;
    uint64_t* tempd = (uint64_t*)&val;
    *(uint64_t*)(dataptr) = _byteswap_uint64(*tempd);
    return rtrndata;
}

/*----------------------------------------------------------------------------
 * nproc
 *----------------------------------------------------------------------------*/
int LocalLib::nproc (void)
{
    // STILL TODO

    return 1;
}

/*----------------------------------------------------------------------------
 * setIOMaxsize
 *----------------------------------------------------------------------------*/
void LocalLib::setIOMaxsize(int maxsize)
{
    io_maxsize = maxsize;
}

/*----------------------------------------------------------------------------
 * getIOMaxsize
 *----------------------------------------------------------------------------*/
int LocalLib::getIOMaxsize(void)
{
    return io_maxsize;
}

/*----------------------------------------------------------------------------
 * setIOTimeout
 *----------------------------------------------------------------------------*/
void LocalLib::setIOTimeout(int timeout)
{
    io_timeout = timeout;
}

/*----------------------------------------------------------------------------
 * getIOTimeout
 *----------------------------------------------------------------------------*/
int LocalLib::getIOTimeout(void)
{
    return io_timeout;
}

/*----------------------------------------------------------------------------
 * performIOTimeout
 *----------------------------------------------------------------------------*/
int LocalLib::performIOTimeout(void)
{
    if (io_timeout >= 1000) LocalLib::sleep(io_timeout / 1000);
    else                    LocalLib::sleep(1);
    return TIMEOUT_RC;
}

/******************************************************************************
 * TTY LIBRARY
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * initLib
 *----------------------------------------------------------------------------*/
void TTYLib::initLib()
{
}

/*----------------------------------------------------------------------------
 * deinitLib
 *----------------------------------------------------------------------------*/
void TTYLib::deinitLib(void)
{
}

/*----------------------------------------------------------------------------
 * ttyopen
 *----------------------------------------------------------------------------*/
int TTYLib::ttyopen(const char* _device, int _baud, char _parity)
{
    // STILL TODO

    (void)_device;
    (void)_baud;
    (void)_parity;

    return INVALID_RC;
}

/*----------------------------------------------------------------------------
 * ttyclose
 *----------------------------------------------------------------------------*/
void TTYLib::ttyclose(int fd)
{
    // STILL TODO

    (void)fd;
}

/*----------------------------------------------------------------------------
 * ttywrite
 *----------------------------------------------------------------------------*/
int TTYLib::ttywrite(int fd, const void* buf, int size, int timeout)
{
    // STILL TODO

    (void)fd;
    (void)buf;
    (void)size;
    (void)timeout;

    return 0;
}

/*----------------------------------------------------------------------------
 * ttyread
 *----------------------------------------------------------------------------*/
int TTYLib::ttyread(int fd, void* buf, int size, int timeout)
{
    // STILL TODO

    (void)fd;
    (void)buf;
    (void)size;
    (void)timeout;

    return 0;
}
