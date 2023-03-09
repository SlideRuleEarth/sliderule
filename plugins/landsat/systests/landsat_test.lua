local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --

local assets = asset.loaddir() -- looks for asset_directory.csv in same directory this script is located in
local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", asset="landsat-hls"}
local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms)):name("LpdaacAuthScript")
sys.wait(5)

-- Unit Test --

local  lon = -179.0
local  lat = 51.0

print(string.format("\n-------------------------------------------------\nLandsat Plugin test\n-------------------------------------------------"))

local demType = "landsat-hls"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0 }))

for i = 1, 1, 1 do
    local tbl, status = dem:sample(lon, lat)
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
