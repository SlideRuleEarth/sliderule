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
local RC_SEND_FAILURE = -6

--
-- populate_catalogs
--
local function populate_catalogs(rqst, q, userlog)
    if rqst[geo.PARMS] then
        for dataset,raster_parms in pairs(rqst[geo.PARMS]) do
            if not raster_parms["catalog"] then
                userlog:alert(core.INFO, core.RTE_STATUS, string.format("proxy request <%s> querying resources for %s", q, dataset))
                local rc, rsps = earthdata.search(raster_parms, rqst["poly"])
                if rc == RC_SUCCESS then
                    rqst[geo.PARMS][dataset]["catalog"] = json.encode(rsps)
                    userlog:alert(core.INFO, core.RTE_STATUS, string.format("proxy request <%s> returned %d resources for %s", q, rsps and rsps["features"] and #rsps["features"] or 0, dataset))
                elseif rc ~= RC_UNSUPPORTED then
                    userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("request <%s> failed to get catalog for %s <%d>: %s", q, dataset, rc, rsps))
                    rqst[geo.PARMS][dataset]["catalog"] = {}
                end
            end
        end
    end
end

--
-- get_resources
--
local function get_resources(rqst, q, userlog)
    local max_retries = 3
    local attempt = 1
    while true do
        local rc, rsps = earthdata.search(rqst)
        if rc == earthdata.SUCCESS then
            userlog:alert(core.INFO, core.RTE_STATUS, string.format("request <%s> retrieved %d resources", q, #rsps))
            return RC_SUCCESS, rsps
        elseif rc == earthdata.RSPS_TRUNCATED then
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> failed query... response truncated: %s", q, rsps))
            return RC_EARTHDATA_FAILURE, nil
        elseif rc == earthdata.UNSUPPORTED then
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> failed query... unsupported dataset: %s", q, rsps))
            return RC_EARTHDATA_FAILURE, nil
        else -- retry
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> failed attempt %d <%d>: %s", q, attempt, rc, rsps))
            attempt = attempt + 1
            if attempt > max_retries then
                userlog:alert(core.CRITICAL, core.RTE_SIMPLIFY, string.format("request <%s> failed query... aborting!", q))
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
        local dataframes, runners = create(userlog)
        local node_timeout = parms["node_timeout"]
        local sender = core.framesender(rspq, parms["key_space"], node_timeout)

        -- Add Runners to Dataframes
        for _, df in pairs(dataframes) do
            -- Add Provided Runners
            for _, runner in ipairs(runners) do
                df:run(runner)
            end
            -- Add Sampler Runner
            if parms:withsamplers() then
                df:run(geo.framesampler(parms))
            end
            -- Add Default Runners
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
                userlog:alert(core.INFO, core.RTE_STATUS, string.format("request <%s> on %s generated dataframe [%s] with %d rows and %s columns", rspq, parms["resource"], key, df:numrows(), df:numcols()))
            else
                userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> on %s timed out waiting for dataframe [%s] to complete", rspq, parms["resource"], key))
            end
        end

        -- Complete
        return RC_SUCCESS

    end

    -- Query EarthData for Resources to Process
    local resources = nil
    if rqst["resources"] and (#rqst["resources"] > 0) then
        resources = rqst["resources"]
    else
        local status
        rqst["asset"] = parms["asset"]
        status, resources = get_resources(rqst, rspq, userlog)
        if status ~= RC_SUCCESS then
            userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> earthdata queury failed: %d", rspq, status))
            return status
        end
    end

    -- Check Request Constraints
    if #resources <= 0 then
        userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("request <%s> has no resources to process", rspq))
        return RC_NO_RESOURCES
    end

    -- Initialize Variables
    local proxyq_name = "proxy."..rspq
    local locks_per_node = rqst["locks"] or (rqst["poly"] and 1 or core.MAX_LOCKS_PER_NODE)

    -- Create Receiving DataFrame
    local rqst_str = json.encode(rqst)
    local df = core.dataframe({}, {endpoint=endpoint, request=rqst_str})
    local expected_concurrent_channels = #resources * channels
    df:receive(proxyq_name, rspq, expected_concurrent_channels, parms["rqst_timeout"] * 1000)

    -- Proxy Request
    local endpoint_proxy = core.proxy(endpoint, resources, rqst_str, parms["node_timeout"], locks_per_node, proxyq_name, true, parms["num_nodes"], _rqst.srcip)

    -- Receive DataFrame (blocks until dataframe complete or timeout)
    if not df:waiton(parms["rqst_timeout"] * 1000) then
        userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("request <%s> failed to receive proxied dataframe"));
        return RC_PROXY_FAILURE
    end

    -- Check DataFrame Constraints
    if df:numrows() <= 0 or df:numcols() <= 0 then
        userlog:alert(core.WARNING, core.RTE_STATUS, string.format("request <%s> produced an empty dataframe", rspq));
    end

    -- Return to User
    local rc = RC_SUCCESS
    local result = nil
    if parms:witharrow() then
        -- Create Arrow DataFrame
        local arrow_dataframe = arrow.dataframe(parms, df)
        if not arrow_dataframe then
            userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("request <%s> failed to create arrow dataframe", rspq))
            return RC_ARROW_FAILURE
        end

        -- Write DataFrame to Parquet File
        local arrow_filename = arrow_dataframe:export()
        if not arrow_filename then
            userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("request <%s> failed to write dataframe", rspq))
            return RC_PARQUET_FAILURE
        end

        -- Send Parquet File to User
        local status = core.send2user(arrow_filename, parms, rspq)
        if not status then rc = RC_SEND_FAILURE end
    else
        -- Return Dataframe back to User
        result = df
    end

    -- Return Code --
    return rc, result

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
    SEND_FAILURE = RC_SEND_FAILURE,
    proxy = proxy,
    get_resources = get_resources
}

return package
