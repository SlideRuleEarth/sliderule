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

#include "MsgQ.h"
#include "OsApi.h"
#include "Dictionary.h"
#include "StringLib.h"

#include <cstdarg>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

int MsgQ::StandardQueueDepth = MsgQ::CFG_DEPTH_INFINITY;
Dictionary<MsgQ::global_queue_t> MsgQ::queues;
Mutex MsgQ::listmut;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
MsgQ::MsgQ(const char* name, MsgQ::free_func_t free_func, int depth, int data_size)
{
    /* Create Queue */
    listmut.lock();
    {
        try
        {
            msgQ = queues[name].queue; // exception thrown here if name is null or not found
            msgQ->attachments++; // if found, then another attachment is made
            // set free function to support publishers later establishing a
            // free function on a queue created by a subscriber
            if(!msgQ->free_func && free_func) msgQ->free_func = free_func;
        }
        catch(RunTimeException& e)
        {
            // Allocate and initialize new message queue structure
            msgQ                    = new message_queue_t;
            msgQ->front             = NULL;
            msgQ->back              = NULL;
            msgQ->name              = StringLib::duplicate(name);
            msgQ->len               = 0;
            msgQ->max_data_size     = data_size;
            msgQ->soo_count         = 0;
            msgQ->free_func         = free_func;
            msgQ->locknblock        = new Cond(NUMSIGS);
            msgQ->state             = STATE_OKAY;
            msgQ->attachments       = 1;
            msgQ->subscriptions     = 0;
            msgQ->max_subscribers   = MSGQ_DEFAULT_SUBSCRIBERS;
            msgQ->free_blocks       = 0;

            // Set depth
            if(depth == CFG_DEPTH_STANDARD) msgQ->depth = StandardQueueDepth;
            else                            msgQ->depth = depth;

            // Allocate free block stack
            msgQ->free_block_stack = new char* [MAX_FREE_STACK_SIZE];

            // Create subscriber arrays
            msgQ->subscriber_type = new subscriber_type_t [msgQ->max_subscribers];
            msgQ->curr_nodes = new queue_node_t* [msgQ->max_subscribers];
            for(int i = 0; i < msgQ->max_subscribers; i++)
            {
                msgQ->subscriber_type[i] = UNSUBSCRIBED;
                msgQ->curr_nodes[i] = NULL;
            }

            // Register message queue with non-null name
            const global_queue_t global_queue = { .queue = msgQ };
            if(msgQ->name) queues.add(msgQ->name, global_queue);
        }
    }
    listmut.unlock();
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
MsgQ::MsgQ(const MsgQ& existing_q, free_func_t free_func)
{
    listmut.lock();
    {
        msgQ = existing_q.msgQ;
        msgQ->attachments++;
        // set free function to support publishers later establishing a
        // free function on a queue created by a subscriber
        if(!msgQ->free_func && free_func) msgQ->free_func = free_func;
    }
    listmut.unlock();
}

/*----------------------------------------------------------------------------
 * Destructor
 *
 *  message queues are persistent and cannot be deleted, for this reason the
 *  only two object variables - msgQ and msgRec - are left alone
 *----------------------------------------------------------------------------*/
MsgQ::~MsgQ()
{
    listmut.lock();
    {
        msgQ->attachments--;
        if(msgQ->attachments == 0)
        {
            /* Unregister message queue */
            if(msgQ->name) queues.remove(msgQ->name);

            /* Free message queue resources */
            delete msgQ->locknblock;
            delete [] msgQ->name;
            delete [] msgQ->free_block_stack;
            delete [] msgQ->subscriber_type;
            delete [] msgQ->curr_nodes;

            /* Free Message Q */
            delete msgQ;
        }
    }
    listmut.unlock();
}

/*----------------------------------------------------------------------------
 * getCount
 *----------------------------------------------------------------------------*/
int MsgQ::getCount(void)
{
    return msgQ->len;
}

/*----------------------------------------------------------------------------
 * getDepth
 *----------------------------------------------------------------------------*/
int MsgQ::getDepth(void)
{
    return msgQ->depth;
}

/*----------------------------------------------------------------------------
 * getName
 *----------------------------------------------------------------------------*/
const char* MsgQ::getName(void)
{
    return msgQ->name;
}

/*----------------------------------------------------------------------------
 * getSubCnt
 *----------------------------------------------------------------------------*/
int MsgQ::getSubCnt(void)
{
    return msgQ->subscriptions;
}

/*----------------------------------------------------------------------------
 * getState
 *----------------------------------------------------------------------------*/
int MsgQ::getState(void)
{
    return msgQ->state;
}

/*----------------------------------------------------------------------------
 * isFull
 *----------------------------------------------------------------------------*/
bool MsgQ::isFull(void)
{
    if(msgQ->depth == CFG_DEPTH_INFINITY)
    {
        return false;
    }

    return (msgQ->len >= msgQ->depth);
}

/*----------------------------------------------------------------------------
 * init
 *
 *  assumed thread safe context
 *----------------------------------------------------------------------------*/
void MsgQ::init(void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *
 *  assumed thread safe context
 *  free blocks cleaned up in the subscriber class
 *----------------------------------------------------------------------------*/
void MsgQ::deinit(void)
{
    global_queue_t curr_q;
    const char* curr_name = queues.first(&curr_q);
    while(curr_name)
    {
        delete [] curr_q.queue->name;
        delete [] curr_q.queue->free_block_stack;
        delete [] curr_q.queue->subscriber_type;
        delete [] curr_q.queue->curr_nodes;
        delete curr_q.queue;
        curr_name = queues.next(&curr_q);
    }
}

/*----------------------------------------------------------------------------
 * existQ
 *----------------------------------------------------------------------------*/
bool MsgQ::existQ(const char* qname)
{
    return queues.find(qname);
}

/*----------------------------------------------------------------------------
 * numQ
 *----------------------------------------------------------------------------*/
int MsgQ::numQ(void)
{
    return queues.length();
}

/*----------------------------------------------------------------------------
 * listQ
 *----------------------------------------------------------------------------*/
int MsgQ::listQ(queueDisplay_t* list, int list_size)
{
    int j = 0;
    global_queue_t curr_q;
    const char* curr_name = queues.first(&curr_q);
    while(curr_name)
    {
        if(j >= list_size) break;
        list[j].name = curr_q.queue->name;
        list[j].len = curr_q.queue->len;
        list[j].subscriptions = curr_q.queue->subscriptions;
        switch(curr_q.queue->state)
        {
            case STATE_OKAY         : list[j].state = "OKAY";       break;
            case STATE_TIMEOUT      : list[j].state = "TIMEOUT";    break;
            case STATE_FULL         : list[j].state = "FULL";       break;
            case STATE_SIZE_ERROR   : list[j].state = "ERRSIZE";    break;
            case STATE_ERROR        : list[j].state = "ERROR";      break;
            case STATE_EMPTY        : list[j].state = "EMPTY";      break;
            default                 : list[j].state = "UNKNOWN";    break;
        }
        j++;
        curr_name = queues.next(&curr_q);
    }
    return j;
}

/*----------------------------------------------------------------------------
 * setStdQDepth
 *----------------------------------------------------------------------------*/
bool MsgQ::setStdQDepth(int depth)
{
    if(depth >= 0)
    {
        StandardQueueDepth = depth;
        return true;
    }

    return false;
}

/******************************************************************************
 * PUBLISHER METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Publisher::Publisher(const char* name, MsgQ::free_func_t free_func, int depth, int data_size): MsgQ(name, free_func, depth, data_size)
{
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Publisher::Publisher (const MsgQ& existing_q, MsgQ::free_func_t free_func): MsgQ(existing_q, free_func)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Publisher::~Publisher() = default;

/*----------------------------------------------------------------------------
 * postRef
 *
 *  Assumptions:
 *  1. free_func != NULL
 *  2. data != NULL
 *  3. size > 0
 *----------------------------------------------------------------------------*/
int Publisher::postRef(void* data, int size, int timeout)
{
    return post(data, ((unsigned int)size) & ~MSGQ_COPYQ_MASK, NULL, 0, timeout);
}

/*----------------------------------------------------------------------------
 * postCopy
 *
 *  Assumptions:
 *  1. data != NULL
 *  2. size > 0
 *
 *  Notes:
 *  1. wraps the post call with a copy in setting
 *----------------------------------------------------------------------------*/
int Publisher::postCopy(const void* data, int size, int timeout)
{
    return post(const_cast<void*>(data), static_cast<unsigned int>(size) | MSGQ_COPYQ_MASK, NULL, 0, timeout);
}

/*----------------------------------------------------------------------------
 * postCopy
 *
 *  Assumptions:
 *  1. data != NULL
 *  2. size > 0
 *
 *  Notes:
 *  1. wraps the post call with a copy in setting
 *  2. size is checked for being positive
 *  3. Rollover check for size + secondary_size is not needed because both
 *     are promoted to unsigned in the post() call, which can hold the sum
 *     of two max signed-int values
 *----------------------------------------------------------------------------*/
int Publisher::postCopy(const void* data, int size, const void* secondary_data, int secondary_size, int timeout)
{
    if(size < 0 || secondary_size < 0) return STATE_SIZE_ERROR;

    return post(const_cast<void*>(data), static_cast<unsigned int>(size) | MSGQ_COPYQ_MASK,
                const_cast<void*>(secondary_data), static_cast<unsigned int>(secondary_size), timeout);
}

/*----------------------------------------------------------------------------
 * postString
 *
 *  Notes:
 *  1. posts copy of string to queue with a check
 *  2. the safest of all msgq post calls
 *----------------------------------------------------------------------------*/
int Publisher::postString(const char* format_string, ...)
{
    /* Check format_string */
    if(format_string == NULL) return STATE_ERROR;

    /* Build Formatted Message String */
    va_list args;
    va_start(args, format_string);
    char str[MAX_POSTED_STR + 1];
    const int vlen = vsnprintf(str, MAX_POSTED_STR, format_string, args);
    int slen = MIN(vlen, MAX_POSTED_STR);
    va_end(args);
    if (slen < 2) return STATE_SIZE_ERROR; // do not send null strings
    str[slen] = '\0'; // guarantee null termination
    slen++; // include null termination in message

    /* Post the String */
    const int status = post(str, ((unsigned int)slen) | MSGQ_COPYQ_MASK, NULL, 0, IO_CHECK);
    if(status == STATE_OKAY) return slen;
    return status;
}

/*----------------------------------------------------------------------------
 * defaultFree
 *----------------------------------------------------------------------------*/
void Publisher::defaultFree(void* obj, void* parm)
{
    (void)parm;
    delete[] reinterpret_cast<char*>(obj);
}

/*----------------------------------------------------------------------------
 * post
 *----------------------------------------------------------------------------*/
int Publisher::post(void* data, unsigned int mask, const void* secondary_data, unsigned int secondary_size, int timeout)
{
    int         post_state  = STATE_OKAY;
    const bool  copy        = (mask & MSGQ_COPYQ_MASK) != 0;
    const int   data_size   = mask & ~MSGQ_COPYQ_MASK;

    /* post data */
    msgQ->locknblock->lock();
    {
        /* check ability to queue */
        if(msgQ->max_data_size != CFG_SIZE_INFINITY &&
           (data_size + secondary_size) > (unsigned int)msgQ->max_data_size)
        {
            /* size is too big */
            post_state = STATE_SIZE_ERROR;
        }
        else if(msgQ->subscriptions <= 0)
        {
            /* don't post messages to a queue with no subscribers */
            post_state = STATE_NO_SUBSCRIBERS;
        }
        else if(timeout != IO_CHECK)
        {
            /* wait for room in queue */
            while(isFull())
            {
                if(!msgQ->locknblock->wait(READY2POST, timeout))
                {
                    post_state = STATE_TIMEOUT;
                    break;
                }
            }
        }
        else if(isFull())
        {
            /* post check on full queue */
            post_state = STATE_FULL;
        }

        /* if state is okay proceed with enqueue */
        if(post_state == STATE_OKAY)
        {
            /* Allocate Memory for Node */
            int memory_needed = sizeof(queue_node_t);
            if(copy)
            {
                memory_needed += data_size;
                if(secondary_data)
                {
                    memory_needed += secondary_size;
                }
            }

            /* create temp node */
            queue_node_t* temp = reinterpret_cast<queue_node_t*>(new char [memory_needed]);

            /* perform copy if queue is a copy queue */
            if(copy)
            {
                temp->data = reinterpret_cast<char*>(temp) + sizeof(queue_node_t);
                memcpy(temp->data, data, data_size);
                if(secondary_data)
                {
                    memcpy(temp->data + data_size, secondary_data, secondary_size);
                }
            }
            else
            {
                temp->data = reinterpret_cast<char*>(data);
            }

            /* construct node to be added */
            temp->mask = mask + secondary_size;
            temp->next = NULL; // for queue
            temp->refs = msgQ->subscriptions;

            /* place temp node into queue */
            if(msgQ->back == NULL)  msgQ->front = temp;
            else                    msgQ->back->next = temp;
            msgQ->back = temp;

            /* update subscribers */
            for(int i = 0; i < msgQ->max_subscribers; i++)
            {
                /* modify current node if necessary */
                if( (msgQ->subscriber_type[i] != UNSUBSCRIBED) &&
                    (msgQ->curr_nodes[i] == NULL) )
                {
                    msgQ->curr_nodes[i] = temp;
                }
            }

            /* increment queue size */
            msgQ->len++;

            /* trigger ready */
            msgQ->locknblock->signal(READY2RECV);
        }
        else if(post_state == STATE_NO_SUBSCRIBERS && copy)
        {
            /* The STATE_NO_SUBSCRIBERS error is only raised when passing by
             * reference because the publisher message queue object does not
             * own the ability to dereference the data being attempted
             * to be posted (that is a function of the subscriber); so the
             * poster must handle the deallocation on a failed post in this
             * case. No error is raised when posting by copy since the poster
             * has no responsibility in either case (success or failure). */
            post_state = STATE_OKAY;
        }

        /* set queue state */
        msgQ->state = post_state;

        /* if still room wake up other publishers */
        if(!isFull())
        {
            msgQ->locknblock->signal(READY2POST, Cond::NOTIFY_ONE);
        }
    }
    msgQ->locknblock->unlock();

    /* return */
    return post_state;
}

/******************************************************************************
 * SUBSCRIBER METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Subscriber::Subscriber(const char* name, subscriber_type_t type, int depth, int data_size): MsgQ(name, NULL, depth, data_size)
{
    init_subscriber(type);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Subscriber::Subscriber(const MsgQ& existing_q, subscriber_type_t type): MsgQ(existing_q, NULL)
{
    init_subscriber(type);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Subscriber::~Subscriber()
{
    msgQ->locknblock->lock();
    {
        /* Dereference All Nodes */
        queue_node_t* node = msgQ->curr_nodes[id];
        while(node != NULL)
        {
            node->refs--;
            node = node->next;
        }
        const bool space_reclaimed = reclaim_nodes(true);

        /* Clean up Remaining Free Blocks */
        if(msgQ->subscriptions == 1)
        {
            if(msgQ->free_blocks > 0)
            {
                for(int i = msgQ->free_blocks - 1; i >= 0; i--)
                {
                    queue_node_t* temp = reinterpret_cast<queue_node_t*>(msgQ->free_block_stack[i]);
                    if((temp->mask & MSGQ_COPYQ_MASK) == 0)
                    {
                        if(msgQ->free_func) (*msgQ->free_func)(temp->data, NULL);
                        else                assert(msgQ->free_func);
                    }
                    delete [] msgQ->free_block_stack[i];
                }
                msgQ->free_blocks = 0;
            }
        }
        msgQ->curr_nodes[id] = NULL;

        /* Unregister */
        if(msgQ->subscriber_type[id] == SUBSCRIBER_OF_OPPORTUNITY) msgQ->soo_count--;
        msgQ->subscriber_type[id] = UNSUBSCRIBED;
        msgQ->subscriptions--;

        /* Signal Publishers */
        if(space_reclaimed)
        {
            msgQ->locknblock->signal(READY2POST, Cond::NOTIFY_ONE);
        }
    }
    msgQ->locknblock->unlock();
}

