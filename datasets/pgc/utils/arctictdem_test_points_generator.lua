local M = {}

-- Fixed region: bounding box around ArcticDEM test area from file ../data/arcticdem_strips.geojson
local min_lon = -150.0
local max_lon = -149.0
local min_lat =  70.0
local max_lat =  71.0

-- Deterministic pseudo-random number generator (LCG)
local function srand(seed)
    local a = 1664525
    local c = 1013904223
    local m = 2^32
    local state = seed or 1
    return function()
        state = (a * state + c) % m
        return state / m
    end
end

-- Generate deterministic pseudo-random points in bounding box
function M.generate_points(maxPoints)
    local lons = {}
    local lats = {}
    local heights = {}

    local rand = srand(maxPoints)  -- seed with maxPoints for repeatability

    for _ = 1, maxPoints do
        local lon = min_lon + (max_lon - min_lon) * rand()
        local lat = min_lat + (max_lat - min_lat) * rand()

        table.insert(lons, lon)
        table.insert(lats, lat)
        table.insert(heights, 0)
    end

    return lons, lats, heights
end

return M
