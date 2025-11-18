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

if _rqst["arg"] == "atl13" then
    if parms["refid"] then
        response = core.ams("GET", string.format("atl13?refid=%d", parms["refid"]))
    elseif parms["name"] then
        response = core.ams("GET", string.format("atl13?name=%s", string.gsub(parms["name"], " ", "%%20")))
    elseif parms["coord"]["lat"] or parms["coord"]["lon"] then
        response = core.ams("GET", string.format("atl13?lon=%f&lat=%f", parms["coord"]["lon"], parms["coord"]["lat"]))
    else
        response = json.encode({error="must supply at least one query parameter"})
    end
elseif _rqst["arg"] == "atl24" then
    if parms["granule"] then
        response = core.ams("GET", string.format("atl24/granule/%s", parms["granule"]))
    else
        local first_parm = true
        local request_str = ""
        for _,v in ipairs({"t0", "t1", "season", "photons0", "photons1", "meandepth0", "meandepth1", "mindepth0", "mindepth1", "maxdepth0", "maxdepth1", "poly"}) do
            if parms[v] then
                request_str = request_str .. string.format("%s%s=%s", first_parm and "" or "&", v, parms[v])
                first_parm = false
            end
        end
        if #request_str > 0 then
            response = core.ams("GET", string.format("atl24?%s", request_str))
        else
            response = json.encode({error="must supply at least one query parameter"})
        end
    end
else
    response = json.encode({error=string.format("unrecognized product: %s", _rqst["arg"])})
end

return response