/*----------------------------------------------------------------------------
 * dereference
 *----------------------------------------------------------------------------*/
bool Subscriber::dereference(msgRef_t& ref, bool with_delete)
{
    /* Null Handles are Vacuous */
    if(ref._handle == NULL) return false;

    /* Cast Handle to Queue Node Pointer */
    queue_node_t* node = static_cast<queue_node_t*>(ref._handle);

    /* Deference Node and Reclaim Freed Space */
    msgQ->locknblock->lock();
    {
        node->refs--;
        const bool space_reclaimed = reclaim_nodes(with_delete);

        if(space_reclaimed)
        {
            msgQ->locknblock->signal(READY2POST, Cond::NOTIFY_ONE);
        }
    }
    msgQ->locknblock->unlock();

    /* Set Handle to Null for Future */
    ref._handle = NULL;

    /* Success */
    return true;
}

/*----------------------------------------------------------------------------
 * drain
 *----------------------------------------------------------------------------*/
void Subscriber::drain(bool with_delete)
{
    msgQ->locknblock->lock();
    {
        /* Dereference All Nodes */
        queue_node_t* node = msgQ->curr_nodes[id];
        while(node != NULL)
        {
            node->refs--;
            node = node->next;
        }
        const bool space_reclaimed = reclaim_nodes(with_delete);
        msgQ->curr_nodes[id] = NULL;

        if(space_reclaimed)
        {
            msgQ->locknblock->signal(READY2POST, Cond::NOTIFY_ONE);
        }
    }
    msgQ->locknblock->unlock();
}

