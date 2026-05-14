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
    proxy.proxy(resources, parms, "atl06s", "atl06srec")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL06 Parallel Subsetter",
    description = "Spatially and temporally subsets ATL06 granules elevations with additional filters",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary", "arrow"},
    schema = {
        tags = "s-series, icesat2",
        request = [[ "application/json": {
            "schema": {
                "$ref": "../components/schemas/Atl06Parameters.json"
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "allOf": [
                    { "$ref": "../components/schemas/atl06srec.json" },
                    { "$ref": "../components/schemas/atl06srec.elevation.json" }
                ],
                "description": "Stream of binary-encoded ICESat-2 surface elevations (ATL06)"
            }
        } ]]
    }
}