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
    proxy.proxy(resources, parms, "atl03v", "atl03vrec")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL03 Parallel Viewer",
    description = "Spatially and temporally subsets segments from multiple ATL03 granules with additional filters (s-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary", "arrow"},
    schema = {
        request = [[ "application/json": {
            "schema": {
                "$ref": "#/components/schemas/Icesat2Parameters"
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "allOf": [
                    { "$ref": "#/components/schemas/atl03vrec" },
                    { "$ref": "#/components/schemas/atl03vrec.segments" }
                ],
                "description": "Stream of binary-encoded ICESat-2 segment-level data"
            }
        } ]]
    }
}
