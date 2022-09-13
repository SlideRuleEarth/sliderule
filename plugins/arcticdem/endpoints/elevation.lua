--
-- ENDPOINT:    /source/elevation
--
-- PURPOSE:     return an elevation from the ArcticDEM given a lat and lon
--
-- INPUT:       rqst
--              {
--                  "dem-asset":    "<name of asset to use>"
--                  "lat":          <lat>
--                  "lon":          <lon>
--              }
--
-- OUTPUT:      elevation
--

local json = require("json")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local dem_asset = rqst["dem-asset"] or "arcticdem-local"
local lat = rqst["lat"]
local lon = rqst["lon"]

-- Get Asset --
--local asset = core.getbyname(dem_asset)
--if not asset then
--    userlog:sendlog(core.ERROR, string.format("invalid asset specified: %s", dem_asset))
--    return
--end

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("request arcticdem elevation for %f,%f ...", lat, lon))

-- Get Elevation --
local elevation = 0.0

-- Return Response
return json.encode({elevation=elevation})