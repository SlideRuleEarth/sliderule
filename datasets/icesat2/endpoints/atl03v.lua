--
-- ENDPOINT:    /source/atl03v
--

local json          = require("json")
local georesource   = require("georesource")
local rqst          = json.decode(arg[1])
local parms         = icesat2.parms(rqst["parms"], rqst["shard"], "icesat2", rqst["resource"])

local args = {
    result_q        = (parms[geo.PARMS] and not parms:hasoutput()) and "result." .. parms["resource"] .. "." .. rspq or rspq,
    result_rec      = "atl03vrec",
}

local proc          = georesource.initialize(parms, nil, args)
if proc then
    local reader    = icesat2.atl03v(args.result_q, parms, false)
    local status    = georesource.waiton(parms, nil, reader, nil, proc.sampler_disp, proc.userlog, true)
end
