--
-- ENDPOINT:    /source/atl24s
--
local json          = require("json")
local dataframe     = require("dataframe")
local earthdata     = require("earthdata")
local rqst          = json.decode(arg[1])
local parms         = bathy.parms(rqst["parms"], rqst["shard"], "icesat2", rqst["resource"])
local userlog       = msg.publish(rspq) -- create user log publisher (alerts)

if earthdata.query(parms, rspq, userlog) == earthdata.SUCCESS then

    local df = nil

    if parms["resource"] then
    else
        df = dataframe.proxy("atl24s", parms, rspq, userlog)
    end

    if df then
        dataframe.send(df, parms, rspq, userlog)
    end
end