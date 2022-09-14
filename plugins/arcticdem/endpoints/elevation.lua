--
-- ENDPOINT:    /source/elevation
--
-- PURPOSE:     return an elevation from the ArcticDEM given a lat and lon
--
-- INPUT:       rqst
--              {
--                  "dem-asset":    "<name of asset to use>"
--                  "coordinates": [
--                      [<latitude>, <longitude>],
--                      [<latitude>, <longitude>]...
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


for _, position in ipairs(coord) do
    lat = position[1]
    lon = position[2]
    el, status = dem:elevation(lon, lat)
    table.insert(elevations, el)
end


-- Return Response
return json.encode({elevations=elevations})