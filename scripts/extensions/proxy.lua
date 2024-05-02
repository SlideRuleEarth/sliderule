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
local earthdata = require("earth_data_query")

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
        if output_parms:isarrow() then
            local parms_str = json.encode(parms)
            arrow_builder = arrow.builder(output_parms, rspq, rspq .. "-builder", rec, rqstid, parms_str, endpoint)
            if arrow_builder then
                rsps_from_nodes = rspq .. "-builder"
                terminate_proxy_stream = true
            end
        end
    end

    -- Determine Locks per Node --
    local locks_per_node = (parms["poly"] and not parms["ignore_poly_for_cmr"]) and 1 or netsvc.MAX_LOCKS_PER_NODE

    -- Populate Resources via CMR Request --
    if not resources then
        local rc, rsps = earthdata.cmr(parms)
        if rc == earthdata.SUCCESS then
            resources = rsps
            userlog:alert(core.INFO, core.RTE_INFO, string.format("proxy request <%s> retrieved %d resources from CMR", rspq, #resources))
        else
            userlog:alert(core.CRITICAL, core.RTE_SIMPLIFY, string.format("proxy request <%s> failed to make CMR request <%d>: %s", rspq, rc, rsps))
            return
        end
    end

    -- Populate Catalogs via STAC and TNM Requests --
    local geo_parms = parms[geo.PARMS]
    if geo_parms then
        for dataset,raster_parms in pairs(geo_parms) do
            if not raster_parms["catalog"] then
                local rc, rsps = earthdata.search(raster_parms, parms["poly"])
                if rc == earthdata.SUCCESS then
                    parms[geo.PARMS][dataset]["catalog"] = json.encode(rsps)
                else
                    userlog:alert(core.WARNING, core.RTE_INFO, string.format("proxy request <%s> failed to get catalog for %s: %d", rspq, dataset, rc))
                end
            end
        end
    end

    -- Proxy Request --
    local endpoint_proxy = netsvc.proxy(endpoint, resources, json.encode(parms), node_timeout, locks_per_node, rsps_from_nodes, terminate_proxy_stream, cluster_size_hint)

    -- Wait Until Proxy Completes --
    local duration = 0
    local interval = 10 < timeout and 10 or timeout -- seconds
    while (userlog:numsubs() > 0) and not endpoint_proxy:waiton(interval * 1000) do
        duration = duration + interval
        if timeout >= 0 and duration >= timeout then
            userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("proxy request <%s> timed-out after %d seconds", rspq, duration))
            do return end
        end
    end

    -- Wait Until Dispatch Completion --
    if arrow_builder then
        while (userlog:numsubs() > 0) and not arrow_builder:waiton(interval * 1000) do
            duration = duration + interval
            if timeout >= 0 and duration >= timeout then
                userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("proxy dispatch <%s> timed-out after %d seconds", rspq, duration))
                do return end
            end
        end
    end
end

local package = {
    proxy = proxy
}

return package
