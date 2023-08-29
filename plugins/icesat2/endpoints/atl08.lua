--
-- ENDPOINT:    /source/atl08
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
    source_rec      = "atl03rec",
    result_rec      = "atl08rec",
    result_batch    = "atl08rec.vegetation",
    index_field     = "extent_id",
    lon_field       = "longitude",
    lat_field       = "latitude",
    height_field    = "h_te_median"
}

local rqst_parms    = icesat2.parms(parms)
local algo          = icesat2.atl08(args.result_q, rqst_parms)
local proc          = georesource.initialize(resource, parms, algo, args)

if proc then
    local reader    = icesat2.atl03(proc.asset, resource, proc.source_q, rqst_parms, true)
    local status    = georesource.waiton(resource, parms, algo, reader, proc.algo_disp, proc.sampler_disp, proc.userlog, false)
end
