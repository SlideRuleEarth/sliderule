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
    local arrow_parms = nil
    local arrow_file = nil
    local arrow_metafile = nil
    if parms[arrow.PARMS] then
        arrow_parms = arrow.parms(parms[arrow.PARMS])
        -- Determine if Keeping Local File (needed for later ArrowSampler) --
        local keep_local = parms[geo.PARMS] ~= nil
        -- Arrow Builder --
        if arrow_parms:isarrow() then
            local parms_str = json.encode(parms)
            arrow_builder = arrow.builder(arrow_parms, rspq, rspq .. "-builder", rec, rqstid, parms_str, endpoint, keep_local)
            if arrow_builder then
                rsps_from_nodes = rspq .. "-builder"
                terminate_proxy_stream = true
                if keep_local then
                    arrow_file, arrow_metafile = arrow_builder:filenames()
                end
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
            userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> retrieved %d resources from CMR", rspq, #resources))
        else
            userlog:alert(core.CRITICAL, core.RTE_SIMPLIFY, string.format("request <%s> failed to make CMR request <%d>: %s", rspq, rc, rsps))
            return
        end
    end

    -- Populate Catalogs via STAC and TNM Requests --
    local geo_parms = parms[geo.PARMS]
    if geo_parms then
        for dataset,raster_parms in pairs(geo_parms) do
            if not raster_parms["catalog"] then
                userlog:alert(core.INFO, core.RTE_INFO, string.format("proxy request <%s> querying resources for %s", rspq, dataset))
                local rc, rsps = earthdata.search(raster_parms, parms["poly"])
                if rc == earthdata.SUCCESS then
                    parms[geo.PARMS][dataset]["catalog"] = json.encode(rsps)
                    userlog:alert(core.INFO, core.RTE_INFO, string.format("proxy request <%s> returned %d resources for %s", rspq, #parms[geo.PARMS][dataset]["catalog"]["features"], dataset))
                else
                    userlog:alert(core.WARNING, core.RTE_INFO, string.format("request <%s> failed to get catalog for %s: %d", rspq, dataset, rc))
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
            userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> timed-out after %d seconds waiting for endpoint proxy", rspq, duration))
            do return end
        end
    end

    -- Handle Arrow Builder and Arrow Sampler --
    if arrow_builder then
        -- Create Raster Objects for Arrow Sampler --
        local georasters = nil
        if parms[geo.PARMS] then
            georasters = {}
            for key,settings in pairs(parms[geo.PARMS]) do
                local robj = geo.raster(geo.parms(settings))
                if robj then
                    georasters[key] = robj
                else
                    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed to create raster %s", rspq, key))
                end
            end
         end

        -- Wait Until Arrow Builder Completion --
        while (userlog:numsubs() > 0) and not arrow_builder:waiton(interval * 1000) do
            duration = duration + interval
            if timeout >= 0 and duration >= timeout then
                userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> timed-out after %d seconds waiting for arrow builder", rspq, duration))
                do return end
            end
        end

        -- Create Arrow Sampler and Sample Rasters --
        local arrow_sampler = arrow.sampler(arrow_parms, arrow_file, rspq, georasters)

        -- Wait Until Arrow Sampler Completion --
        while (userlog:numsubs() > 0) and not arrow_sampler:waiton(interval * 1000) do
            duration = duration + interval
            if timeout >= 0 and duration >= timeout then
                userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> timed-out after %d seconds waiting for arrow sampler", rspq, duration))
                do return end
            end
            userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> continuing to sample rasters after %d seconds...", rspq, duration))
        end
    end

    -- cleanup
    if arrow_file then
        os.remove(arrow_file)
    end
    if arrow_metafile then
        os.remove(arrow_metafile)
    end
end

local package = {
    proxy = proxy
}

return package
