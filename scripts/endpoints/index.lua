--
-- ENDPOINT:    /source/index
--
-- INPUT:       arg[1]
--              {
--                  "<index name>": { <index parameters>... } 
--              }
--
-- OUTPUT:      {
--                  "resources": ["<resource name>", ...]
--              }
--
-- NOTES:       1. Both the input and the output are json objects
--              2. The index parameters are specific to the index being requested
--
--

local json = require("json")
local parm = json.decode(arg[1])

local asset = core.getbyname(parm["asset"])
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
