local runner = require("test_executive")
local asset = require("asset")
local json = require("json")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate()

-- Self Test --

local  sigma = 1.0e-9

-- Amazon forest
local  lon =    -62.0
local  lat =    -30.0
local  height =   0.0

print(string.format("\n-------------------------------------------------\nGEDI l3-elevation sample POI\n-------------------------------------------------"))

local expResults = {{110.244743347168, 1326585618, '/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_elev_lowestmode_mean_2019108_2022019_002_03.tif'}}
local demType = "gedil3-elevation"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
local starttime = time.latch();
local tbl, err = dem:sample(lon, lat, height)
local stoptime = time.latch();
runner.assert(err == 0)
runner.assert(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))



print(string.format("\n-------------------------------------------------\nGEDI l3-elevation-stddev sample POI\n-------------------------------------------------"))

local expResults = {{0.520213186741, 1326585618, '/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_elev_lowestmode_stddev_2019108_2022019_002_03.tif'}}
local demType = "gedil3-elevation-stddev"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
local starttime = time.latch();
local tbl, err = dem:sample(lon, lat, height)
local stoptime = time.latch();
runner.assert(err == 0)
runner.assert(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))


print(string.format("\n-------------------------------------------------\nGEDI l3-cannopy sample POI\n-------------------------------------------------"))

local expResults = {{3.698355197906, 1326585618, '/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_rh100_mean_2019108_2022019_002_03.tif'}}
local demType = "gedil3-canopy"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
local starttime = time.latch();
local tbl, err = dem:sample(lon, lat, height)
local stoptime = time.latch();
runner.assert(err == 0)
runner.assert(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))



print(string.format("\n-------------------------------------------------\nGEDI l3-cannopy-stddev sample POI\n-------------------------------------------------"))

local expResults = {{0.573970079422, 1326585618, '/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_rh100_stddev_2019108_2022019_002_03.tif'}}
local demType = "gedil3-canopy-stddev"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
local starttime = time.latch();
local tbl, err = dem:sample(lon, lat, height)
local stoptime = time.latch();
runner.assert(err == 0)
runner.assert(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))



print(string.format("\n-------------------------------------------------\nGEDI l3-counts sample POI\n-------------------------------------------------"))

local expResults = {{152, 1326585618, '/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L3_LandSurface_Metrics_V2/data/GEDI03_counts_2019108_2022019_002_03.tif'}}
local demType = "gedil3-counts"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
local starttime = time.latch();
local tbl, err = dem:sample(lon, lat, height)
local stoptime = time.latch();
runner.assert(err == 0)
runner.assert(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))



print(string.format("\n-------------------------------------------------\nGEDI l4b sample POI\n-------------------------------------------------"))

local expResults = {{ 0.000637468358, 1312070418, '/vsis3/ornl-cumulus-prod-protected/gedi/GEDI_L4B_Gridded_Biomass_V2_1/data/GEDI04_B_MW019MW223_02_002_02_R01000M_V2.tif'}}
local demType = "gedil4b"
local dem = geo.raster(geo.parms({ asset = demType, algorithm = "NearestNeighbour", radius = 0}))
local starttime = time.latch();
local tbl, err = dem:sample(lon, lat, height)
local stoptime = time.latch();
runner.assert(err == 0)
runner.assert(tbl ~= nil)
print(string.format("sample time: %f", stoptime - starttime))

local sampleCnt = 0
for i, v in ipairs(tbl) do
    local el = v["value"]
    local fname = v["file"]
    local time = v["time"]
    print(string.format("(%02d) %17.12f  time: %.2f  %s", i, el, time, fname))
    sampleCnt = sampleCnt + 1

    if tostring(el) ~= "nan" then
        runner.assert(math.abs(el - expResults[i][1]) < sigma)
    end
    runner.assert(time == expResults[i][2])
    runner.assert(fname == expResults[i][3])
end
runner.assert(sampleCnt == #expResults, string.format("Received unexpected number of samples: %d instead of %d", sampleCnt, #expResults))

-- Report Results --

runner.report()

