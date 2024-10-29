--
-- ENDPOINT:    /source/atl24s
--
local endpoint      = "atl24s"
local json          = require("json")
local earthdata     = require("earth_data_query")
local rqst          = json.decode(arg[1])
local parms         = bathy.parms(rqst["parms"], 0, "icesat2", rqst["resource"])
local resource      = parms["resource"]
local resources     = parms["resources"]
local userlog       = msg.publish(rspq) -- create user log publisher (alerts)

------------------------------------
--- Proxy Request
------------------------------------
if resources == nil then

    -- Populate Catalogs via STAC and TNM Requests --
    local geo_parms = parms[geo.PARMS]
    if geo_parms then
        for dataset,raster_parms in pairs(geo_parms) do
            if not raster_parms["catalog"] then
                userlog:alert(core.INFO, core.RTE_INFO, string.format("proxy request <%s> querying resources for %s", rspq, dataset))
                local rc, rsps = earthdata.search(raster_parms, parms["poly"])
                if rc == earthdata.SUCCESS then
                    parms:setcatalog(dataset,json.encode(rsps))
                    userlog:alert(core.INFO, core.RTE_INFO, string.format("proxy request <%s> returned %d resources for %s", rspq, rsps and #rsps["features"] or 0, dataset))
                elseif rc ~= earthdata.UNSUPPORTED then
                    userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to get catalog for %s <%d>: %s", rspq, dataset, rc, rsps))
                end
            end
        end
    end

    -- Populate Resources via CMR Request --
    if not resources then
        local rc, rsps = earthdata.cmr(parms:export())
        if rc == earthdata.SUCCESS then
            resources = rsps
            userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> retrieved %d resources from CMR", rspq, #resources))
        else
            userlog:alert(core.CRITICAL, core.RTE_SIMPLIFY, string.format("request <%s> failed to make CMR request <%d>: %s", rspq, rc, rsps))
            return
        end
    end

    -- Proxy Request --
    local locks_per_node = (parms["poly"] and not parms["ignore_poly_for_cmr"]) and 1 or core.MAX_LOCKS_PER_NODE
    local endpoint_proxy = core.proxy(endpoint, resources, json.encode(rqst["parms"]), parms["node_timeout"], locks_per_node, rspq, false, parms["cluster_size_hint"])

    -- Wait Until Proxy Completes --
    local timeout = parms["rqst_timeout"]
    local duration = 0
    local interval = 10 < timeout and 10 or timeout -- seconds
    while (userlog:numsubs() > 0) and not endpoint_proxy:waiton(interval * 1000) do
        duration = duration + interval
        if timeout >= 0 and duration >= timeout then
            userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> timed-out after %d seconds waiting for endpoint proxy", rspq, duration))
            do return end
        end
    end

------------------------------------
--- Granule Request
------------------------------------
else

    print(resource)

end