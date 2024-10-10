--
-- ENDPOINT:    /source/atl06
--

local json          = require("json")
local georesource   = require("georesource")
local rqst          = json.decode(arg[1])
local parms         = icesat2.parms(rqst["parms"], rqst["shard"], "icesat2", rqst["resource"])

local args = {
    result_q        = (parms[geo.PARMS] and not parms:hasoutput()) and "result." .. parms["resource"] .. "." .. rspq or rspq,
    source_rec      = "atl03rec",
    result_rec      = "atl06rec",
}

local algo          = icesat2.atl06(args.result_q, parms)
local proc          = georesource.initialize(parms, algo, args)
if proc then
    local reader    = icesat2.atl03s(proc.source_q, parms, true)
    local status    = georesource.waiton(parms, algo, reader, proc.algo_disp, proc.sampler_disp, proc.userlog, true)
end
