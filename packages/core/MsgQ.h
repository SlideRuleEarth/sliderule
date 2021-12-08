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

#ifndef __msgq__
#define __msgq__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Dictionary.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#ifndef MAX_FREE_STACK_SIZE
#define MAX_FREE_STACK_SIZE 4096
#endif

/******************************************************************************
 * MSGQ CLASS
 ******************************************************************************/

class MsgQ
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* subscriber types */
        typedef enum {
            UNSUBSCRIBED = 0,
            SUBSCRIBER_OF_OPPORTUNITY,
            SUBSCRIBER_OF_CONFIDENCE
        } subscriber_type_t;

        typedef struct {
            const char* name;
            int         len;
            const char* state;
            int         subscriptions;
        } queueDisplay_t;

        typedef void (*free_func_t) (void* obj, void* parm);

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int CFG_DEPTH_INFINITY     = 0;
        static const int CFG_DEPTH_STANDARD     = -1;
        static const int CFG_SIZE_INFINITY      = 0;
        static const int STATE_OKAY             = 1;
        static const int STATE_TIMEOUT          = TIMEOUT_RC;
        static const int STATE_FULL             = -1;
        static const int STATE_SIZE_ERROR       = -2;
        static const int STATE_ERROR            = -3;
        static const int STATE_EMPTY            = -4;
        static const int STATE_NO_SUBSCRIBERS   = -5;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        MsgQ            (const char* name, free_func_t free_func=NULL, int depth=CFG_DEPTH_STANDARD, int data_size=CFG_SIZE_INFINITY);
                        MsgQ            (const MsgQ& existing_q, free_func_t free_func=NULL);
                        ~MsgQ           (void);

                int     getCount        (void);
                int     getDepth        (void);
         const  char*   getName         (void);
                int     getSubCnt       (void);
                int     getState        (void);
                bool    isFull          (void);

        static  void    init            (void);
        static  void    deinit          (void);
        static  bool    existQ          (const char* qname);
        static  int     numQ            (void); // number of registered queues
        static  int     listQ           (queueDisplay_t* list, int list_size);
        static  bool    setStdQDepth    (int depth);

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MSGQ_DEFAULT_SUBSCRIBERS = 2;
        static const unsigned int MSGQ_COPYQ_MASK = 1 << ((sizeof(unsigned int) * 8) - 1);

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* signal_types_t */
        typedef enum {
            READY2RECV = 0,
            READY2POST = 1,
            NUMSIGS = 2
        } signal_types_t;

        /* queue_node_t */
        typedef struct queue_node_s {
            char*                   data;
            struct queue_node_s*    next;                               // used for FIFO message queue
            unsigned int            mask;                               // msb is type, rest is size
            int                     refs;                               // reference count used for dynamic deallocation
        } queue_node_t;

        /* message_queue_t */
        typedef struct {
            queue_node_t*           front;                              // queue out
            queue_node_t*           back;                               // queue in
            const char*             name;                               // name of the message queue
            int                     depth;                              // maximum number of items queue can hold
            int                     len;                                // current number of items queue is holding
            int                     max_data_size;                      // maximum size of an item that is allowed to be queued
            int                     soo_count;                          // the number of subscriber of opportunities subscribed to this queue
            free_func_t             free_func;                          // call-back to delete data when nodes reclaimed
            Cond*                   locknblock;                         // conditional "signal" when queue has data to be read or able to be written
            int                     state;                              // state of queue
            int                     attachments;                        // number of publishers and subscribers on this queue
            int                     subscriptions;                      // number of subscribers on this queue
            int                     max_subscribers;                    // current allocation of subscriber-based buffers
            subscriber_type_t*      subscriber_type;                    // [max_subscribers] type of subscription for the id
            queue_node_t**          curr_nodes;                         // [max_subscribers] used for subscriptions
            char**                  free_block_stack;                   // [free_stack_size] optimization of memory usage: deallocate in groups
            int                     free_blocks;                        // current number of blocks of free_block_stack
        } message_queue_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static int                          StandardQueueDepth;
        static Dictionary<message_queue_t*> queues;
        static Mutex                        listmut;

        message_queue_t* msgQ;
};

/******************************************************************************
 * PUBLISHER CLASS
 ******************************************************************************/

class Publisher: public MsgQ
{
    public:

        static const int MAX_POSTED_STR = 1024;

                Publisher       (const char* name, MsgQ::free_func_t free_func=NULL, int depth=CFG_DEPTH_STANDARD, int data_size=CFG_SIZE_INFINITY);
                Publisher       (const MsgQ& existing_q, MsgQ::free_func_t free_func=NULL);
                ~Publisher      (void);


        int     postRef         (void* data, int size, int timeout=IO_CHECK);
        int     postCopy        (const void* data, int size, int timeout=IO_CHECK);
        int     postCopy        (const void* data, int size, const void* secondary_data, int secondary_size, int timeout=IO_CHECK);
        int     postString      (const char* format_string, ...) VARG_CHECK(printf, 2, 3);

    private:

        int     post            (void* data, unsigned int mask, void* secondary_data, unsigned int secondary_size, int timeout);

};

/******************************************************************************
 * SUBSCRIBER CLASS
 ******************************************************************************/

class Subscriber: public MsgQ
{
    public:

        typedef struct {
            void*   data;
            int     size;
            int     state;
            void*   _handle;
        } msgRef_t;

                Subscriber      (const char* name, subscriber_type_t type=SUBSCRIBER_OF_CONFIDENCE, int depth=CFG_DEPTH_STANDARD, int data_size=CFG_SIZE_INFINITY);
                Subscriber      (const MsgQ& existing_q, subscriber_type_t type=SUBSCRIBER_OF_CONFIDENCE);
                ~Subscriber     (void);

        bool    dereference     (msgRef_t& ref, bool with_delete=true);
        void    drain           (bool with_delete=true);
        bool    isEmpty         (void);
        void*   getData         (void* _handle, int* size=NULL);

        int     receiveRef      (msgRef_t& ref, int timeout);
        int     receiveCopy     (void* data, int size, int timeout);

    private:

        int id;                 // index into current node table

        int     receive         (msgRef_t& ref, int size, int timeout, bool copy=false);
        bool    reclaim_nodes   (bool delete_data);
        void    init_subscriber (subscriber_type_t type);
};

#endif  /* __msgq__ */
