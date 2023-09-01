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
    default_asset   = "swot-sim-ecco-llc4320",
    result_q        = parms[geo.PARMS] and "result." .. resource .. "." .. rspq or rspq,
    result_rec      = "swotl2geo",
    index_field     = "scan.scan_id",
    lon_field       = "scan.longitude",
    lat_field       = "scan.latitude"
}

local rqst_parms    = swot.parms(parms)
local proc          = georesource.initialize(resource, parms, nil, args)

if proc then
    local reader    = swot.swotl2(proc.asset, resource, args.result_q, rqst_parms, true)
    local status    = georesource.waiton(resource, parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
