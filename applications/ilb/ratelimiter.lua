--[[
Copyright (c) 2021, University of Washington
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

core.log(core.info, "Loading rate limiter...")

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

--
-- Action: ratelimit
--
local function ratelimit(txn)
    local client_ip = txn.sf:src()
    local username = txn:get_var("txn.sub")
    local mode = #username > 0 and "user" or "ip"
    local originator = #username > 0 and username or client_ip

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
-- Register with HAProxy
--
core.register_action("ratelimit", { "http-req" }, ratelimit)
