--
-- ENDPOINT:    /source/atl24v
--
local json          = require("json")
local rqst          = json.decode(arg[1])
local parms         = icesat2.parms(rqst["parms"], "icesat2", rqst["resource"])
local reader        = bathy.viewer(parms)
local status        = reader:waiton(parms["timeout"] * 1000)

-- return counts
if status then
    return json.encode(reader:counts())
else
    return string.format("{\"message\":\"timeout reached <%d>\"}", parms["timeout"])
end
