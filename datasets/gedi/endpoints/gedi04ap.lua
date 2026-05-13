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
    proxy.proxy(resources, parms, "gedi04a", "gedi04arec")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "GEDI 4A Parallel Subsetter",
    description = "Spatially and temporally subsets above ground biomass density from multiple GEDI 4A granules with additional filters (s-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary", "arrow"},
    schema = {
        tags = "p-series, gedi",
        request = [[ "application/json": {
            "schema": {
                "$ref": "../components/schemas/GediParameters.json"
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "allOf": [
                    { "$ref": "../components/schemas/gedi04arec.json" },
                    { "$ref": "../components/schemas/gedi04arec.footprint.json" }
                ],
                "description": "Stream of binary-encoded GEDI 4A footprints"
            }
        } ]]
    }
}