-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local proxy = require("proxy")

    local rqst = json.decode(arg[1])
    local resources = rqst["resources"]
    local parms = rqst["parms"]

    proxy.proxy(resources, parms, "gedi04a", "gedi04arec")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "GEDI 4A Parallel Subsetter",
    description = "Spatially and temporally subsets above ground biomass density from multiple GEDI 4A granules with additional filters (s-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}