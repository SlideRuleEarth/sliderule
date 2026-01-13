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
RATELIMIT_WEEKLY_COUNT = 50000 -- maximum number of requests that can be made in one week
RATELIMIT_BACKOFF_COUNT = 10 -- after the max count is reached, this is the number of new requests that can be made before the next backoff
RATELIMIT_BLOCK_PERIOD = 10 * 60 -- user will not be able to make requests for this number of seconds

-- Rate Limiting Data
WeekOfLastRequest = {} -- "<ip_address>": <week number of last request>
RequestCount = {} -- "<ip_address>": <number of accumulated requests in the week>
BlockTime = {} -- "<ip_address>": <os time until which ip address will be blocked>

--
-- Action: ratelimit
--
local function ratelimit(txn)
    local client_ip = txn.sf:src()

    -- check blocked status
    local now = os.time()
    if now <= (BlockTime[client_ip] or 0) then
        txn:set_var("txn.block_this_ip", true)
        return
    end

    -- clear any blocks
    BlockTime[client_ip] = nil

    -- track requests per ip
    local this_week_number = os.date("%V")
    local last_week_number = WeekOfLastRequest[client_ip]
    if this_week_number == last_week_number then
        RequestCount[client_ip] = (RequestCount[client_ip] or 0) + 1
    else
        RequestCount[client_ip] = 1
    end
    WeekOfLastRequest[client_ip] = this_week_number

    -- check ip rates
    if RequestCount[client_ip] >= RATELIMIT_WEEKLY_COUNT then
        RequestCount[client_ip] = RequestCount[client_ip] - RATELIMIT_BACKOFF_COUNT
        BlockTime[client_ip] = os.time() + RATELIMIT_BLOCK_PERIOD
    end

end

--
-- Register with HAProxy
--
core.register_action("ratelimit", { "http-req" }, ratelimit)
