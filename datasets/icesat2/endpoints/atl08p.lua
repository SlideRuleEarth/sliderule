-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local proxy = require("proxy")

    local rqst = json.decode(arg[1])
    local resources = rqst["resources"]
    local parms = rqst["parms"]

    proxy.proxy(resources, parms, "atl08", "atl08rec")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "ATL08 PhoReal Vegetation Metrics Parallel",
    description = "Generates ATL08-like vegetation metrics using user supplied processing parameters (p-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}