/*----------------------------------------------------------------------------
 * isEmpty
 *----------------------------------------------------------------------------*/
bool Subscriber::isEmpty(void)
{
    return (msgQ->curr_nodes[id] == NULL);
}

/*----------------------------------------------------------------------------
 * getData
 *
 *  Notes
 *  1. must be called prior to call to dereference, otherwise caller does not
 *     own the data anymore
 *----------------------------------------------------------------------------*/
void* Subscriber::getData(void* _handle, int* size)
{
    assert(_handle);
    queue_node_t* node = static_cast<queue_node_t*>(_handle);
    if(size) *size = node->mask & ~MSGQ_COPYQ_MASK;
    return static_cast<void*>(node->data);
}

/*----------------------------------------------------------------------------
 * receiveRef
 *
 *  Notes
 *  1. accepts message reference structure (passed by reference)
 *  2. returns a pointer to the data and the size of the data
 *----------------------------------------------------------------------------*/
int Subscriber::receiveRef(msgRef_t& ref, int timeout)
{
    ref._handle = NULL;
    return receive(ref, CFG_SIZE_INFINITY, timeout, false);
}

/*----------------------------------------------------------------------------
 * receiveCopy
 *
 *  Notes
 *  1. wraps the receive call with a copy out setting
 *  2. this is the safest call to use
 *  3. allows buffer on stack of calling function
 *----------------------------------------------------------------------------*/
