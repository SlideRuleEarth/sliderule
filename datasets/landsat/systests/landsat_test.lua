local runner = require("test_executive")

-- Setup --

runner.authenticate()

local _,td = runner.srcscript()
local geojsonfile = td.."../data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Self Test --

local  lon = -179.0
local  lat = 51.0
local  height = 0

print(string.format("\n-------------------------------------------------\nLandsat Plugin test\n-------------------------------------------------"))

local demType = "landsat-hls"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"B03", "NDVI"}, catalog = contents }))

for i = 1, 1, 1 do
    local tbl, err = dem:sample(lon, lat, height)
    if err ~= 0 then
        print(string.format("======> FAILED to read", lon, lat))
    else
        local el, fname
        for j, v in ipairs(tbl) do
            el = v["value"]
            fname = v["file"]
            print(string.format("%15f, fname: %s", el, fname))
        end
        -- print(string.format("%15f, fname: %s", el, fname))
    end

    lon = lon + 0.1
    -- lat = lat + 0.1
end

os.exit()
