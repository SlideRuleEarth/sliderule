-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    -- imports
    local json = require("json")
    local parm = json.decode(arg[1])

    -- get parameters
    local asset = core.getbyname(parm["asset"])
    local pole = parm["pole"]
    local lat = parm["lat"]
    local lon = parm["lon"]
    local x = parm["x"]
    local y = parm["y"]
    local span = parm["span"]
    local span1 = parm["span1"]
    local span2 = parm["span2"]

    -- initialize results
    local result = {}
    local index = nil

    -- create index
    if pole == "north" then pole = core.NORTH_POLAR
    else pole = core.SOUTH_POLAR end
    index = core.spatialindex(asset, pole)

    -- polar conversion
    if lat and lon then
        result["x"], result["y"] = index:project(lon, lat)
    end

    -- spherical Conversion
    if x and y then
        result["lon"], result["lat"] = index:sphere(x, y)
    end

    -- split
    if span then
        result["split"] = {}
        result["split"]["lspan"], result["split"]["rspan"] = index:split(span)
    end

    -- intersect
    if span1 and span2 then
        result["intersect"] = index:intersect(span1, span2)
    end

    -- combine
    if span1 and span2 then
        result["combine"] = index:combine(span1, span2)
    end

    -- results
    return json.encode(result)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "Geospatial Index Operation",
    description = "Perform intersection, combination, and split operations on geospatial indexes",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    outputs = {"json"}
}

-- INPUT
--  {
--      "asset": <asset>
--      "pole": "north" | "south"
--      "lat": <latitude, -90 to 90>
--      "lon": <longitude, -180 to 180>
--      "x": <x coordinate>
--      "y": <y coordinate>
--      "span":
--      {
--          "lat0": <latitude, -90 to 90>
--          "lon0": <longitude, -180 to 180>
--          "lat1": <latitude, -90 to 90>
--          "lon1": <longitude, -180 to 180>
--      }
--      "span1": {..}
--      "span2": {..}
--  }
--
-- OUTPUT
--  {
--      "intersect": true|false # performed against span1 and span2
--      "combine":  # performed against span1 and span2
--      {
--          "lat0": <latitude, -90 to 90>
--          "lon0": <longitude, -180 to 180>
--          "lat1": <latitude, -90 to 90>
--          "lon1": <longitude, -180 to 180>
--      }
--      "split":
--      {
--          "lspan": {..}
--          "rspan": {..}
--      }
--      "lat": <latitude, -90 to 90> # x,y converted to spherical coordinates
--      "lon": <longitude, -180 to 180> # x,y converted to spherical coordinates
--      "x": <x coordinate> # lat,lon converted to projected coordinates
--      "y": <y coordinate> # lat,lon converted to projected coordinates
--  }
