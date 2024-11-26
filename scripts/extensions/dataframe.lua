--
-- EXTENSION:   dataframe.lua
--

--
-- Imports
--
local json = require("json")

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
local function proxy(endpoint, parms, rspq, userlog)

    -- Check Resource --
    if parms["resource"] then
        return nil, RC_PASS_THROUGH
    end

    -- Proxy Request --
    local resources = parms["resources"]
    local parms_tbl = parms:export()
    parms_tbl["resources"] = nil -- remove list of resources in proxied request
    local rqst_json = json.encode(parms_tbl)
    local proxyq_name = "proxy."..rspq
    local proxyq = msg.subscribe(proxyq_name) -- needed for receiving dataframe
    local node_timeout = parms["node_timeout"]
    local cluster_size_hint = parms["cluster_size_hint"]
    local locks_per_node = (parms["poly"] and not parms["ignore_poly_for_cmr"]) and 1 or core.MAX_LOCKS_PER_NODE
    local endpoint_proxy = core.proxy(endpoint, resources, rqst_json, node_timeout, locks_per_node, proxyq_name, true, cluster_size_hint)

    -- Receive DataFrame (blocks until dataframe complete or timeout) --
    local dataframe = core.rxdataframe(proxyq, rspq, parms["rqst_timeout"])
    if not dataframe then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to receive dataframe"));
        return nil, RC_PROXY_FAILURE
    end

    -- Success --
    return dataframe, RC_SUCCESS

end

--
-- Function: send
--
--  transmit dataframe back to requesting client
--
local function send(dataframe, parms, rspq, userlog)

    -- Create Arrow DataFrame --
    local arrow_dataframe = arrow.dataframe(parms, dataframe)
    if not arrow_dataframe then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to create arrow dataframe", rspq))
        return RC_ARROW_FAILURE
    end

    -- Write DataFrame to Parquet File --
    local arrow_filename = arrow_dataframe:export(nil, arrow.PARQUET)
    if not arrow_filename then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to write dataframe", rspq))
        return RC_PARQUET_FAILURE
    end

    -- Send Parquet File to User --
    arrow.send2user(arrow_filename, parms, rspq)
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
    proxy = proxy,
    send = send
}

return package
