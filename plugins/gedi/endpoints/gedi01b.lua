--
-- ENDPOINT:    /source/gedi01b
--

local json          = require("json")
local georesource   = require("georesource")

local rqst          = json.decode(arg[1])
local resource      = rqst["resource"]
local parms         = rqst["parms"]

local args = {
    shard           = rqst["shard"] or 0, -- key space
    default_asset   = "gedil1b",
    result_q        = parms[geo.PARMS] and "result." .. resource .. "." .. rspq or rspq,
    result_rec      = "gedi01brec",
    index_field     = "footprint.shot_number",
    lon_field       = "footprint.longitude",
    lat_field       = "footprint.latitude",
    height_field    = "footprint.elevation_start"
}

local rqst_parms    = gedi.parms(parms)
local proc          = georesource.initialize(resource, parms, nil, args)

if proc then
    local reader    = gedi.gedi01b(proc.asset, resource, args.result_q, rqst_parms, true)
    local status    = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