int Subscriber::receiveCopy(void* data, int size, int timeout)
{
    assert(data);

    msgRef_t ref;
    ref.data = data;
    const int status = receive(ref, size, timeout, true);
    if(status == STATE_OKAY) return ref.size;
    return status;
}

/*----------------------------------------------------------------------------
 * receive
 *
 *  Notes
 *  1. accepts timeout in milliseconds
 *  2. ref is checked in higher level functions and can be assumed as safe to use
 *----------------------------------------------------------------------------*/
int Subscriber::receive(msgRef_t& ref, int size, int timeout, bool copy)
{
    /* initialize reference structure */
    ref.state = STATE_OKAY;
    ref.size = size;
    ref._handle = 0;

    /* receive data */
    msgQ->locknblock->lock();
    {
        bool space_reclaimed = false;

        /* check state of queue */
        if(timeout != IO_CHECK)
        {
            /* wait for message to be posted */
            while(isEmpty())
            {
                if(!msgQ->locknblock->wait(READY2RECV, timeout))
                {
                    ref.state = STATE_TIMEOUT;
                    break;
                }
            }
        }
        else if(isEmpty())
        {
            /* receive check on empty queue */
            ref.state = STATE_EMPTY;
        }

        /* dequeue data */
        if(ref.state == STATE_OKAY)
        {
            /* update queue status*/
            queue_node_t* node = msgQ->curr_nodes[id];
            msgQ->curr_nodes[id] = node->next;
            const int node_size = node->mask & ~MSGQ_COPYQ_MASK;

            /* perform dequeue */
            if(!copy)
            {
                ref.data = node->data;
                ref.size = node_size;
                ref._handle = static_cast<void*>(node);
            }
            else
            {
                if(node_size <= size)
                {
                    memcpy(ref.data, node->data, node_size);
                }
                else
                {
                    ref.state = STATE_SIZE_ERROR;
                }

                ref.size = node_size;
                node->refs--;
                space_reclaimed = reclaim_nodes(true);
            }
        }

        /* set queue state */
        msgQ->state = ref.state;

        /* signal publishers */
        if(space_reclaimed)
        {
            msgQ->locknblock->signal(READY2POST, Cond::NOTIFY_ONE);
        }
    }
    msgQ->locknblock->unlock();

    /* return status */
    return ref.state;
}

