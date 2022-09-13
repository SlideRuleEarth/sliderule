--
-- ENDPOINT:    /source/elevation
--
-- PURPOSE:     return an elevation from the ArcticDEM given a lat and lon
--
-- INPUT:       rqst
--              {
--                  "dem-asset":    "<name of asset to use>"
--                  "coordinates": [
--                      {
--                         "latitude":   <lat>
--                         "longitude":  <lon>
--                      },
--                      {
--                         "latitude":   <lat>
--                         "longitude":  <lon>
--                      }...
--                  ]
--              }
--
-- OUTPUT:      elevation
--

local json = require("json")

-- Request Parameters --
local rqst = json.decode(arg[1])
local dem_asset = rqst["dem-asset"] or "arcticdem-local"
local coord = rqst["coordinates"]

local el, status, lat, lon
local elevations = {}


-- Get Elevation --
local dem = arcticdem.raster()


for i, position in ipairs(coord) do
    el, status = dem:elevation(position.longitude, position.latitude)
    table.insert(elevations, el)
end


-- Return Response
return json.encode({elevations=elevations})