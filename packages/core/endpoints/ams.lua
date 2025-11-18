--[[
NAME:       Asset Metadata Service
ENDPOINT:   /source/ams/atl13
            {
                "refid": <atl13 reference id>,
                "coord": {
                    "lat": <containing latitude>,
                    "lon": <containing longitude>
                },
                "name": <lake name>
            }
ENDPOINT:   /source/ams/atl24
            {
                "t0": <start time, e.g. 2018-10-01>,
                "t1": <stop time, e.g. 2018-10-02>,
                "season": <0: winter, 1: spring, 2: summer, 3: fall>,
                "photons0": <minimum number of bathy photons>,
                "photons1": <maximum number of bathy photons>,
                "meandepth0": <minimum mean depth of bathy photons>,
                "meandepth1": <maximum mean depth of bathy photons>,
                "mindepth0": <minimum min depth of bathy photons>,
                "mindepth1": <maximum min depth of bathy photons>,
                "maxdepth0": <minimum max depth of bathy photons>,
                "maxdepth1": <maximum max depth of bathy photons>,
                "poly": <polygon: lon1 lat1 lon2 lat2 ... lonN latN>,
                "granule": <ATL24 granule name>
            }
OUTPUT:     json string of AMS output
--]]

local json = require("json")
local parms = json.decode(arg[1])

local response = nil
if parms["refid"] then
    response = core.ams("GET", string.format("atl13?refid=%d", parms["refid"]))
elseif parms["name"] then
    response = core.ams("GET", string.format("atl13?name=%s", string.gsub(parms["name"], " ", "%%20")))
elseif parms["coord"]["lat"] or parms["coord"]["lon"] then
    response = core.ams("GET", string.format("atl13?lon=%f&lat=%f", parms["coord"]["lon"], parms["coord"]["lat"]))
end

if response then
    return response
end

