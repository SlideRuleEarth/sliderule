-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json          = require("json")
local georesource   = require("georesource")
local rqst          = json.decode(arg[1])
local parms         = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local args = {
        result_q        = georesource.result_q_name(parms),
        source_rec      = "atl03rec",
        result_rec      = "atl08rec",
    }
    local algo          = icesat2.atl08(args.result_q, parms)
    local proc          = georesource.initialize(parms, algo, args)
    if proc then
        local reader    = icesat2.atl03s(proc.source_q, parms, true)
        local status    = georesource.waiton(parms, algo, reader, proc.algo_disp, proc.sampler_disp, proc.userlog, false)
    end
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL08 PhoReal Vegetation Metrics",
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