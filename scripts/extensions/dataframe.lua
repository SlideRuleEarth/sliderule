--
-- EXTENSION:   dataframe.lua
--

--
-- Imports
--
local earthdata = require("earth_data_query")
local json = require("json")

--
-- Constants
--
local RC_SUCCESS = 0
local RC_EARTHDATA_FAILURE = -1
local RC_PROXY_FAILURE = -2
local RC_ARROW_FAILURE = -3
local RC_PARQUET_FAILURE = -4
local RC_NO_RESOURCES = -5
local RC_EMPTY_DATAFRAME = -6

--
-- populate_catalogs
--
local function populate_catalogs(rqst, q, userlog)
    if rqst[geo.PARMS] then
        for dataset,raster_parms in pairs(rqst[geo.PARMS]) do
            if not raster_parms["catalog"] then
                userlog:alert(core.INFO, core.RTE_INFO, string.format("proxy request <%s> querying resources for %s", q, dataset))
                local rc, rsps = earthdata.search(raster_parms, rqst["poly"])
                if rc == RC_SUCCESS then
                    rqst[geo.PARMS][dataset]["catalog"] = json.encode(rsps)
                    userlog:alert(core.INFO, core.RTE_INFO, string.format("proxy request <%s> returned %d resources for %s", q, rsps and #rsps["features"] or 0, dataset))
                elseif rc ~= RC_UNSUPPORTED then
                    userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to get catalog for %s <%d>: %s", q, dataset, rc, rsps))
                    rqst[geo.PARMS][dataset]["catalog"] = {}
                end
            end
        end
    end
end

--
-- get_resources
--
local function get_resources(parms, rqst, q, userlog)
    if rqst["resources"] and (#rqst["resources"] > 0) then
        return RC_SUCCESS, rqst["resources"]
    end
    local max_retries = 3
    local attempt = 1
    while true do
        local rc, rsps = earthdata.search(parms:export())
        if rc == earthdata.SUCCESS then
            userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> retrieved %d resources", q, #rsps))
            return RC_SUCCESS, rsps
        else
            userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed attempt %d <%d>: %s", q, attempt, rc, rsps))
            attempt = attempt + 1
            if attempt > max_retries then
                userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> failed query... aborting!", q))
                return RC_EARTHDATA_FAILURE, nil
            end
        end
    end
end

--
-- Function: proxy
--
--  fanout request to multiple nodes and assemble results
--
local function proxy(endpoint, parms, rqst, rspq, channels, create)

    -- Initialize Variables
    local start_time = time.gps() -- for timeout handling
    local userlog = msg.publish(rspq) -- for alerts

    -- Populate Catalogs on Initial User Request
    if parms["key_space"] == core.INVALID_KEY then
        populate_catalogs(rqst, rspq, userlog)
    end

    -- Check if Resource Already Set
    if #parms["resource"] > 0 then

        -- Create Dataframes and Runners
        local dataframes, runners = create(parms, rspq, userlog)
        local node_timeout = parms["node_timeout"]
        local sender = core.framesender(rspq, parms["key_space"], node_timeout)

        -- Add Runners to Dataframes
        for _, df in pairs(dataframes) do
            for _, runner in pairs(runners) do
                df:run(runner)
            end
            df:run(sender)
            df:run(core.TERMINATE)
        end

        -- Wait for Dataframes to Complete
        for key, df in pairs(dataframes) do
            local current_timeout = (node_timeout * 1000) - (time.gps() - start_time)
            if current_timeout < 0 then current_timeout = 0 end
            local remaining_timeout = math.tointeger(current_timeout)
            local status = df:finished(remaining_timeout, rspq)
            if status then
                userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> on %s sent dataframe [%s] with %d rows and %s columns", rspq, parms["resource"], key, df:numrows(), df:numcols()))
            else
                userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> on %s timed out waiting for dataframe [%s] to complete", rspq, parms["resource"], key))
            end
        end

        -- Complete
        return RC_SUCCESS

    end

    -- Query EarthData for Resources to Process
    local status, resources = get_resources(parms, rqst, rspq, userlog)
    if status ~= RC_SUCCESS then
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> earthdata queury failed: %d", rspq, status))
        return status
    end

    -- Check Request Constraints
    if #resources <= 0 then
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> has no resources to process", rspq))
        return RC_NO_RESOURCES
    end

    -- Initialize Variables
    local proxyq_name = "proxy."..rspq
    local locks_per_node = (rqst["poly"] and not parms["ignore_poly_for_cmr"]) and 1 or core.MAX_LOCKS_PER_NODE

    -- Create Receiving DataFrame
    local df = core.dataframe()
    local expected_concurrent_channels = #resources * channels
    df:receive(proxyq_name, rspq, expected_concurrent_channels, parms["rqst_timeout"] * 1000)

    -- Proxy Request
    local endpoint_proxy = core.proxy(endpoint, resources, json.encode(rqst), parms["node_timeout"], locks_per_node, proxyq_name, true, parms["num_nodes"], 1)

    -- Receive DataFrame (blocks until dataframe complete or timeout)
    if not df:waiton(parms["rqst_timeout"] * 1000) then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to receive proxied dataframe"));
        return RC_PROXY_FAILURE
    end

    -- Check DataFrame Constraints
    if df:numrows() <= 0 or df:numcols() <= 0 then
        userlog:alert(core.WARNING, core.RTE_INFO, string.format("request <%s> produced an empty dataframe", rspq));
        return RC_EMPTY_DATAFRAME
    end

    -- Create Arrow DataFrame
    local arrow_dataframe = arrow.dataframe(parms, df)
    if not arrow_dataframe then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to create arrow dataframe", rspq))
        return RC_ARROW_FAILURE
    end

    -- Write DataFrame to Parquet File
    local arrow_filename = arrow_dataframe:export()
    if not arrow_filename then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to write dataframe", rspq))
        return RC_PARQUET_FAILURE
    end

    -- Send Parquet File to User
    arrow.send2user(arrow_filename, parms, rspq)

    -- Success --
    return RC_SUCCESS

end

--
-- package
--
local package = {
    SUCCESS = RC_SUCCESS,
    EARTHDATA_FAILURE = RC_EARTHDATA_FAILURE,
    PROXY_FAILURE = RC_PROXY_FAILURE,
    ARROW_FAILURE = RC_ARROW_FAILURE,
    NO_RESOURCES = RC_NO_RESOURCES,
    EMPTY_DATAFRAME = RC_NO_RESOURCES,
    proxy = proxy
}

return package
