--
-- ENDPOINT:    /source/atl08p
--
-- PURPOSE:     fan out atl08 requests to multiple back-end servers and collect responses
--
-- INPUT:       rqst
--              {
--                  "resources":    ["<url of hdf5 file or object>", ...],
--                  "parms":        {<table of parameters>},
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      stream of responses
--

local json = require("json")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]
local timeout = parms["rqst-timeout"] or parms["timeout"] or icesat2.RQST_TIMEOUT
local node_timeout = parms["node-timeout"] or parms["timeout"] or icesat2.NODE_TIMEOUT

-- Initialize Timeouts --
local duration = 0
local interval = 10 < timeout and 10 or timeout -- seconds

-- Initialize Queue Management --
local rsps_from_nodes = rspq
local terminate_proxy_stream = false

-- Handle Output Options --
local output_dispatch = nil
if parms["output"] then
    local output_parms = arrow.parms(parms["output"])
    -- Parquet Writer --
    if output_parms:isparquet() then
        rsps_from_nodes = rspq .. "-parquet"
        terminate_proxy_stream = true
        local except_pub = core.publish(rspq)
        local parquet_builder = arrow.parquet(output_parms, rspq, "atl08rec.vegetation", rqstid, "lon", "lat")
        output_dispatch = core.dispatcher(rsps_from_nodes)
        output_dispatch:attach(parquet_builder, "atl08rec")
        output_dispatch:attach(except_pub, "exceptrec") -- exception records
        output_dispatch:attach(except_pub, "eventrec") -- event records
        output_dispatch:run()
    end
end

-- Proxy Request --
local proxy = netsvc.proxy("atl08", resources, json.encode(parms), node_timeout, rsps_from_nodes, terminate_proxy_stream)

-- Wait Until Proxy Completes --
while (userlog:numsubs() > 0) and not proxy:waiton(interval * 1000) do
    duration = duration + interval
    if timeout >= 0 and duration >= timeout then
        userlog:sendlog(core.ERROR, string.format("proxy request <%s> timed-out after %d seconds", rspq, duration))
        do return end
    end
end

-- Wait Until Dispatch Completion --
if terminate_proxy_stream then
    while (userlog:numsubs() > 0) and not output_dispatch:waiton(interval * 1000) do
        duration = duration + interval
        if timeout >= 0 and duration >= timeout then
            userlog:sendlog(core.ERROR, string.format("proxy dispatch <%s> timed-out after %d seconds", rspq, duration))
            do return end
        end
    end
end
