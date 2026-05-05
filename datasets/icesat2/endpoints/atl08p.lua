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
    proxy.proxy(resources, parms, "atl08", "atl08rec")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL08 PhoReal Vegetation Metrics Parallel",
    description = "Generates ATL08-like vegetation metrics using user supplied processing parameters (p-series)",
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
                    { "$ref": "#/components/schemas/atl08rec" },
                    { "$ref": "#/components/schemas/atl08rec.vegetation" }
                ],
                "description": "Stream of binary-encoded calculated vegetation metrics (PhoREAL)"
            }
        } ]]
    }
}