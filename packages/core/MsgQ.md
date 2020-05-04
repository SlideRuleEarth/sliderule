## Class: MsgQ

**Path**:       `packages/core`
**Modules**:    `MsgQ.cpp, MsgQ.h`
**Anchors**:    [Description](#description) | [Application Programming Interface](#application-programming-interface) | [Examples](#examples)

### Description

The **_MsgQ_** class implements a thread level message queue with two helper classes:  Publisher and Subscriber. The **_Publisher_** class provides an interface to posting messages to a queue.  The **_Subscriber_** class provides an interface to receiving messages from a queue.

This message queue implementation has the following features and characteristics:

* Message queues are visible only within the process they are created, hence they are thread level queues that provide a queue interface to a global process memory space.

* Each message queue supports multiple **_thread safe_** publishers and subscribers.

* Each message queue supports the specification of a **_maximum depth and object size constraint_** for bounding system resource usage.

* Message queues are **_persistent_**.  The first publisher or subscriber to be created with a specified message queue name creates the underlying message queue data structure and all subsequent publishers and subscribers that are created with the same message queue name attach themselves to the existing message queue data structure.  The characteristics of the message queue are set by the first call to create it  and subsequent publishers and subscribers do not override the characteristics even if different characteristics are specified.  Once the last publisher or subscriber attached to a message queue has been deleted, the underlying message queue is also deleted and its memory resources freed.

* The message queue interface provides for an **_unnamed_** message queue which acts like a private class data member.  The only way an unnamed message queue can be attached to is by providing a reference to an existing publisher or subscriber.

* Both publisher and subscriber interfaces support **_blocking and non-blocking_** (indefinite and timeout) operations.

* Both publisher and subscriber interfaces support **_passing data by copy or by reference_**.  When multiple subscribers are present, retrieving data by reference allows for efficient use of read-only object data from multiple threads within one process.

* Queued objects are **_reference counted_** and automatically deallocated.  Each subscriber as a pointer to its current object.  As the subscriber receives data from the queue, the reference count for that object is decremented (either immediately if the data is copied out, or later if the data is returned via a reference pointer).  Once an object is no longer referenced by any subscriber, the queued object and associated queue resources are marked for garbage collection.

* Each subscriber sees its own view of the message queue starting from the time it is attached.  Any objects that were posted to the message queue prior to a subscriber subscribing are not seen by the attaching subscriber; the subscriber only sees objects that were posted after its subscription occurred. As a result, a message queue with only publishers attached to it, and no subscribers, will drop any data that is posted to it.

* Options are provided for managing queued objects completely outside of the message queues by passing only references to them.  In those cases, the automatic deallocation only frees queue resources, and does not free the object resources.  This allows stringing multiple message queues together without copying the data multiple times.

* **_Lazy resource deallocation_** is implemented to optimize Linux kernel memory management.  Objects are collected and deallocated in large blocks which drastically improves throughput when system memory utilization is high.

* Two types of subscribers are supported: subscribers of confidence, and subscribers of opportunity.  A **_Subscriber of Confidence_** guarantees that any object added to the message queue after the subscriber attaches to the queue will be delivered and delivered in order.  A **_Subscriber of Opportunity_** does not guarantee delivery of any object, but the objects that are delivered are still delivered in order.  Subscribers of opportunity cannot prevent a publisher from posting data to the queue in the presence of subscribers of confidence subscribing to the same queue.  In other words, if a message queue has subscribers of opportunity, if the queue fills up and would therefore prevent a publisher from posting data to the queue, the message queue will automatically deference nodes from subscribers of opportunity to make space for the data to be posted.

Conceptually, this message queue system can be viewed as a long chain of objects linked together like the links of a metal chain.  Each link is a data node that contains the object being queued as well as pointers to the link ahead of it and behind it.  At the front of the chain, publishers continually place additional links.  When a subscriber _attaches_ to the chain, it gets a pointer to the very front of the chain where new links are added.  As a subscriber makes calls to receive the data, its pointer moves up to the next link in the chain.  If a subscriber falls behind and is not making calls to receive the data fast enough, then its pointer will fall further and further back from the very front of the chain where publishers are adding new links.  If it falls far enough behind, then the chain will have reached its maximum length and the message queue will prevent new links from being added until the subscribers that are holding things up make more calls to read the data.

When a subscriber moves its pointer up the chain, it _dereferences_ the link it just read.  The message queue system is constantly looking at all of the links in the chain from the tail end, looking for links that have no more references (i.e. all attached subscribers are ahead of it).  When it sees a link like that it saves it off in a garbage collection list.  Once that list builds up to a certain size, the message queue does a block deallocation of the links in the list, freeing them from the chain all at once.  It should therefore be noted that the total amount of memory needed by the message queue system is the maximum size of the chain, plus the size of the garbage collection list. But from the application's stand point all of this complexity is hidden.  The application only sees a message queue in which publishers create new links in the chain, and subscribers attach to the those links and walk at their own pace up the chain reading the links as they go.

### Application Programming Interface

#### Constants

`SUBSCRIBER_OF_OPPORTUNITY` : specifies that a subscriber is a subscriber of opportunity

`SUBSCRIBER_OF_CONFIDENCE` : specifies that a subscriber is a subscriber of confidence

`CFG_DEPTH_INFINITY` : no limit on the number of objects that can be queued on the message queue

`CFG_DEPTH_STANDARD` : the message queue class maintains a global depth configuration that can be globally changed; specifying this option uses the globally specified value as it is set when the call is made.  In other words, changing the global depth does not go back and modify existing queue depths, it only affects new queues that are created.

`CFG_SIZE_INFINITY` : no limit on the size of the object that is queued on the message queue

`IO_CHECK` : do not block, but just check the state of the queue, attempt the operation, and return right away

`IO_PEND` : block forever until the requested operation succeeds or a fatal error is encountered

`STATE_OKAY` : the message queue is healthy and can have data posted to it and read from it

`STATE_FULL` : the message queue is full and cannot accept any more data (unless a subscriber of opportunity is making it full), until data in the queue is read

`STATE_SIZE_ERROR` : an object attempted to be queued that was larger than the maximum allowed size for that queue

`STATE_TIMEOUT` : a blocking operation timed out before it succeeded

`STATE_ERROR` : an unspecified error occurred when attempting an operation on the message queue

`STATE_EMPTY` : the message queue is empty and cannot have any further data read from it until new data is posted

`STATE_NO_SUBSCRIBERS` : an object attempted to be queued on a queue that had no subscribers and the publisher was posting the data by reference meaning it is then the publisher's responsibility to free the resources of that object.

`MAX_POSTED_STR` : the maximum size of the formatted string that is posted to a queue via one of the formatted string post calls


#### Types

`Subscriber::msgRef_t` : Defines a reference to a queued object.  The object's contents are returned in the **_data_** pointer.  The size of the object in bytes is returned in the **_size_** field.  The state of the message queue when the object was read out of the queue is returned in the **_state_** field.  A handle to the internal queue node that stored the returned object is returned in the **_\_handle_** field.  The _handle field is not intended for use by the calling application except in extreme cases when complicated queueing patterns are needed to optimize memory usage.

`MsgQ::queueDisplay_t` : Defines a description structure for a queue that can be used to display information about the queue.  The **_name_** provides the name of the queue.  The **_len_** provides the number of objects that are queued.  The **_state_** is the current state of the queue.  The **_subscriptions_** is the number of subscribers that are attached to the queue.

`MsgQ::free_func_t` : Defines a pointer to a function that frees the queued object.  All publishers that post by reference must also specify a free function that is used by the message queue to free the resources associated with the object once all references to the object have been dereferenced.  Publishers that post by copy do not need to specify such a function since the memory is allocated internally as an array of bytes.


#### Methods

| | | |
|---|---|---|
| [MsgQ](#msgQ)             | [Static Routines](#static-routines)       | [Get/Set](#get-set)           |
| [Publisher](#publisher)   | [Post Reference](#post-reference)         | [Post Copy](#post-copy)       |
| [Subscriber](#subscriber) | [Receive Reference](#receive-reference)   | [Receive Copy](#receive-copy) |


##### MsgQ

`MsgQ::MsgQ (const char* name, MsgQ::free_func_t free_func=NULL, int depth=CFG_DEPTH_STANDARD, int data_size=CFG_SIZE_INFINITY)` : Base constructor for message queues.  Except in rare cases when the characteristics of a message queue need to be established before any subsribers or publishers are created, this constructor should not be directly called.  Instead the Subscriber or Publisher constructors should be called which will in turn call this constructor with the appropriate settings.

* **name** - the name of the message queue, null if a private queue
* **free_func** - the function used to free queued objects after they have been fully dereferenced
* **depth** - the maximum number of objects that can be queued at one time in the message queue
* **data_size** - the maximum size of the object that can be queued in the message queue

`MsgQ::MsgQ (const MsgQ& existing_q, MsgQ::free_func_t free_func=NULL)` : Base constructor for message queues where the underlying queue already exists.  This supports private message queues created from the Publisher and Subscriber class constructors.

* **existing_q** - an existing message queue
* **free_func** - the function used to free queued objects after they have been fully dereferenced

`MsgQ::~MsgQ (void)` : Base destructor for all message queues.  Handles freeing resources associated with message queue once all subsribers and publishers have detached.

##### Static Routines

`static void MsgQ::initQ (void)` : Initializes the message queue library.  This function must be called at startup before any other calls are made to the message queue API, and after the call to initialize the OsApi library.

`static void MsgQ::deinitQ (void)` : Frees resources associated with the message queue library.  Typically this call would be made just prior to program exit, but prior to any call that uninitialized the OsApi.

`static bool MsgQ::existQ (const char* qname)` : Helper function that returns true if the named message queue exists.

`static int MsgQ::numQ (void)` : Helper function that returns the number of queues currently registered.

`static int MsgQ::listQ (queueDisplay_t* list, int list_size)` : Helper function that returns a populated list of message queue meta information.  See the queueDisplay_t type description above for more information.

`static void MsgQ::setStdQDepth (int depth)` : Sets the standard queue depth that is automatically used on any message queue created with the CFG_DEPTH_STANDARD option.  The purpose of this function is to provide the means to tailor the library for use on hardware with varying memory sizes.  It is sometimes necessary to maintain large queues of data - but on different size systems, what constitutes a large queue is different; therefore, with one call, all queues in the system that use the standard depth can have their depth fixed to an appropriate size.

##### Get/Set

`int MsgQ::getCount (void)` : Returns the number of objects currently queued in the message queue

`int MsgQ::getDepth (void)` : Returns the maximum number of objects that can be queued in the message queue

`const char* MsgQ::getName (void)` : Returns the name of the message queue as a pointer reference to the internal stored name.  No deallocation of the return value is necessary.  The return value should be treated as read-only.  Null indicates a private queue.

`int MsgQ::getState (void)` : Returns the current state of the message queue.  See the STATE_* definitions above.

`bool MsgQ::isFull (void)` : Returns true if the message queue is currently full and cannot enqueue any additional objects.

`bool Subscriber::isEmpty (void)` : Returns true if the message queue's subscription is currently empty - i.e. no further objects can be read from the subsription to the message queue until new objects are enqueued.

`void* Subscriber::getData (long _handle)` : Returns a pointer to the queued object specified by the _handle.  This function is to be rarely used, and only in extreme cases where the memory utilization of a large sequence of queues must be carefully and explicitly managed.  For example, it is sometimes the case that a large set of data needs to be maintained in a queue (e.g. long integration times for image sets), but while that data is queued, a different re-ordering or indexing of the data needs to be performed.  It can be helpful to let the management of the memory be left to the queue, and re-index the data via the handles.  This function enables that kind of scenario by providing a means at getting to the data via only the handle.

##### Publisher

`Publisher::Publisher (const char* name, MsgQ::free_func_t free_func=NULL, int depth=CFG_DEPTH_STANDARD, int data_size=CFG_SIZE_INFINITY)` : Constructor for the Publisher class.  This will create a publishing attachment to the specified message queue.  If the queue does not exist, it will create the queue with the parameters specified.  If the queue does exist, the parameters specified will be ignored and the attachment will proceed.

* **name** - the name of the message queue
* **free_func** - the function used to free queued objects after they have been fully dereferenced
* **depth** - the maximum number of objects that can be queued at one time in the message queue
* **data_size** - the maximum size of the object that can be queued in the message queue

`Publisher::Publisher (const MsgQ& existing_q, MsgQ::free_func_t free_func=NULL)` : Constructor for Publisher class where the underlying queue already exists.  This supports private message queues created by other Publishers or Subscribers.

* **existing_q** - an existing message queue
* **free_func** - the function used to free queued objects after they have been fully dereferenced

`Publisher::~Publisher (void)` : Destructor for the Publisher class.  Detaches the publisher from the message queue, frees resources associated with the publisher only, and then calls the MsgQ destructor.

##### Post Reference

`int Publisher::postRef (void* data, int size, int timeout=IO_CHECK)` : Posts the data to the message queue as a reference.  No data is copied, instead only the data pointer is enqueued in the message queue, and the size of the data is stored for later retrieval on receives.  To use this function, the associated publisher must have a free function provided.  It is the responsibility of the Publisher to provide a free function since it is the originator of the data.  It is the responsibility of the Subscriber to call the free function (via a dereference), since it knows when it is safe to free the data.

* **data** - a pointer to the data being queued
* **size** - the size of the data being queued, used in the post only to check against the maximum allowed data size.   The size is later used in receive calls:  returned in the receive by reference, or used in the receive by copy to copy the data contents out.
* **timeout** - the minimal amount of time, specified in milliseconds, to block the operation in order to wait for it to succeed.  If IO_CHECK is supplied, then the operation will be non-blocking and immediately return.  If IO_PEND is supplied, then the opperation will block forever until the operation succeeds.

_Returns_ - the function will return the state of the message queue at the time of the post operation's attempt.  See the STATE_* definitions above for details.  If the operation succeeded, STATE_OKAY will be returned.  Otherwise, one of the error codes will be returned indicating why the operation failed.  This is different than the postCopy which returns the number of bytes copied on success.  Given that the data is a pointer, there is no concept of bytes being queued, only that the post succeeded or failed for a given reason.

##### Post Copy

`int Publisher::postCopy (const void* data, int size, int timeout=IO_CHECK)` : Posts a copy of the data to the message queue.  The message queue internally allocates sufficient space for the contents of the data, and copies the contents onto the queue, leaving the original data untouched.  When this function returns, the memory pointed to by the data parameter can be immediately freed or reused.

* **data** - a pointer to the data being queued
* **size** - the size of the data being queued, used to copy the data into the queue and to check against the maximum allowed data size.
* **timeout** - the minimal amount of time, specified in milliseconds, to block the operation in order to wait for it to succeed.  If IO_CHECK is supplied, then the operation will be non-blocking and immediately return.  If IO_PEND is supplied, then the opperation will block forever until the operation succeeds.

_Returns_ - the function will return the size of the data queued on success, or an error code on failure.  See the STATE_* definitions above for details.

`int Publisher::postCopy (const void* header_data, int header_size, const void* payload_data, int payload_size, int timeout=IO_CHECK)` :  This function behaves exactly like the above postCopy function except that it takes two sets of pointers and sizes to the data instead of one.  The purpose of this function is to support the often employed paradigm where messages are encapsulated inside of a highler level protocol before being queued.  For instance, if an image is being transfered over a SpaceWire network using RMAP (remote memory access protocol),  the image data would need to be broken up into smaller chucks, have an RMAP header prepended to it, and then sent.  Prepending the header requires that a new contiguous memory block be obtained of sufficient size to contain both the RMAP header and the image payload.  Providing this API allows the allocation of that contiquous memory block and the subsequent copying of the data to only occur once - when the data is enqueued.  Note that the data size check is performed against the sum total of the header and payload sizes.

* **header_data** - a pointer to the header of the message being queued
* **header_size** - the size of the header
* **payload_data** - a pointer to the payload of the message being queued
* **payload_size** - the size of the payload
* **timeout** - the minimal amount of time, specified in milliseconds, to block the operation in order to wait for it to succeed.  If IO_CHECK is supplied, then the operation will be non-blocking and immediately return.  If IO_PEND is supplied, then the opperation will block forever until the operation succeeds.

_Returns_ - the function will return the size of the total amount data queued on success, or an error code on failure.  See the STATE_* definitions above for details.

`int Publisher::postString (const char* format_string, ...)` : Creates a formated ASCII string of data as specified by the format_string.  Underneath, this function calls one of the above postCopy functions, and is provided here as a wrapper function because of how often such functionality is needed when working with text based messaging.  The ASCII string that is ultimately enqueued is always null terminated, and the size of the object recorded in the message queue includes the null termination.

* **format_string** - a string format specification analogous to the c printf specification.

_Returns_ - the function will return the size of the string (including null terminator) queued on success, or an error code on failure.  See the STATE_* definitions above for details.

##### Subscriber

`Subscriber::Subscriber (const char* name, subscriber_type_t type=SUBSCRIBER_OF_CONFIDENCE, int depth=CFG_DEPTH_STANDARD, int data_size=CFG_SIZE_INFINITY)` : Constructor for the Subscriber class.  This will create a subscribing attachment to the specified message queue.  If the queue does not exist, it will create the queue with the parameters specified.  If the queue does exist, the parameters specified will be ignored and the attachment will proceed.  Only data that is posted AFTER a subsriber has attached will be made available to that subscriber.  In other words, any data that currently exists on the message queue when a subsriber attaches will not be visible or ever returned to the subsriber.

* **name** - the name of the message queue
* **type** - the type of subscription, see SUBSCRIBER_OF_CONFIDENCE and SUBSCRIBER_OF_OPPORTUNITY definitions above.
* **depth** - the maximum number of objects that can be queued at one time in the message queue
* **data_size** - the maximum size of the object that can be queued in the message queue

`Subscriber::Subscriber (const MsgQ& existing_q, subscriber_type_t type=SUBSCRIBER_OF_CONFIDENCE)` : Constructor for Subscriber class where the underlying queue already exists.  This supports private message queues created by other Publishers or Subscribers.

* **existing_q** - an existing message queue
* **type** - the type of subscription, see SUBSCRIBER_OF_CONFIDENCE and SUBSCRIBER_OF_OPPORTUNITY definitions above.

`Subscriber::~Subscriber (void)` :  Destructor for the Subscriber class.  Detaches the subscriber from the message queue, dereferences all queued objects remaining on the message queue for this subsriber only, frees resources associated with the subscriber, and then calls the MsgQ destructor.

##### Receive Reference

`int Subscriber::receiveRef (msgRef_t& ref, int timeout)` :  Receives a reference to the oldest subscribed-to object on the message queue that has yet to be received by the subscriber (i.e. the next object on the queue).  No data is copied, instead only the data pointer and data size is placed in the returned reference structure. If this function is called, a subsequent call to _dereference_ must be made once the data is no longer needed.

* **ref** - a reference to a msgRef_t structure.  The contents of the structure are populated by the function.  See its definition above in the [Types](#types) section. On both success and error the structure is populated, but only on success can the contents of the structure be trusted.
* **timeout** - the minimal amount of time, specified in milliseconds, to block the operation in order to wait for it to succeed.  If IO_CHECK is supplied, then the operation will be non-blocking and immediately return.  If IO_PEND is supplied, then the opperation will block forever until the operation succeeds.

_Returns_ - the function will return the state of the message queue at the time of the receive operation's attempt.  See the STATE_* definitions above for details.  If the operation succeeded, STATE_OKAY will be returned.  Otherwise, one of the error codes will be returned indicated why the operation failed.  This is different than the receiveCopy which returns the number of bytes copied on success.  Given that the data is a reference, there is no concept of bytes being dequeued, only that the receive succeeded or failed for a given reason.

`bool Subscriber::dereference (msgRef_t& ref, bool with_delete=true)` : Dereferences the queued object identified by the _ref_ structure.  When receiving a message by reference, this call MUST be made to free the resources associated with the message.

* **ref** - a reference to a msgRef_t structure.  The contents of the structure will be read by the function and used to properly dereference the associated queue node.  Limited error checking can be performed on this parameter; therefore, passing an invalid ref structure will cause unknown (e.g. but almost certainly) bad behaviors.
* **with_delete** - if true (which is the default), then the message queue resources and data associated with the queued object will be freed (by calling the associated free_func for the latter).  If false, then only the message queue resources associated with the queued object will be freed, and the queued object itself will be left alone.

_Returns_ - true on success, false on failure.  Currently, there is no condition which causes it to fail.

`void Subscriber::drain (bool with_delete=true)` : All messages in the message queue still waiting to be read by the subscriber are immediately derferenced as if the subscriber had read the entire queue.  The subscriber is moved up to the top of message queue and returned an empty view of the queue.

* **with_delete** - if true (which is the default), then the message queue resources and data associated with the queued objects will be freed (by calling the associated free_func for the latter).  If false, then only the message queue resources associated with the queued objects will be freed, and the queued object itself will be left alone.

##### Receive Copy

`int Subscriber::receiveCopy (void* data, int size, int timeout)` : Receives a copy of the oldest subscribed to object on the message queue that has yet to be received by the subscriber (i.e. the next object on the queue).  The data is copied, and the queued object is automatically dereferenced.

* **data** - a pointer to a buffer that the dequeued data will be copied into
* **size** - the size of the data buffer, and therefore the maximum size allowed for the dequeued data dequeued.
* **timeout** - the minimal amount of time, specified in milliseconds, to block the operation in order to wait for it to succeed.  If IO_CHECK is supplied, then the operation will be non-blocking and immediately return.  If IO_PEND is supplied, then the opperation will block forever until the operation succeeds.

_Returns_ - the function will return the size of the data object dequeued and copied into the buffer on success, or an error code on failure.  See the STATE_* definitions above for details.


### Examples

#### Example - Post By Reference
```cpp
    void my_free_func(void* obj, void* parm) { delete [] (unsigned char*) obj; }
    Publisher* myq = new Publisher("myq_name", my_free_func);
    unsigned char* buf = new unsigned char[MY_BUF_SIZE];
    int status = myq->postRef(buf, MY_BUF_SIZE, 1000);
    if(status <= 0) delete [] buf;
```

#### Example - Post By Copy
```cpp
    unsigned char buf[MY_BUF_SIZE];
    int bytes = myq->postCopy(buf, MY_BUF_SIZE, 1000);
```

#### Example - Receive By Reference
```cpp
    Subscriber::msgRef_t ref;
    int status = myq->receiveRef(ref, 1000);
    if(status > 0)
    {
        const char* mymsg = (const char*)ref.data;
        printf("My Message: %s\n", mymsg);
        myq->dereference(ref);
    }
```

#### Example - Receive By Copy
```cpp
    char buffer[MAX_MSG_SIZE];
    int bytes = myq->receiveCopy(buffer, MAX_MSG_SIZE, 1000);
    if(bytes > 0) printf("My Message: %s\n", buffer);
```

