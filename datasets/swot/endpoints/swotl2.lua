--
-- ENDPOINT:    /source/gedi01b
--

local json          = require("json")
local georesource   = require("georesource")
local rqst          = json.decode(arg[1])
local parms         = swot.parms(rqst["parms"], rqst["key_space"], "swot-sim-ecco-llc4320", rqst["resource"])

local args = {
    result_q        = georesource.result_q_name(parms),
    result_rec      = "swotl2geo",
}

local proc          = georesource.initialize(parms, nil, args)
if proc then
    local reader    = swot.swotl2(args.result_q, parms, true)
    local status    = georesource.waiton(parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
