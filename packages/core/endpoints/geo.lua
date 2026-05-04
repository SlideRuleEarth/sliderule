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
    inputs = {"json"},
    outputs = {"json"},
    schema = {
        request = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "asset": {
                        "type": "string",
                        "description": "Name of the asset"
                    },
                    "pole": {
                        "type": "string",
                        "enum": ["north", "south"],
                        "description": "Polar projection to use"
                    },
                    "lat": {
                        "type": "number",
                        "description": "Latitude, -90 to 90"
                    },
                    "lon": {
                        "type": "number",
                        "description": "Longitude, -180 to 180"
                    },
                    "x": {
                        "type": "number",
                        "description": "X coordinate in projected space"
                    },
                    "y": {
                        "type": "number",
                        "description": "Y coordinate in projected space"
                    },
                    "span": {
                        "type": "object",
                        "description": "Bounding box for split operation",
                        "properties": {
                            "lat0": { "type": "number", "description": "Latitude of first corner, -90 to 90" },
                            "lon0": { "type": "number", "description": "Longitude of first corner, -180 to 180" },
                            "lat1": { "type": "number", "description": "Latitude of second corner, -90 to 90" },
                            "lon1": { "type": "number", "description": "Longitude of second corner, -180 to 180" }
                        }
                    },
                    "span1": {
                        "type": "object",
                        "description": "First bounding box for intersect/combine operations",
                        "properties": {
                            "lat0": { "type": "number" },
                            "lon0": { "type": "number" },
                            "lat1": { "type": "number" },
                            "lon1": { "type": "number" }
                        }
                    },
                    "span2": {
                        "type": "object",
                        "description": "Second bounding box for intersect/combine operations",
                        "properties": {
                            "lat0": { "type": "number" },
                            "lon0": { "type": "number" },
                            "lat1": { "type": "number" },
                            "lon1": { "type": "number" }
                        }
                    }
                }
            }
        }]],
        response = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "intersect": {
                        "type": "boolean",
                        "description": "Result of intersection test on span1 and span2"
                    },
                    "combine": {
                        "type": "object",
                        "description": "Combined bounding box of span1 and span2",
                        "properties": {
                            "lat0": { "type": "number", "description": "Latitude of first corner, -90 to 90" },
                            "lon0": { "type": "number", "description": "Longitude of first corner, -180 to 180" },
                            "lat1": { "type": "number", "description": "Latitude of second corner, -90 to 90" },
                            "lon1": { "type": "number", "description": "Longitude of second corner, -180 to 180" }
                        }
                    },
                    "split": {
                        "type": "object",
                        "description": "Result of splitting span into left and right halves",
                        "properties": {
                            "lspan": {
                                "type": "object",
                                "properties": {
                                    "lat0": { "type": "number" },
                                    "lon0": { "type": "number" },
                                    "lat1": { "type": "number" },
                                    "lon1": { "type": "number" }
                                }
                            },
                            "rspan": {
                                "type": "object",
                                "properties": {
                                    "lat0": { "type": "number" },
                                    "lon0": { "type": "number" },
                                    "lat1": { "type": "number" },
                                    "lon1": { "type": "number" }
                                }
                            }
                        }
                    },
                    "lat": {
                        "type": "number",
                        "description": "Latitude from x,y converted to spherical coordinates, -90 to 90"
                    },
                    "lon": {
                        "type": "number",
                        "description": "Longitude from x,y converted to spherical coordinates, -180 to 180"
                    },
                    "x": {
                        "type": "number",
                        "description": "X coordinate from lat,lon converted to projected coordinates"
                    },
                    "y": {
                        "type": "number",
                        "description": "Y coordinate from lat,lon converted to projected coordinates"
                    }
                }
            }
        }]]
    }
}
