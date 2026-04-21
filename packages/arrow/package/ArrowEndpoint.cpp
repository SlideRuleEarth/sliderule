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
#include "OutputLib.h"
#include "SystemConfig.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * defaultHandler
 *----------------------------------------------------------------------------*/
void ArrowEndpoint::defaultHandler (Request* request, LuaEngine* engine, content_t selected_output, const char* arguments)
{
    assert(selected_output == EndpointObject::ARROW); (void)selected_output;

    /* Start Response Thread */
    const Thread rqst_pid(requestThread, request); // will join before exiting this function

    /* Get Lua State */
    lua_State* L = engine->getLuaState();

     /* Create Publisher to Arrow Response Queue */
    Publisher* rspq = new Publisher(FString("%s-arrow", request->id).c_str());

    /* Supply Global Variables to Script */
    request->setLuaTable(engine->getLuaState(), request->id, rspq->getName(), arguments);

    /* Get Main Function */
    lua_getfield(L, -1, ENDPOINT_MAIN);
    if(!lua_isfunction(L, -1))
    {
        const FString error_msg("Did not find function <%s> to call in %s", ENDPOINT_MAIN, request->resource);
        sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
    }

    /* Execute Main Function */
    const int lua_status = lua_pcall(L, 0, LUA_MULTRET, 0); // function is popped from stack

    /* Log Status */
    if(lua_status != LUA_OK)
    {
        mlog(CRITICAL, "Failed to execute endpoint %s: %d", request->resource, lua_status);
    }
}

/*----------------------------------------------------------------------------
 * responseThread
 *----------------------------------------------------------------------------*/
void* ArrowEndpoint::responseThread (void* parm)
{
    Request* request = static_cast<Request*>(parm);

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, request->trace_id, "arrow_endpoint_response", "{\"id\":\"%s\"}", request->id);

    /* Create Subscriber to Arrow Response Queue */
    const FString arrow_rspq("%s-arrow", request->id); // well known, must match requestThread
    Subscriber inq(arrow_rspq.c_str());

    /* Create Publisher */
    Publisher rspq(request->id);

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
                    if(StringLib::match(record.getRecordType(), OutputLib::dataRecType))
                    {
                        const OutputLib::output_file_data_t* file_data = reinterpret_cast<OutputLib::output_file_data_t*>(record.getRecordData());
                        const long data_size = record.getAllocatedDataSize() - offsetof(OutputLib::output_file_data_t, data);

                        /* Check Size */
                        if(data_size <= bytes_to_send)
                        {
                            /* Send Header */
                            if(!hdr_sent)
                            {
                                hdr_sent = true;
                                sendHeader(OK, content2str(ARROW), &rspq, NULL, true);
                            }

                            /* Post Arrow Bytes */
                            const int rc = rspq.postCopy(file_data->data, data_size, SystemConfig::settings().publishTimeoutMs.value);
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
                                hdr_sent = true;
                                sendHeader(Internal_Server_Error, content2str(TEXT), &rspq, "Corrupted transfer");
                            }

                            /* Mark Failure */
                            mlog(ERROR, "Corrupted transfer detected on <%s>, received %ld bytes when only %ld bytes left to send", inq.getName(), data_size, bytes_to_send);
                            complete = true;
                        }
                    }
                    /* Arrow Meta Record */
                    else if(StringLib::match(record.getRecordType(), OutputLib::metaRecType))
                    {
                        /* Save Off Bytes to Send */
                        const OutputLib::output_file_meta_t* file_meta = reinterpret_cast<OutputLib::output_file_meta_t*>(record.getRecordData());
                        bytes_to_send = file_meta->size;
                    }
                }
                catch (const RunTimeException& e)
                {
                    /* Send Header */
                    if(!hdr_sent)
                    {
                        hdr_sent = true;
                        sendHeader(Internal_Server_Error, content2str(TEXT), &rspq, "Invalid record");
                    }

                    /* Mark Failure */
                    mlog(ERROR, "Invalid record of size %d received on <%s>", ref.size, inq.getName());
                    complete = true;
                }
            }
            /* Handle Terminator */
            else if(ref.size == 0)
            {
                /* Send Header */
                if(!hdr_sent)
                {
                    hdr_sent = true;
                    sendHeader(Service_Unavailable, content2str(TEXT), &rspq, "Failed execution");
                }

                /* Mark Failure - `complete` should have been set above when all bytes received */
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
                hdr_sent = true;
                sendHeader(Internal_Server_Error, content2str(TEXT), &rspq, "Queing failure");
            }

            /* Mark Failure */
            mlog(CRITICAL, "Failed to receive data on input queue <%s>: %d\n", inq.getName(), recv_status);
            complete = true;
        }
    }

    /* (If Not Sent) Send Header */
    if(!hdr_sent)
    {
        sendHeader(Internal_Server_Error, content2str(TEXT), &rspq, "Missing data");
    }

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
