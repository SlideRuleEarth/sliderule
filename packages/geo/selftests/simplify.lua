local runner = require("test_executive")
local time = require("time")
local srcfile, dirpath = runner.srcscript()

-- Load GeoJSON file
local geojsonfile = dirpath.."../data/grandmesa.geojson"
local f = io.open(geojsonfile, "r")
runner.assert(f ~= nil, "failed to open geojson file", true)
local vectorfile = f:read("*a")
f:close()

-- Helper to extract exterior ring as { {lat=, lon=} ... }
local function geojson_to_poly(geojson)
    local json = require("json")
    local obj = json.decode(geojson)
    runner.assert(obj ~= nil, "failed to decode geojson")
    local coords = obj.features[1].geometry.coordinates[1]
    local poly = {}
    for i, pt in ipairs(coords) do
        local lon = pt[1]
        local lat = pt[2]
        table.insert(poly, {lat=lat, lon=lon})
    end
    return poly
end

-- Generate a simple, non-self-intersecting random polygon with N vertices
local function random_polygon(num_vertices, center_lat, center_lon, radius_deg)
    runner.assert(num_vertices >= 3, "num_vertices must be >= 3")
    center_lat = center_lat or 0.0
    center_lon = center_lon or 0.0
    radius_deg = radius_deg or 0.1

    local angles = {}
    for i = 1, num_vertices do
        angles[i] = math.random() * 2.0 * math.pi
    end
    table.sort(angles)

    local poly = {}
    for _, a in ipairs(angles) do
        local r = radius_deg * (0.5 + math.random()) -- jitter radius to avoid perfect circle
        local lat = center_lat + r * math.sin(a)
        local lon = center_lon + r * math.cos(a)
        table.insert(poly, {lat = lat, lon = lon})
    end
    return poly
end

-- Build polygon
local poly = geojson_to_poly(vectorfile)

-- One simplify call for now
local buffer_distance = 0.002   -- ~220 meters
local simplify_tolerance = 0.01 -- ~1.1 km

print("\n------------------\ngeo.simplify Test\n------------------")
local simplified = geo.simplify(poly, buffer_distance, simplify_tolerance)
runner.assert(simplified ~= nil, "simplify returned nil")
runner.assert(#simplified > 2, "simplified polygon too small")
print(string.format("original vertices: %d", #poly))
print(string.format("simplified vertices: %d (buffer=%.3f, simplify=%.3f)", #simplified, buffer_distance, simplify_tolerance))

print("\n------------------\nRandom Polygon Stress\n------------------")
local stress_cases = {100, 1000, 10000}
for _, n in ipairs(stress_cases) do
    local rp = random_polygon(n, 0.0, 0.0, 0.1)
    local t0 = time.latch()
    local rs = geo.simplify(rp, buffer_distance, simplify_tolerance)
    local t1 = time.latch()
    runner.assert(rs ~= nil, string.format("simplify returned nil for %d verts", n))
    runner.assert(#rs > 2, string.format("simplified polygon too small for %d verts", n))
    print(string.format("N=%5d : before=%5d, after=%5d, time=%.4fs", n, #rp, #rs, t1 - t0))
end

runner.report()
