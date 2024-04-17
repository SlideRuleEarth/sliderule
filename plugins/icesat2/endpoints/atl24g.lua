--
-- ENDPOINT:    /source/atl24g
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
    result_rec      = "bathyrec",
}

local rqst_parms    = icesat2.bathyparms(parms)
local proc          = georesource.initialize(resource, parms, nil, args)

if proc then
    local year      = resource:sub(7,10)
    local month     = resource:sub(11,12)
    local day       = resource:sub(13,14)
    local rdate     = string.format("%04d-%02d-%02dT00:00:00Z", year, month, day)
    local rgps      = time.gmt2gps(rdate)
    local rdelta    = 15 * 24 * 60 * 60 * 1000 -- 15 days * 24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second
    local t0        = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta))
    local t1        = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta))
    local hls_parms = {ndwi = {asset = "landsat-hls", t0 = t0, t1 = t1, use_poi_time = true, bands = {"NDWI"}}}
    local hls       = geo.raster(geo.parms(hls_parms))
    local reader    = icesat2.atl03bathy(proc.asset, resource, args.result_q, rqst_parms, hls, false)
    local status    = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, false)
end
