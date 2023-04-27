--
-- EXTENSION:   proxy.lua
--
-- PURPOSE:     fan out requests to multiple back-end servers and collect responses
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

local function proxy(resources, parms, endpoint, rec, batch, lon, lat)
    -- Create User Status --
    local userlog = msg.publish(rspq)

    -- Request Parameters --
    local timeout = parms["rqst-timeout"] or parms["timeout"] or netsvc.RQST_TIMEOUT
    local node_timeout = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT

    -- Initialize Queue Management --
    local rsps_from_nodes = rspq
    local terminate_proxy_stream = false

    -- Handle Output Options --
    local parquet_builder = nil
    if parms[arrow.PARMS] then
        local output_parms = arrow.parms(parms[arrow.PARMS])
        -- Parquet Writer --
        if output_parms:isparquet() then
            rsps_from_nodes = rspq .. "-parquet"
            terminate_proxy_stream = true
            parquet_builder = arrow.parquet(output_parms, rspq, rsps_from_nodes, rec, batch, rqstid, lon, lat, "time")
        end
    end

    -- Proxy Request --
    local proxy = netsvc.proxy(endpoint, resources, json.encode(parms), node_timeout, rsps_from_nodes, terminate_proxy_stream)

    -- Wait Until Proxy Completes --
    local duration = 0
    local interval = 10 < timeout and 10 or timeout -- seconds
    while (userlog:numsubs() > 0) and not proxy:waiton(interval * 1000) do
        duration = duration + interval
        if timeout >= 0 and duration >= timeout then
            userlog:sendlog(core.ERROR, string.format("proxy request <%s> timed-out after %d seconds", rspq, duration))
            do return end
        end
    end

    -- Wait Until Dispatch Completion --
    if terminate_proxy_stream then
        while (userlog:numsubs() > 0) and not parquet_builder:waiton(interval * 1000) do
            duration = duration + interval
            if timeout >= 0 and duration >= timeout then
                userlog:sendlog(core.ERROR, string.format("proxy dispatch <%s> timed-out after %d seconds", rspq, duration))
                do return end
            end
        end
    end
end

local package = {
    proxy = proxy
}

return package
