--
-- EXTENSION:   dataframe.lua
--

--
-- Imports
--
local json = require("json")
local earthdata = require("earth_data_query")

--
-- Constants
--
local RC_SUCCESS = 1
local RC_PASS_THROUGH = 0
local RC_EARTHDATA_FAILURE = -1
local RC_PROXY_FAILURE = -2
local RC_ARROW_FAILURE = -3
local RC_PARQUET_FAILURE = -4

--
-- Function: proxy
--
--  fanout request to multiple nodes and assemble results
--
local function proxy(parms, endpoint, userlog)

    -- Check Resource
    if parms["resource"] then
        return RC_PASS_THROUGH
    end

    -- DataFrame Builder --
    local dataframe_rspq = "dataframe."..rspq
    local dataframe = nil -- given a msgq, it receives dataframe records and assembles a dataframe

    -- Proxy Request --
    local resources = parms["resources"]
    local parms_tbl = parms:export()
    parms_tbl["resources"] = nil
    local rqst_json = json.encode(parms_tbl)
    local node_timeout = parms["node_timeout"]
    local cluster_size_hint = parms["cluster_size_hint"]
    local locks_per_node = (parms["poly"] and not parms["ignore_poly_for_cmr"]) and 1 or core.MAX_LOCKS_PER_NODE
    local endpoint_proxy = core.proxy(endpoint, resources, rqst_json, node_timeout, locks_per_node, dataframe_rspq, false, cluster_size_hint)

    -- Setup Timeout --
    local timeout = parms["rqst_timeout"]
    local duration = 0
    local interval = 10 < timeout and 10 or timeout -- seconds

    -- Wait Until Proxy Completes --
    while (userlog:numsubs() > 0) and not endpoint_proxy:waiton(interval * 1000) do
        duration = duration + interval
        if timeout >= 0 and duration >= timeout then
            userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> timed-out after %d seconds waiting for endpoint proxy", rspq, duration))
            return RC_PROXY_FAILURE
        end
    end

    -- Wait Until DataFrame Builder Completes --

    -- Create Arrow DataFrame
    local arrow_dataframe = arrow.dataframe(parms, dataframe)
    if not arrow_dataframe then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to create arrow dataframe", rspq))
        return RC_ARROW_FAILURE
    end

    -- Write DataFrame to Parquet File
    local arrow_filename = arrow_dataframe:export(nil, arrow.PARQUET)
    if not arrow_filename then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to write dataframe", rspq))
        return RC_PARQUET_FAILURE
    end

    -- Send Parquet File to User
    arrow.send2user(arrow_filename, parms, rspq)

    -- Success
    return RC_SUCCESS
end

--
-- package
--
local package = {
    SUCCESS = RC_SUCCESS,
    PASS_THROUGH = RC_PASS_THROUGH,
    EARTHDATA_FAILURE = RC_EARTHDATA_FAILURE,
    PROXY_FAILURE = RC_PROXY_FAILURE,
    ARROW_FAILURE = RC_ARROW_FAILURE,
    PARQUET_FAILURE = RC_PARQUET_FAILURE,
    proxy = proxy
}

return package
