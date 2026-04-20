-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local proxy = require("proxy")

    local rqst = json.decode(arg[1])
    local resources = rqst["resources"]
    local parms = rqst["parms"]

    proxy.proxy(resources, parms, "atl06s", "atl06srec")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "ATL06 Parallel Subsetter",
    description = "Spatially and temporally subsets ATL06 granules elevations with additional filters (s-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}