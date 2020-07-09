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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <termios.h>
#include <sys/sysinfo.h>

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void TTYLib::init()
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void TTYLib::deinit(void)
{
}

/*----------------------------------------------------------------------------
 * ttyopen
 *----------------------------------------------------------------------------*/
int TTYLib::ttyopen(const char* _device, int _baud, char _parity)
{
    int fd = INVALID_RC;
    int baud = 0;
    int parity = 0;

    /* Set Parity */
    if(_parity == 'N' || _parity == 'n')
    {
        parity = 0; // no parity
    }
    else if(_parity == 'O' || _parity == 'o')
    {
        parity = PARENB | PARODD; // odd parity
    }
    else if(_parity == 'E' || _parity == 'e')
    {
        parity = PARENB; // event parity
    }

    /* Set Baud */
    switch(_baud)
    {
        case     50:    baud = B50;      break;
        case     75:    baud = B75;      break;
        case    110:    baud = B110;     break;
        case    134:    baud = B134;     break;
        case    150:    baud = B150;     break;
        case    200:    baud = B200;     break;
        case    300:    baud = B300;     break;
        case    600:    baud = B600;     break;
        case   1200:    baud = B1200;    break;
        case   1800:    baud = B1800;    break;
        case   2400:    baud = B2400;    break;
        case   4800:    baud = B4800;    break;
        case   9600:    baud = B9600;    break;
        case  19200:    baud = B19200;   break;
        case  38400:    baud = B38400;   break;
        case 115200:    baud = B115200;  break;
        case 230400:    baud = B230400;  break;
        case 460800:    baud = B460800;  break;
        case 500000:    baud = B500000;  break;
        case 576000:    baud = B576000;  break;
        case 921600:    baud = B921600;  break;
        case 1000000:   baud = B1000000; break;
        case 1152000:   baud = B1152000; break;
        default:        baud = B0;       break;
    }

    /* Set Device */
    if(_device)
    {
        /* Set File Descriptor */
        fd = open(_device, O_RDWR | O_NOCTTY | O_NDELAY);
        if(fd < 0)
        {
            dlog("Failed (%d) to open %s: %s\n", errno, _device, strerror(errno));
            fd = INVALID_RC;
        }
        else
        {
            struct termios tty;
            memset(&tty, 0, sizeof(tty));
            if(tcgetattr (fd, &tty) != 0)
            {
                dlog("Failed (%d) tcgetattr for %s: %s\n", errno, _device, strerror(errno));
                fd = INVALID_RC;
            }
            else
            {
                cfsetospeed (&tty, baud);
                cfsetispeed (&tty, baud);

                tty.c_cflag     = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
                tty.c_iflag    &= ~IGNBRK;                          // disable break processing
                tty.c_iflag    &= ~(INLCR | ICRNL | IUCLC);         // no remapping on input
                tty.c_lflag     = 0;                                // no signaling chars, no echo
                tty.c_oflag     = 0;                                // no remapping, no delays
                tty.c_cc[VMIN]  = 0;                                // read is non-blocking
                tty.c_cc[VTIME] = 0;                                // no read timeout
                tty.c_iflag    &= ~(IXON | IXOFF | IXANY);          // shut off xon/xoff ctrl
                tty.c_cflag    |= (CLOCAL | CREAD);                 // ignore modem controls
                tty.c_cflag    &= ~(PARENB | PARODD);               // initialize parity to off
                tty.c_cflag    |= parity;                           // set parity
                tty.c_cflag    &= ~CSTOPB;                          // send one stop bit
                tty.c_cflag    &= ~CRTSCTS;                         // no hardware flow control

                if(tcsetattr (fd, TCSANOW, &tty) != 0)
                {
                    dlog("Failed (%d) tcsetattr for %s: %s\n", errno, _device, strerror(errno));
                    fd = INVALID_RC;
                }
            }
        }
    }

    /* Return Descriptor */
    return fd;
}

/*----------------------------------------------------------------------------
 * ttyclose
 *----------------------------------------------------------------------------*/
void TTYLib::ttyclose(int fd)
{
    close(fd);
}

/*----------------------------------------------------------------------------
 * ttywrite
 *----------------------------------------------------------------------------*/
int TTYLib::ttywrite(int fd, const void* buf, int size, int timeout)
{
    unsigned char* cbuf = (unsigned char*)buf;
    int activity = 1;
    int revents = POLLOUT;

    /* Check Device */
    if(fd == INVALID_RC)
    {
        if(timeout != IO_CHECK) LocalLib::performIOTimeout();
        return TIMEOUT_RC;
    }

    /* Send Data */
    int c = 0;
    while(c >= 0 && c < size)
    {
        /* Perform Poll if not Checking */
        if(timeout != IO_CHECK)
        {
            /* Build Poll Structure */
            struct pollfd polllist[1];
            polllist[0].fd = fd;
            polllist[0].events = POLLOUT | POLLHUP;
            polllist[0].revents = 0;

            /* Poll */
            do activity = poll(polllist, 1, timeout);
            while(activity == -1 && (errno == EINTR || errno == EAGAIN));

            /* Check Activity */
            if(activity <= 0) break;
            revents = polllist[0].revents;
        }

        /* Perform Send */
        if(revents & POLLHUP)
        {
            c = SHUTDOWN_RC;
        }
        else if(revents & POLLOUT)
        {
            int ret = write(fd, &cbuf[c], size - c);
            if(ret > 0)         c += ret;
            else if(ret <= 0)   c = TTY_ERR_RC;
        }
    }

    /* Return Results */
    return c;
}

/*----------------------------------------------------------------------------
 * ttyread
 *----------------------------------------------------------------------------*/
int TTYLib::ttyread(int fd, void* buf, int size, int timeout)
{
    int c = TIMEOUT_RC;
    int revents = POLLIN;

    /* Check Device */
    if(fd == INVALID_RC)
    {
        if(timeout != IO_CHECK) LocalLib::performIOTimeout();
        return TIMEOUT_RC;
    }

    /* Perform Poll if not Checking */
    if(timeout != IO_CHECK)
    {
        struct pollfd polllist[1];
        polllist[0].fd = fd;
        polllist[0].events = POLLIN | POLLHUP;
        polllist[0].revents = 0;

        int activity = 1;
        do activity = poll(polllist, 1, timeout);
        while(activity == -1 && (errno == EINTR || errno == EAGAIN));

        revents = polllist[0].revents;
    }

    /* Perform Receive */
    if(revents & POLLIN)
    {
        c = read(fd, buf, size);
        if(c <= 0) c = TTY_ERR_RC;
    }
    else if(revents & POLLHUP)
    {
        c = SHUTDOWN_RC;
    }

    /* Return Results*/
    return c;
}
