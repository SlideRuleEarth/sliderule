--[[
Copyright (c) 2026, University of Washington
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the University of Washington nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
“AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--]]

--
-- Globals
--

-- Rate Limiting Parameters
RATELIMIT_WEEKLY_COUNT = {ip=50000, user=100000} -- maximum number of requests that can be made in one week
RATELIMIT_BACKOFF_COUNT = {ip=10, user=100} -- after the max count is reached, this is the number of new requests that can be made before the next backoff
RATELIMIT_BLOCK_PERIOD = {ip=600, user=600} -- user will not be able to make requests for this number of seconds

-- Rate Limiting Data
WeekOfLastRequest = {} -- "<ip_address or username>": <week number of last request>
RequestCount = {} -- "<ip_address or username>": <number of accumulated requests in the week>
BlockTime = {} -- "<ip_address or username>": <os time until which ip address will be blocked>

-- Analysis Data
EndpointMetric = {} -- "<endpoint>": <number of requests>
SuccessfulRequestCount = 0
FailedRequestCount = 0

-- Utility: table_size
local function table_size(t)
    local n = 0
    for _ in next, t do
        n = n + 1
    end
    return n
end

--
-- Action: ratelimit
--
local function ratelimit(txn)
    local client_ip = txn.sf:src()
    local username = txn:get_var("txn.sub")
    local mode = username and #username > 0 and "user" or "ip"
    local originator = username and #username > 0 and username or client_ip

    -- check blocked status
    local now = os.time()
    if now <= (BlockTime[originator] or 0) then
        txn:set_var("txn.block_this_ip", true)
        return
    end

    -- clear any blocks
    BlockTime[originator] = nil

    -- track requests per ip
    local this_week_number = os.date("%V")
    local last_week_number = WeekOfLastRequest[originator]
    if this_week_number == last_week_number then
        RequestCount[originator] = (RequestCount[originator] or 0) + 1
    else
        RequestCount[originator] = 1
    end
    WeekOfLastRequest[originator] = this_week_number

    -- check ip rates
    if RequestCount[originator] >= RATELIMIT_WEEKLY_COUNT[mode] then
        RequestCount[originator] = RequestCount[originator] - RATELIMIT_BACKOFF_COUNT[mode]
        BlockTime[originator] = os.time() + RATELIMIT_BLOCK_PERIOD[mode]
    end

end

--
-- Action: metric
--
local function metric(txn)
    local path = txn:get_var("txn.request_path")
    local status = txn.sf:status()
    -- accumulate requests
    if tonumber(status) >= 200 and tonumber(status) < 300 then
        EndpointMetric[path] = (EndpointMetric[path] or 0) + 1
        SuccessfulRequestCount = SuccessfulRequestCount + 1
    else -- count errors
        FailedRequestCount = FailedRequestCount + 1
    end
end

--
-- API: /stats/prometheus
--
--  Provide metrics to prometheus scraper
--
local function api_prometheus(applet)

    -- create endpoint count metrics
    local endpoint_metric = ""
    for endpoint,count in pairs(EndpointMetric) do
        endpoint_metric = endpoint_metric .. string.format([[

# TYPE %s_count counter
%s_count %d
]], endpoint, endpoint, count)
    end

    -- count originators
    local unique_originators = table_size(RequestCount)
    local blocked_originators = table_size(BlockTime)

    -- build full response
    local response = string.format([[
# TYPE successful_request_count counter
successful_request_count %d

# TYPE failed_request_count counter
failed_request_count %d

# TYPE unique_originators counter
unique_originators %d

# TYPE blocked_originators counter
blocked_originators %d
%s
]], SuccessfulRequestCount,
    FailedRequestCount,
    unique_originators,
    blocked_originators,
    endpoint_metric)

    -- send response
    applet:set_status(200)
    applet:add_header("content-length", string.len(response))
    applet:add_header("content-type", "text/plain")
    applet:start_response()
    applet:send(response)
end

--
-- Register with HAProxy
--
core.register_action("analyzer_ratelimit", { "http-req" }, ratelimit)
core.register_action("analyzer_metric", { "http-res" }, metric)
core.register_service("analyzer_prometheus", "http", api_prometheus)
