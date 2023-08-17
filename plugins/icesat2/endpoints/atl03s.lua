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
    result_batch    = "atl03rec.photons",
    index_field     = "time",
    lon_field       = "longitude",
    lat_field       = "latitude",
    height_field    = "height"
}

local rqst_parms    = icesat2.parms(parms)
local proc          = georesource.initialize(resource, parms, nil, args)

if proc then
    if parms[arrow.PARMS] then
        local output_parms = arrow.parms(parms[arrow.PARMS])
    end

    local reader    = icesat2.atl03(proc.asset, resource, args.result_q, rqst_parms, false)
    local status    = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
