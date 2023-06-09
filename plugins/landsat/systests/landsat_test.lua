local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --

local assets = asset.loaddir()
local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms)):name("LpdaacAuthScript")
sys.wait(5)


local _,td = runner.srcscript()
local geojsonfile = td.."../data/hls_trimmed.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Unit Test --

local  lon = -179.0
local  lat = 51.0
local  height = 0

print(string.format("\n-------------------------------------------------\nLandsat Plugin test\n-------------------------------------------------"))

local demType = "landsat-hls"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, bands = {"B03", "NDVI"}, catalog = contents }))

for i = 1, 1, 1 do
    local tbl, status = dem:sample(lon, lat, height)
    if status ~= true then
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
