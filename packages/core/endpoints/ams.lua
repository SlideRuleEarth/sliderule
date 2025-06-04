--[[
NAME:       Asset Metadata Service
ENDPOINT:   /source/ams
INPUT:      {
                "refid": <atl13 reference id>,
                "coord": {
                    "lat": <containing latitude>,
                    "lon": <containing longitude>
                },
                "name": <lake name>
            }
OUTPUT:     json string of AMS output
--]]

local json = require("json")
local parms = json.decode(arg[1])

local response = nil
if parms["refid"] then
    response = core.manager("GET", string.format("/manager/ams/atl13?refid=%d", parms["refid"]))
elseif parms["name"] then
    response = core.manager("GET", string.format("/manager/ams/atl13?name=%s", string.gsub(parms["name"], " ", "%%20")))
elseif parms["coord"]["lat"] or parms["coord"]["lon"] then
    response = core.manager("GET", string.format("/manager/ams/atl13?lon=%f&lat=%f", parms["coord"]["lon"], parms["coord"]["lat"]))
end

if response then
    return response
end

