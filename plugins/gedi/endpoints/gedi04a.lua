--
-- ENDPOINT:    /source/gedi04a
--

local json          = require("json")
local georesource   = require("georesource")

local rqst          = json.decode(arg[1])
local resource      = rqst["resource"]
local parms         = rqst["parms"]

local args = {
    shard           = rqst["shard"] or 0, -- key space
    default_asset   = "gedi04a",
    result_q        = (parms[geo.PARMS] and not parms[arrow.PARMS]) and "result." .. resource .. "." .. rspq or rspq,
    result_rec      = "gedi04arec",
}

local rqst_parms    = gedi.parms(parms)
local proc          = georesource.initialize(resource, parms, nil, args)

if proc then
    local reader    = gedi.gedi04a(proc.asset, resource, args.result_q, rqst_parms, true)
    local status    = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
