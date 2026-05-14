-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json          = require("json")
local georesource   = require("georesource")
local rqst          = json.decode(arg[1])
local parms         = icesat2.parms13s(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local args = {
        result_q        = georesource.result_q_name(parms),
        result_rec      = "atl13srec",
    }
    local proc          = georesource.initialize(parms, nil, args)
    if proc then
        local reader    = icesat2.atl13s(args.result_q, parms, false)
        local status    = georesource.waiton(parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
    end
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL13 Subsetter",
    description = "Spatially and temporally subsets single ATL13 granule lake metrics with additional filters",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary", "arrow"},
    schema = {
        tags = "s-series, icesat2",
        request = [[ "application/json": {
            "schema": {
                "$ref": "../components/schemas/Atl13sParameters.json"
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "allOf": [
                    { "$ref": "../components/schemas/atl13srec.json" },
                    { "$ref": "../components/schemas/atl13srec.elevation.json" }
                ],
                "description": "Stream of binary-encoded ICESat-2 inland lake measurements (ATL13)"
            }
        } ]]
    }
}