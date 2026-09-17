--
-- EXTENSION:   dataframe.lua
--

--
-- Imports
--
local earthdata = require("earth_data_query")
local json = require("json")
local las = require("las")

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
local RC_LAS_FAILURE = -7

--
-- populate_catalogs
--
local function populate_catalogs(rqst, q, userlog, poly, parms)
    if rqst[geo.PARMS] then
        for dataset,raster_parms in pairs(rqst[geo.PARMS]) do
            if not raster_parms["catalog"] then
                userlog:alert(core.INFO, core.RTE_STATUS, string.format("proxy <%s> querying resources for %s", q, dataset))
                local rc, rsps = earthdata.search(raster_parms, poly)
                if rc == RC_SUCCESS then
                    if parms then
                        parms:setcatalog(dataset, json.encode(rsps))
                    else
                        rqst[geo.PARMS][dataset]["catalog"] = json.encode(rsps)
                    end
                    userlog:alert(core.INFO, core.RTE_STATUS, string.format("proxy <%s> returned %d resources for %s", q, rsps and rsps["features"] and #rsps["features"] or 0, dataset))
                elseif rc ~= RC_UNSUPPORTED then
                    userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("<%s> failed to get catalog for %s <%d>: %s", q, dataset, rc, rsps))
                end
            end
        end
    end
end

--
-- get_resources
--
local function get_resources(rqst, q, userlog)
    local rc, rsps = earthdata.search(rqst)
    if rc == earthdata.SUCCESS then
        userlog:alert(core.INFO, core.RTE_STATUS, string.format("<%s> retrieved %d resources", q, #rsps))
        return RC_SUCCESS, rsps
    elseif rc == earthdata.RSPS_TRUNCATED then
        userlog:alert(core.CRITICAL, core.RTE_TOO_MANY_RESOURCES, string.format("<%s> query response truncated: %s", q, rsps))
        return RC_EARTHDATA_FAILURE, nil
    else
        userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("<%s> failed query: %s", q, rsps))
        return RC_EARTHDATA_FAILURE, nil
    end
end

--
-- poly_from_bbox
--
local function poly_from_dataframes(dataframes)
    local min_x = 180.0
    local min_y = 90.0
    local max_x = -180.0
    local max_y = -90.0
    for _, df in pairs(dataframes) do
        local df_min_x, df_min_y, df_max_x, df_max_y = df:bbox()
        if df_min_x < min_x then min_x = df_min_x end
        if df_min_y < min_y then min_y = df_min_y end
        if df_max_x > max_x then max_x = df_max_x end
        if df_max_y > max_y then max_y = df_max_y end
    end
    return {
        {lat = min_y, lon = min_x},
        {lat = min_y, lon = max_x},
        {lat = max_y, lon = max_x},
        {lat = max_y, lon = min_x},
        {lat = min_y, lon = min_x}
    }
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
        if not parms["jit_catalog"] then
            populate_catalogs(rqst, rspq, userlog, rqst["poly"])
        end
    end

    -- Check if Resource Already Set
    if #parms["resource"] > 0 then

        -- Create Dataframes and Runners
        local dataframes, runners = create(userlog)
        local node_timeout = parms["node_timeout"]

        -- (Optionally) Create Frame Sender
        local sender = nil
        if parms:withsamplers() then
            sender = core.framesender(parms, rspq)
        end

        -- Add Runners to Dataframes
        for _, df in pairs(dataframes) do
            -- Add Provided Runners
            for _, runner in ipairs(runners) do
                df:run(runner)
            end
            -- (Optionally) Add Frame Sender
            if not parms:withsamplers() then
                df:run(sender)
            end
            -- Add Default Runners
            df:run(core.TERMINATE)
        end

        -- Wait for Dataframes to Complete
        for key, df in pairs(dataframes) do
            local current_timeout = (node_timeout * 1000) - (time.gps() - start_time)
            if current_timeout < 0 then current_timeout = 0 end
            local remaining_timeout = math.tointeger(current_timeout)
            local status = df:finished(remaining_timeout, rspq)
            if status then
                userlog:alert(core.INFO, core.RTE_STATUS, string.format("<%s> %s/%s generated %d rows and %s columns", rspq, parms["resource"], key, df:numrows(), df:numcols()))
            else
                userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("<%s> %s/%s timed out waiting to complete", rspq, parms["resource"], key))
            end
        end

        -- With Sampler
        if parms:withsamplers() then
            if parms["jit_catalog"] then
                local poly = poly_from_dataframes(dataframes)
                populate_catalogs(rqst, rspq, userlog, poly, parms)
            end
            local status, errmsg = geo.multisampler(parms, dataframes)
            if status then
                for _, df in pairs(dataframes) do
                    df:send(rspq, parms["key_space"] + (df:key() << 32))
                end
            else
                userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("<%s> %s processing aborted: %s", rspq, parms["resource"], errmsg))
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
        if status ~= RC_SUCCESS then return status end
    end

    -- Check Request Constraints
    local num_resources = #resources
    if num_resources <= 0 then
        userlog:alert(core.CRITICAL, core.RTE_FAILURE, string.format("<%s> has no resources to process", rspq))
        return RC_NO_RESOURCES
    end

    -- Initialize Variables
    local proxyq_name = "proxy."..rspq
    local locks_per_node = rqst["locks"] or (rqst["poly"] and 1 or core.MAX_LOCKS_PER_NODE)

    -- Create Receiving DataFrame
    local rqst_str = json.encode(rqst)
    local df = core.dataframe({}, {endpoint=endpoint, request=rqst_str:sub(0,1048576), num_resources=num_resources})
    local expected_concurrent_channels = num_resources * channels
    df:receive(proxyq_name, rspq, expected_concurrent_channels, parms["rqst_timeout"] * 1000)

    -- Proxy Request
    local endpoint_proxy = core.proxy(endpoint, resources, rqst_str, parms["node_timeout"], locks_per_node, proxyq_name, true, parms["num_nodes"], _rqst.srcip)

    -- Receive DataFrame (blocks until dataframe complete or timeout)
    if not df:waiton(parms["rqst_timeout"] * 1000) then
        userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("<%s> failed to receive proxied dataframe"));
        return RC_PROXY_FAILURE
    elseif df:inerror() then
        userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("<%s> detected error in received dataframe"));
        return RC_PROXY_FAILURE
    end

    -- Check DataFrame Constraints
    if df:numcols() <= 0 then
        userlog:alert(core.WARNING, core.RTE_STATUS, string.format("<%s> resulted in an invalid dataframe", rspq));
    elseif df:numrows() <= 0 then
        userlog:alert(core.WARNING, core.RTE_STATUS, string.format("<%s> produced an empty dataframe", rspq));
    end

    -- Return to User
    local rc = RC_SUCCESS
    local result = nil
    if parms:withlas() then

        -- Create LAS DataFrame
        local las_dataframe = las.dataframe(parms, df)
        if not las_dataframe then
            userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("<%s> failed to create LAS dataframe", rspq))
            return RC_LAS_FAILURE
        end

        -- Write DataFrame to LAS File
        local las_filename = las_dataframe:export()
        if not las_filename then
            userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("<%s> failed to export LAS/LAZ output", rspq))
            return RC_LAS_FAILURE
        end

        -- Send LAS File to User
        local status = core.send2user(las_filename, rspq, parms)
        if not status then rc = RC_SEND_FAILURE end

    elseif parms:witharrow() then

        -- Create Arrow DataFrame
        local arrow_dataframe = arrow.dataframe(parms, df)
        if not arrow_dataframe then
            userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("<%s> failed to create arrow dataframe", rspq))
            return RC_ARROW_FAILURE
        end

        -- Write DataFrame to Parquet File
        local arrow_filename = arrow_dataframe:export()
        if not arrow_filename then
            userlog:alert(core.ERROR, core.RTE_FAILURE, string.format("<%s> failed to write dataframe", rspq))
            return RC_PARQUET_FAILURE
        end

        -- Send Parquet File to User
        local status = core.send2user(arrow_filename, rspq, parms)
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
