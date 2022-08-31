--
-- ENDPOINT:    /source/arcdem
--
-- PURPOSE:     subset and return digital elevations
--
-- INPUT:       rqst
--              {
--                  "dem-asset":    "<name of asset to use, defaults to atlas-local>"
--                  "resources":    ["<url of dem file or object>", ...]
--                  "parms":        {<table of parameters>}
--                  "timeout":      <milliseconds to wait for first response>
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      arctdem
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--

local json = require("json")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local dem_asset = rqst["atl03-asset"] or "nsidc-s3"
local resource = rqst["resource"]
local parms = rqst["parms"]
local timeout = rqst["timeout"] or core.PEND

-- Get Asset --
local asset = core.getbyname(dem_asset)
if not asset then
    userlog:sendlog(core.ERROR, string.format("invalid asset specified: %s", dem_asset))
    return
end

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("request <%s> arcticdem subsetting for %s ...", rspq, resource))

return
