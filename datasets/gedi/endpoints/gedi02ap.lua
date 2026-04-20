-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local proxy = require("proxy")

    local rqst = json.decode(arg[1])
    local resources = rqst["resources"]
    local parms = rqst["parms"]

    proxy.proxy(resources, parms, "gedi02a", "gedi02arec")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "GEDI 2A Parallel Subsetter",
    description = "Spatially and temporally subsets elevations from multiple GEDI 2A granules with additional filters (s-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}