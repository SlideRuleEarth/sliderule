--
-- ENDPOINT:    /source/gedi04a
--

local json          = require("json")
local georesource   = require("georesource")
local rqst          = json.decode(arg[1])
local parms         = gedi.parms(rqst["parms"], rqst["key_space"], "gedil4a", rqst["resource"])

local args = {
    result_q        = georesource.result_q_name(parms),
    result_rec      = "gedi04arec",
}

local proc          = georesource.initialize(parms, nil, args)
if proc then
    local reader    = gedi.gedi04a(args.result_q, parms, true)
    local status    = georesource.waiton(parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
