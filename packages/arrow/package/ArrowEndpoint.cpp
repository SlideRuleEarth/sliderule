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

#include "ArrowEndpoint.h"
#include "OsApi.h"
#include "TimeLib.h"
#include "ArrowLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArrowEndpoint::LUA_META_NAME = "ArrowEndpoint";
const struct luaL_Reg ArrowEndpoint::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void ArrowEndpoint::init (void)
{
}

/*----------------------------------------------------------------------------
 * luaCreate - endpoint([<normal memory threshold>], [<stream memory threshold>])
 *----------------------------------------------------------------------------*/
int ArrowEndpoint::luaCreate (lua_State* L)
{
    try
    {
        /* Create Lua Endpoint */
        return createLuaObject(L, new ArrowEndpoint(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowEndpoint::ArrowEndpoint(lua_State* L):
    EndpointObject(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArrowEndpoint::~ArrowEndpoint(void) = default;

/*----------------------------------------------------------------------------
 * requestThread
 *----------------------------------------------------------------------------*/
void* ArrowEndpoint::requestThread (void* parm)
{
    int status_code = RTE_STATUS;
    rqst_info_t* info = static_cast<rqst_info_t*>(parm);
    EndpointObject::Request* request = info->request;
    ArrowEndpoint* arrow_endpoint = dynamic_cast<ArrowEndpoint*>(info->endpoint);
    const double start = TimeLib::latchtime();

    /* Get Request Script */
    const char* script_pathname = LuaEngine::sanitize(request->resource);

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, request->trace_id, "arrow_endpoint_request", "{\"verb\":\"%s\", \"resource\":\"%s\"}", verb2str(request->verb), request->resource);

    /* Log Request */
    mlog(INFO, "%s %s: %s", verb2str(request->verb), request->resource, request->body);

    /* Create Publisher to Arrow Response Queue */
    const FString arrow_rspq("%s-arrow", request->id); // well known, must match responseThread
    Publisher* rspq = new Publisher(arrow_rspq.c_str());

    /* Check Authentication */
    const bool authorized = arrow_endpoint->authenticate(request);

    /* Handle Request */
    if(authorized)
    {
        /* Create Engine */
        LuaEngine* engine = new LuaEngine(script_pathname, reinterpret_cast<const char*>(request->body), trace_id, NULL, true);

        /* Supply Global Variables to Script */
        engine->setString(LUA_RESPONSE_QUEUE, rspq->getName());
        engine->setString(LUA_REQUEST_ID, request->id);

        /* Execute Engine
         *  The call to execute the script blocks on completion of the script. The lua state context
         *  is locked and cannot be accessed until the script completes */
        const bool status = engine->executeEngine(IO_PEND);

        /* Check Status
         *  At this point the http header has already been sent, so an error header cannot be sent;
         *  but we can log the error and report it as such in telemetry */
        if(!status)
        {
            mlog(CRITICAL, "Failed to execute script %s", script_pathname);
            status_code = RTE_FAILURE;
        }

        /* Clean Up */
        delete engine;
    }
    else
    {
        /* Respond with Unauthorized Error */
        sendHeader(rspq, Unauthorized, "Unauthorized");
        status_code = RTE_UNAUTHORIZED;
    }

    /* End Response */
    const int rc = rspq->postCopy("", 0, POST_TIMEOUT_MS);
    if(rc <= 0)
    {
        mlog(CRITICAL, "Failed to post terminator on %s: %d", rspq->getName(), rc);
        status_code = RTE_DID_NOT_COMPLETE;
    }

    /* Generate Metric for Endpoint */
    const EventLib::tlm_input_t tlm = {
        .code = status_code,
        .duration = static_cast<float>(TimeLib::latchtime() - start),
        .source_ip = request->getHdrSourceIp(),
        .endpoint = request->resource,
        .client = request->getHdrClient(),
        .account = request->getHdrAccount()
    };
    telemeter(INFO, tlm);

    /* Clean Up */
    delete rspq;
    delete [] script_pathname;
    delete request; // deleted here only
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * responseThread
 *----------------------------------------------------------------------------*/
void* ArrowEndpoint::responseThread (void* parm)
{
    rsps_info_t* info = static_cast<rsps_info_t*>(parm);

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, info->trace_id, "arrow_endpoint_response", "{\"id\":\"%s\"}", info->rqst_id);

    /* Create Subscriber to Arrow Response Queue */
    const FString arrow_rspq("%s-arrow", info->rqst_id); // well known, must match requestThread
    Subscriber inq(arrow_rspq.c_str());

    /* Create Publisher */
    Publisher rspq(info->rqst_id);

    /* Initialize State Variables */
    long bytes_to_send = 0;
    bool complete = false;
    bool hdr_sent = false;

    /* While Receiving Messages */
    while(!complete)
    {
        Subscriber::msgRef_t ref;
        const int recv_status = inq.receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            /* Handle Valid Records */
            if(ref.size > 0)
            {
                try
                {
                    const RecordInterface record(reinterpret_cast<unsigned char*>(ref.data), ref.size);

                    /* Arrow Data Record */
                    if(StringLib::match(record.getRecordType(), ArrowLib::dataRecType))
                    {
                        const ArrowLib::arrow_file_data_t* file_data = reinterpret_cast<ArrowLib::arrow_file_data_t*>(record.getRecordData());
                        const long data_size = record.getAllocatedDataSize() - offsetof(ArrowLib::arrow_file_data_t, data);

                        /* Check Size */
                        if(data_size <= bytes_to_send)
                        {
                            /* Send Header */
                            if(!hdr_sent)
                            {
                                hdr_sent = sendHeader(&rspq, OK);
                            }

                            /* Post Arrow Bytes */
                            const int rc = rspq.postCopy(file_data->data, data_size, POST_TIMEOUT_MS);
                            if(rc <= 0) mlog(CRITICAL, "Failed to post arrow data on <%s>: %d", rspq.getName(), rc);

                            /* Check if Complete */
                            bytes_to_send -= data_size;
                            if(bytes_to_send == 0)
                            {
                                complete = true;
                            }
                        }
                        else // invalid size
                        {
                            /* Send Header */
                            if(!hdr_sent)
                            {
                                hdr_sent = sendHeader(&rspq, Internal_Server_Error, "Corrupted transfer");
                            }

                            /* Mark Failure */
                            mlog(ERROR, "Corrupted transfer detected on <%s>, received %ld bytes when only %ld bytes left to send", inq.getName(), data_size, bytes_to_send);
                            complete = true;
                        }
                    }
                    /* Arrow Meta Record */
                    else if(StringLib::match(record.getRecordType(), ArrowLib::metaRecType))
                    {
                        /* Save Off Bytes to Send */
                        const ArrowLib::arrow_file_meta_t* file_meta = reinterpret_cast<ArrowLib::arrow_file_meta_t*>(record.getRecordData());
                        bytes_to_send = file_meta->size;
                    }
                }
                catch (const RunTimeException& e)
                {
                    /* Send Header */
                    if(!hdr_sent)
                    {
                        hdr_sent = sendHeader(&rspq, Internal_Server_Error, "Invalid record");
                    }

                    /* Mark Failure */
                    mlog(ERROR, "Invalid record of size %d received on <%s>", ref.size, inq.getName());
                    complete = true;
                }
            }

            /* Handle Terminator */
            if(ref.size == 0)
            {
                /* Send Header */
                if(!hdr_sent)
                {
                    hdr_sent = sendHeader(&rspq, Service_Unavailable, "Failed execution");
                }

                /* Mark Failure */
                mlog(CRITICAL, "Unexpectedly received terminator on <%s>", inq.getName());
                complete = true;
            }

            /* Always Dereference */
            inq.dereference(ref);
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            /* Send Header */
            if(!hdr_sent)
            {
                hdr_sent = sendHeader(&rspq, Internal_Server_Error, "Queing failure");
            }

            /* Mark Failure */
            mlog(CRITICAL, "Failed to receive data on input queue <%s>: %d\n", inq.getName(), recv_status);
            complete = true;
        }
    }

    /* (If Not Sent) Send Header */
    if(!hdr_sent)
    {
        sendHeader(&rspq, Internal_Server_Error, "Missing data");
    }

    /* Post Terminator */
    const int rc = rspq.postCopy("", 0, POST_TIMEOUT_MS);
    if(rc <= 0) mlog(CRITICAL, "Failed to post terminator on <%s>: %d", rspq.getName(), rc);

    /* Clean Up */
    delete [] info->rqst_id;
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * sendHeader
 *----------------------------------------------------------------------------*/
bool ArrowEndpoint::sendHeader (Publisher* outq, code_t http_code, const char* error_msg)
{
    bool hdr_sent = false;
    char header[MAX_HDR_SIZE];

    const int header_length = buildheader(header, http_code, "application/octet-stream", 0, "chunked", serverHead.c_str());
    const int rc = outq->postCopy(header, header_length, POST_TIMEOUT_MS);
    if(rc > 0)  hdr_sent = true;
    else        mlog(CRITICAL, "Failed to post header on <%s>: %d", outq->getName(), rc);

    if(http_code != OK)
    {
        const int rc2 = outq->postCopy(error_msg, StringLib::size(error_msg), POST_TIMEOUT_MS);
        if(rc2 <= 0) mlog(CRITICAL, "Failed to post error message on <%s>: %d", outq->getName(), rc2);
    }

    return hdr_sent;
}

/*----------------------------------------------------------------------------
 * handleRequest - returns true if streaming (chunked) response
 *----------------------------------------------------------------------------*/
bool ArrowEndpoint::handleRequest (Request* request)
{
    /* Start Response Thread */
    rsps_info_t* response_info = new rsps_info_t;
    response_info->trace_id = request->trace_id;
    response_info->rqst_id = StringLib::duplicate(request->id);
    const Thread rsps_pid(responseThread, response_info, false);

    /* Start Response Thread */
    rqst_info_t* request_info = new rqst_info_t;
    request_info->endpoint = this;
    request_info->request = request;
    const Thread rqst_pid(requestThread, request_info, false);

    /* Return Response Type (only streaming supported) */
    return true;
}
