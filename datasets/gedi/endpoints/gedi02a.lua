-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json          = require("json")
    local georesource   = require("georesource")
    local rqst          = json.decode(arg[1])
    local parms         = gedi.parms(rqst["parms"], rqst["key_space"], "gedil2a", rqst["resource"])

    local args = {
        result_q        = georesource.result_q_name(parms),
        result_rec      = "gedi02arec",
    }

    local proc          = georesource.initialize(parms, nil, args)
    if proc then
        local reader    = gedi.gedi02a(args.result_q, parms, true)
        local status    = georesource.waiton(parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
    end
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "GEDI 2A Subsetter",
    description = "Spatially and temporally subsets single GEDI 2A granule elevations with additional filters (s-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary", "arrow"}
}