/*----------------------------------------------------------------------------
 * reclaim_nodes
 *----------------------------------------------------------------------------*/
bool Subscriber::reclaim_nodes(bool delete_data)
{
    bool space_reclaimed = false;

    /* handle subscribers of opportunity */
    if(msgQ->soo_count > 0 && isFull())
    {
        for(int i = 0; i < msgQ->max_subscribers; i++)
        {
            /* only drop data on subscriber of opportunity that is holding up queue */
            if( (msgQ->subscriber_type[i] == SUBSCRIBER_OF_OPPORTUNITY) &&
                (msgQ->curr_nodes[i] == msgQ->front) )
            {
                /* dereference nodes until ref transition...
                 * it is possible that a subscriber of confidence is also
                 * holding up the queue, and the below code will still
                 * dereference subscriber of opportunity nodes, up to the
                 * entire contents of the queue; this is intentional as
                 * a subscriber of opportunity has no guarantee of data
                 * being delivered if it has gotten behind to the point
                 * where it is pointing to the last node in the queue -
                 * in other words, if a subscriber of opportunity wants
                 * to guarantee that data isn't dropped being delivered
                 * to it, then it must make sure it never gets behind to
                 * the point where it is pointing to the last node in the
                 * queue */
                const int starting_ref_count = msgQ->curr_nodes[i]->refs;
                while(msgQ->curr_nodes[i] && (msgQ->curr_nodes[i]->refs == starting_ref_count))
                {
                    msgQ->curr_nodes[i]->refs--;
                    msgQ->curr_nodes[i] = msgQ->curr_nodes[i]->next;
                }
            }
        }
    }

    /* attempt to reclaim space */
    while((msgQ->front != NULL) && (msgQ->front->refs <= 0))
    {
        queue_node_t* node = msgQ->front;

        /* remove from queue */
        if(msgQ->front == msgQ->back)   msgQ->front = msgQ->back = NULL;
        else                            msgQ->front = msgQ->front->next;

        /* deallocate memory block and free data */
        msgQ->free_block_stack[msgQ->free_blocks++] = reinterpret_cast<char*>(node);
        if(msgQ->free_blocks == MAX_FREE_STACK_SIZE)
        {
            for(int i = msgQ->free_blocks - 1; i >= 0; i--)
            {
                queue_node_t* temp = reinterpret_cast<queue_node_t*>(msgQ->free_block_stack[i]);
                if((temp->mask & MSGQ_COPYQ_MASK) == 0)
                {
                    if(msgQ->free_func && delete_data)  (*msgQ->free_func)(temp->data, NULL);
                    else                                assert(msgQ->free_func);
                }
                delete [] msgQ->free_block_stack[i];
            }
            msgQ->free_blocks = 0;
        }

        /* decrement queue length */
        msgQ->len--;
        space_reclaimed = true;
    }

    return space_reclaimed;
}

