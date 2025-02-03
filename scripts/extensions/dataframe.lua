--
-- EXTENSION:   dataframe.lua
--

--
-- Imports
--
local earthdata = require("earth_data_query")
local prettyprint = require("prettyprint")

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

    -- Use Parms -OR- Query EarthData for Resources to Process
    local earthdata_status, resources = earthdata.query(parms, rspq, userlog)
    if earthdata_status ~= earthdata.SUCCESS then
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> earthdata queury failed : %d", rspq, earthdata_status))
        return RC_EARTHDATA_FAILURE
    end

    -- Check for Pass Through
    if #parms["resource"] > 0 then
        return RC_PASS_THROUGH
    end

print("HERE 3")
prettyprint.display(resources)
    -- Initialize Variables
    local proxyq_name = "proxy."..rspq
    local rqst_json = parms:encode()
    local node_timeout = parms["node_timeout"]
    local cluster_size_hint = parms["cluster_size_hint"]
    local locks_per_node = (parms["poly"] and not parms["ignore_poly_for_cmr"]) and 1 or core.MAX_LOCKS_PER_NODE

    -- Create Receiving DataFrame
    local df = core.dataframe()
    df:receive(proxyq_name, rspq, #resources, parms["rqst_timeout"])

    -- Proxy Request --
    local endpoint_proxy = core.proxy(endpoint, resources, rqst_json, node_timeout, locks_per_node, proxyq_name, true, cluster_size_hint)

    -- Receive DataFrame (blocks until dataframe complete or timeout) --
    if not df:waiton(parms["rqst_timeout"] * 1000) then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> failed to receive proxied dataframe"));
        return RC_PROXY_FAILURE
    end

    -- Create Arrow DataFrame --
    local arrow_dataframe = arrow.dataframe(parms, df)
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

    -- Success --
    return RC_SUCCESS

end

--
-- Function: timeout
--
--  calculate a current timeout
--
local function timeout(full_timeout, start_time)
    local current_timeout = (full_timeout * 1000) - (time.gps() - start_time)
    if current_timeout < 0 then current_timeout = 0 end
    return math.tointeger(current_timeout)
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
    timeout = timeout
}

return package
