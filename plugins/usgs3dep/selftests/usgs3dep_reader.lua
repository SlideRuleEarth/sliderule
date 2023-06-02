local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")
local _,td = runner.srcscript()

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

-- Setup --
local assets = asset.loaddir()

local _,td = runner.srcscript()
local geojsonfile = td.."../data/grand_mesa_1m_dem.geojson"
local f = io.open(geojsonfile, "r")
local contents = f:read("*all")
f:close()

-- Unit Test --

local  lon = -108.1
local  lat = 39.1
local  height = 2630.0

print(string.format("\n-------------------------------------------------\nusgs3dep 1meter DEM test URL filter\n-------------------------------------------------"))

local demType = "usgs3dep-1meter-dem"
local url = "USGS_one_meter_x23y434_CO_MesaCo_QL2_UTM12_2016"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents, substr = url }))

local sampleCnt = 0
local sampledFile = ""
local sampledValue = 0
local gpsTime = 0
local tbl, status = dem:sample(lon, lat, height)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat, height))
else
    local value, fname, time
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        time  = v["time"]

        sampleCnt = sampleCnt + 1
        if sampleCnt == 1 then
            sampledValue = value
            sampledFile = fname
            gpsTime = time
        end

        print(string.format("(%02d) value %10.6f, time %.2f, fname: %s", j, value, time, fname))
        runner.check(time ~= 0.0)
    end
end

runner.check(sampleCnt == 1)

local expectedFile = "/vsis3/prd-tnm/StagedProducts/Elevation/1m/Projects/CO_MesaCo_QL2_UTM12_2016/TIFF/USGS_one_meter_x23y434_CO_MesaCo_QL2_UTM12_2016.tif"
runner.check(sampledFile == expectedFile)

-- NOTE: to validate that the custom transform from SLIDERULE EPSG to 1m 3DEP raster pixels worked use the following command:
-- echo -108.1 39.1 2630.0 | PROJ_DEBUG=2 PROJ_NETWORK=ON cs2cs -r -s epsg:7912 epsg:6342+5703
-- at the end of output there should be a statement:
-- Using https://cdn.proj.org/us_noaa_g2018u0.tif
-- 4332449.53      231917.92  2646.53
--
-- Initial Z0 value is 2630.0 after transform it is Z1 2646.53
-- So Geoid shift is (Z0-Z1) = -16.526
-- Value at pixel in the raster must be adjusted by
-- RETURN_VAL = 2654.55810546875 + -16.526 =  2638.03214

local expected_value =  2638.03214 -- first sample from the group of samples
local expected_max = expected_value + 0.00001
local expected_min = expected_value - 0.00001

runner.check(sampledValue <= expected_max and sampledValue >= expected_min)
runner.check(gpsTime == 1289671725.0)

dem = nil

print(string.format("\n-------------------------------------------------\nusgs3dep 1meter DEM test NO filter\n-------------------------------------------------"))
sampleCnt = 0
sampledFile = ""
sampledValue = 0
gpsTime = 0
demType = "usgs3dep-1meter-dem"
dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0, catalog = contents }))
tbl, status = dem:sample(lon, lat, height)
if status ~= true then
    print(string.format("======> FAILED to read", lon, lat))
else
    local value, fname, time
    for j, v in ipairs(tbl) do
        value = v["value"]
        fname = v["file"]
        time  = v["time"]

        sampleCnt = sampleCnt + 1
        if sampleCnt == 1 then
            sampledValue = value
            sampledFile = fname
            gpsTime = time
        end

        print(string.format("(%02d) value %10.6f, time %.2f, fname: %s", j, value, time, fname))
        runner.check(time ~= 0.0)
    end
end

runner.check(sampleCnt == 4)

runner.check(sampledValue <= expected_max and sampledValue >= expected_min)
runner.check(gpsTime == 1289671725.0)

runner.check(sampledFile == expectedFile)


-- Report Results --

runner.report()

