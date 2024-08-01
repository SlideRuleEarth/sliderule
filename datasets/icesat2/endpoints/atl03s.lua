--
-- ENDPOINT:    /source/atl03s
--

local json          = require("json")
local georesource   = require("georesource")

local rqst          = json.decode(arg[1])
local resource      = rqst["resource"]
local parms         = rqst["parms"]

local args = {
    shard           = rqst["shard"] or 0, -- key space
    default_asset   = "icesat2",
    result_q        = parms[geo.PARMS] and "result." .. resource .. "." .. rspq or rspq,
    result_rec      = "atl03rec",
}

local rqst_parms    = icesat2.parms(parms)
local proc          = georesource.initialize(resource, parms, nil, args)

if proc then
    local reader    = icesat2.atl03s(proc.asset, resource, args.result_q, rqst_parms, false)
    local status    = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
