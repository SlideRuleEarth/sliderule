--
-- ENDPOINT:    /source/atl24s
--
local json          = require("json")
local dataframe     = require("dataframe")
local earthdata     = require("earthdata")
local rqst          = json.decode(arg[1])
local parms         = bathy.parms(rqst["parms"], rqst["shard"], "icesat2", rqst["resource"])
local userlog       = msg.publish(rspq) -- create user log publisher (alerts)
local status        = earthdata.query(parms, rspq, userlog)


if status == earthdata.SINGLETON then
    print(parms["resource"])
else
    local proxy = dataframe.proxy(parms, "atl24s", userlog)
end