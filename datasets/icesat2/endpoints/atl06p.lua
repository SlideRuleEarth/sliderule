-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local proxy     = require("proxy")
local rqst      = json.decode(arg[1])
local resources = rqst["resources"]
local parms     = rqst["parms"]

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    proxy.proxy(resources, parms, "atl06", "atl06rec")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL06-SR Parallel",
    description = "Generates ATL06 elevations using user supplied processing parameters (p-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}