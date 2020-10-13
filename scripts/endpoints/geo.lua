--
-- ENDPOINT:    /source/geo
--
-- INPUT:       arg[1]
--              {
--                  "asset": <asset>
--                  "pole": "north" | "south"
--                  "lat": <latitude, -90 to 90>
--                  "lon": <longitude, -180 to 180>
--                  "x": <x coordinate>
--                  "y": <y coordinate>
--                  "span":
--                  {
--                      "lat0": <latitude, -90 to 90>
--                      "lon0": <longitude, -180 to 180>
--                      "lat1": <latitude, -90 to 90>
--                      "lon1": <longitude, -180 to 180>
--                  }
--                  "span1": {..}
--                  "span2": {..}
--              }
--
-- OUTPUT:      {
--                  "intersect": true|false # performed against span1 and span2
--                  "combine":  # performed against span1 and span2
--                  {
--                      "lat0": <latitude, -90 to 90>
--                      "lon0": <longitude, -180 to 180>
--                      "lat1": <latitude, -90 to 90>
--                      "lon1": <longitude, -180 to 180>
--                  }
--                  "split":
--                  {
--                      "lspan": {..}
--                      "rspan": {..}
--                  }
--                  "lat": <latitude, -90 to 90> # x,y converted to spherical coordinates
--                  "lon": <longitude, -180 to 180> # x,y converted to spherical coordinates
--                  "x": <x coordinate> # lat,lon converted to polar coordinates
--                  "y": <y coordinate> # lat,lon converted to polar coordinates
--              }
--
-- NOTES:       1. Both the input and the output are json objects
--              2. Output values are only returned if the needed input values are supplied
--

local json = require("json")
local parm = json.decode(arg[1])

local asset = core.asset(parm["asset"])
local pole = parm["pole"]
local lat = parm["lat"]
local lon = parm["lon"]
local x = parm["x"]
local y = parm["y"]
local span = parm["span"]
local span1 = parm["span1"]
local span2 = parm["span2"]

local result = {}
local index = nil

-- Create Index --
if pole == "north" then pole = core.NORTH_POLAR
else pole = core.SOUTH_POLAR end
index = core.spatialindex(asset, pole)

-- Polar Conversion --
if lat and lon then
    result["x"], result["y"] = index:polar(lat, lon) 
end

-- Spherical Conversion --
if x and y then
    result["lat"], result["lon"] = index:sphere(x, y)
end

-- Split --
if span then
    result["split"] = {}
    result["split"]["lspan"], result["split"]["rspan"] = index:split(span)
end

-- Intersect --
if span1 and span2 then
    result["intersect"] = index:intersect(span1, span2)
end

-- Combine --
if span1 and span2 then
    result["combine"] = index:combine(span1, span2)
end

return json.encode(result)