/*----------------------------------------------------------------------------
 * init_subscriber
 *----------------------------------------------------------------------------*/
void Subscriber::init_subscriber(subscriber_type_t type)
{
    msgQ->locknblock->lock();
    {
        /* Check Need to Resize */
        const int old_max_subscribers = msgQ->max_subscribers;
        if(msgQ->subscriptions >= msgQ->max_subscribers)
        {
            msgQ->max_subscribers *= 2;
        }

        /* Perform Resize */
        if(msgQ->max_subscribers != old_max_subscribers)
        {
            /* Allocate Room for Larger Number of Subscriptions */
            subscriber_type_t*  new_subscribed = new subscriber_type_t [msgQ->max_subscribers];
            queue_node_t**      new_curr_nodes = new queue_node_t* [msgQ->max_subscribers];

            /* Zero Out Upper Half of Arrays */
            for(int i = old_max_subscribers; i < msgQ->max_subscribers; i++)
            {
                new_subscribed[i] = UNSUBSCRIBED;
                new_curr_nodes[i] = NULL;
            }

            /* Copy In Current Values */
            for(int i = 0; i < old_max_subscribers; i++)
            {
                new_subscribed[i] = msgQ->subscriber_type[i];
                new_curr_nodes[i] = msgQ->curr_nodes[i];
            }

            /* Save Off Old Arrays */
            subscriber_type_t* old_subscribed   = msgQ->subscriber_type;
            queue_node_t** old_curr_nodes       = msgQ->curr_nodes;

            /* Swap Pointers */
            msgQ->subscriber_type = new_subscribed;
            msgQ->curr_nodes = new_curr_nodes;

            /* Delete Old Arrays */
            delete [] old_subscribed;
            delete [] old_curr_nodes;
        }

        /* Add Subscription */
        for(int i = 0; i < msgQ->max_subscribers; i++)
        {
            if(msgQ->subscriber_type[i] == UNSUBSCRIBED)  // NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)
            {
                id = i;
                msgQ->subscriber_type[id] = type;
                if(type == SUBSCRIBER_OF_OPPORTUNITY) msgQ->soo_count++;
                msgQ->subscriptions++;
                break;
            }
        }
    }
    msgQ->locknblock->unlock();
}
