--
-- ENDPOINT:    /source/gedi01b
--

local json          = require("json")
local georesource   = require("georesource")
local rqst          = json.decode(arg[1])
local parms         = gedi.parms(rqst["parms"], rqst["shard"], "gedil1b", rqst["resource"])

local args = {
    result_q        = (parms[geo.PARMS] and not parms:hasoutput()) and "result." .. parms["resource"] .. "." .. rspq or rspq,
    result_rec      = "gedi01brec",
}

local proc          = georesource.initialize(parms, nil, args)
if proc then
    local reader    = gedi.gedi01b(args.result_q, parms, true)
    local status    = georesource.waiton(parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
