--
-- ENDPOINT:    /source/gedi02a
--

local json          = require("json")
local georesource   = require("georesource")
local rqst          = json.decode(arg[1])
local parms         = gedi.parms(rqst["parms"], rqst["shard"], "gedi02a", rqst["resource"])

local args = {
    result_q        = (parms[geo.PARMS] and not parms:hasoutput()) and "result." .. parms["resource"] .. "." .. rspq or rspq,
    result_rec      = "gedi02arec",
}

local proc          = georesource.initialize(parms, nil, args)
if proc then
    local reader    = gedi.gedi02a(args.result_q, parms, true)
    local status    = georesource.waiton(parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
