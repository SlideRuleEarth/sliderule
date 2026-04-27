-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local parms = json.decode(arg[1])
local asset = core.getbyname(parms["asset"], true)

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

    -- get parameters
    local pole = parms["pole"]
    local lat = parms["lat"]
    local lon = parms["lon"]
    local x = parms["x"]
    local y = parms["y"]
    local span = parms["span"]
    local span1 = parms["span1"]
    local span2 = parms["span2"]

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
    parms = parms,
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
