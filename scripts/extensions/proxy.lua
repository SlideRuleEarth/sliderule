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

local function proxy(resources, parms, endpoint, rec)
    -- Create User Status --
    local userlog = msg.publish(rspq)

    -- Request Parameters --
    local timeout = parms["rqst-timeout"] or parms["timeout"] or netsvc.RQST_TIMEOUT
    local node_timeout = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT
    local cluster_size_hint = parms["cluster_size_hint"] or netsvc.cluster_size_hint

    -- Initialize Queue Management --
    local rsps_from_nodes = rspq
    local terminate_proxy_stream = false

    -- Handle Output Options --
    local arrow_builder = nil
    if parms[arrow.PARMS] then
        local output_parms = arrow.parms(parms[arrow.PARMS])
        -- Arrow Writer --
        if output_parms:isparquet() or output_parms:iscsv() then
            arrow_builder = arrow.builder(output_parms, rspq, rspq .. "-builder", rec, rqstid)
            if arrow_builder then
                rsps_from_nodes = rspq .. "-builder"
                terminate_proxy_stream = true
            end
        end
    end

    -- Determine Locks per Node --
    local locks_per_node = (parms["poly"] and not parms["ignore_poly_for_cmr"]) and 1 or netsvc.MAX_LOCKS_PER_NODE

    -- Proxy Request --
    local proxy = netsvc.proxy(endpoint, resources, json.encode(parms), node_timeout, locks_per_node, rsps_from_nodes, terminate_proxy_stream, cluster_size_hint)

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
        while (userlog:numsubs() > 0) and not arrow_builder:waiton(interval * 1000) do
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
