-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json          = require("json")
local georesource   = require("georesource")
local rqst          = json.decode(arg[1])
local parms         = icesat2.parms03(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])
local parmsd        = icesat2.parms06d(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local args = {
        result_q        = georesource.result_q_name(parms),
        source_rec      = "atl03rec",
        result_rec      = "atl06rec",
    }
    local algo          = icesat2.atl06(args.result_q, parmsd)
    local proc          = georesource.initialize(parms, algo, args)
    if proc then
        local reader    = icesat2.atl03s(proc.source_q, parms, true)
        local status    = georesource.waiton(parms, algo, reader, proc.algo_disp, proc.sampler_disp, proc.userlog, true)
    end
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL06-SR",
    description = "Generates ATL06 elevations using user supplied processing parameters",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary", "arrow"},
    schema = {
        tags = "p-series, icesat2",
        request = [[ "application/json": {
            "schema": {
                "$ref": "../components/schemas/Atl06DispatchParameters.json"
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "allOf": [
                    { "$ref": "../components/schemas/atl06rec.json" },
                    { "$ref": "../components/schemas/atl06rec.elevation.json" }
                ],
                "description": "Stream of binary-encoded calculated surface elevations"
            }
        } ]]
    }
}
